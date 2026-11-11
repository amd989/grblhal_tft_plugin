/*
 * tft_interface.c - Communication interface between TFT UI and grblHAL
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 */

#include "driver.h"

#if TFT_ENABLE

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "grbl/hal.h"
#include "grbl/state_machine.h"
#include "grbl/system.h"
#include "grbl/settings.h"
#include "grbl/gcode.h"
#include "grbl/stepper.h"
#include "grbl/spindle_control.h"

#include "tft_interface.h"
#include "ui_state.h"
#include "tft_sd.h"
#include "tft_settings.h"

#if WIFI_ENABLE || ETHERNET_ENABLE
#include "networking/networking.h"
#define TFT_NET_STATUS 1
#else
#define TFT_NET_STATUS 0
#endif

// How often the UI state cache is refreshed from grblHAL, in ms.
#define TFT_POLL_INTERVAL_MS    100
#define TFT_CMD_QUEUE_LEN       8
// Drop a command grblHAL has not accepted by then (busy, or in Hold), so it
// does not run unexpectedly much later.
#define TFT_CMD_TIMEOUT_MS      2000

// A chain is a command followed by chained ones, e.g. the steps of a probe
// cycle. Chained commands wait, without the timeout, until the machine is
// idle again. The rest of a chain is dropped when a command of it expires, on
// an alarm, a reset or a stop, and after a failed probe.
#define TFT_CHAIN_TIMEOUT_MS    120000

// Command flags (CMD_*) and tool measurement steps (cmd_kind_t) are in
// tft_logic.h, which builds the lines.

typedef struct {
    uint32_t queued_at;     // not handed to grblHAL before this time
    uint8_t flags;
    uint8_t kind;           // cmd_kind_t
    char line[TFT_CMD_MAX_LEN];
} tft_cmd_t;

static QueueHandle_t cmd_queue = NULL;
static volatile bool drop_chain = false;
static bool probe_done = true;          // the last probe sent has completed
#if TOOLSETTER_ENABLE
static bool toolsetter_selected = false;
#endif
static on_probe_completed_ptr on_probe_completed;
static on_realtime_report_ptr on_realtime_report;
static volatile uint32_t host_report_at;
static volatile bool host_report_seen = false;
static on_execute_realtime_ptr on_execute_realtime;
static void laser_test_service(void);
static float laser_s(float pct);

#if TFT_DEBUG
#define TFT_LOG_QUEUE_LEN       16
#define TFT_LOG_MAX_LEN         64

typedef struct {
    char text[TFT_LOG_MAX_LEN];
} tft_log_t;

static QueueHandle_t log_queue = NULL;

// Queue a debug line from any task, printed later from grblHAL's loop.
void tft_log (const char *fmt, ...)
{
    tft_log_t msg;
    va_list args;

    if(!log_queue)
        return;

    va_start(args, fmt);
    vsnprintf(msg.text, sizeof(msg.text), fmt, args);
    va_end(args);

    xQueueSend(log_queue, &msg, 0);
}
#endif

#if TFT_NET_STATUS

// First network interface: Wi-Fi station, access point or Ethernet
static bool get_net_status (network_info_t *info, network_flags_t flags, void *data)
{
    char buf[sizeof(ui_state.net_status)];
    const char *name = info->is_ethernet ? "Ethernet" : info->wifi_mode == WiFiMode_AP ? "Wi-Fi AP" : "Wi-Fi";

    if(info->link_up && *info->status.ip)
        snprintf(buf, sizeof(buf), "%s %s", name, info->status.ip);
    else
        snprintf(buf, sizeof(buf), "%s not connected", name);

    // Short strings, a torn read in the UI task only garbles one refresh
    strcpy(ui_state.net_status, buf);
    strncpy(ui_state.hostname, info->status.hostname, sizeof(ui_state.hostname) - 1);

    return true;
}

#endif

#if TOOLSETTER_ENABLE

static float probe_mpos_z (void)
{
    return (float)sys.probe_position[Z_AXIS] / settings.axis[Z_AXIS].steps_per_mm;
}

// Build the line for a tool measurement step. False drops the chain.
static bool build_tls_line (tft_cmd_t *cmd)
{
    coord_system_data_t g59_3;
    float mpos[N_AXIS];

    if(!settings_read_coord_data(CoordinateSystem_G59_3, &g59_3))
        return false;

    system_convert_array_steps_to_mpos(mpos, sys.position);

    tft_tls_t t = {
        .z_home = sys.home_position[Z_AXIS],
        .g59_3 = { g59_3.coord.values[X_AXIS], g59_3.coord.values[Y_AXIS], g59_3.coord.values[Z_AXIS] },
        .probe_z = probe_mpos_z(),
        .z_offset = gc_get_offset(Z_AXIS, true),
        .tlo_ref_set = !!(sys.tlo_reference_set.mask & bit(Z_AXIS)),
        .tlo_delta = (float)(sys.probe_position[Z_AXIS] - sys.tlo_reference[Z_AXIS]) / settings.axis[Z_AXIS].steps_per_mm,
        .mpos = { mpos[X_AXIS], mpos[Y_AXIS], mpos[Z_AXIS] },
        .distance = TFT_PROBE_DISTANCE,
        .feed = TFT_PROBE_FEED,
        .feed_slow = TFT_PROBE_FEED_SLOW
    };

    return tft_logic_tls_line((cmd_kind_t)cmd->kind, &t, cmd->line, sizeof(cmd->line));
}

// Pick the probe input for the next G38: toolsetter or touch plate
static void select_probe (bool toolsetter)
{
    if(hal.probe.select && toolsetter != toolsetter_selected) {
        if(hal.probe.select(toolsetter ? Probe_Toolsetter : Probe_Default))
            toolsetter_selected = toolsetter;
    }
}

static void read_toolsetter_state (void)
{
    coord_system_data_t g59_3;

    ui_state.toolsetter_triggered = hal.probe.is_triggered && hal.probe.is_triggered(Probe_Toolsetter);
    ui_state.tlo_ref_set = !!(sys.tlo_reference_set.mask & bit(Z_AXIS));
    ui_state.tlo_z = gc_state.modal.tool_length_offset[Z_AXIS];

    if(settings_read_coord_data(CoordinateSystem_G59_3, &g59_3)) {
        ui_state.tls_pos_set = g59_3.coord.values[X_AXIS] != 0.0f || g59_3.coord.values[Y_AXIS] != 0.0f || g59_3.coord.values[Z_AXIS] != 0.0f;
        for(uint_fast8_t idx = 0; idx < 3 && idx < N_AXIS; idx++)
            ui_state.tls_pos[idx] = g59_3.coord.values[idx];
    }
}

#endif // TOOLSETTER_ENABLE

/*
 * Runs in grblHAL's foreground loop. This is the only place the plugin touches
 * grblHAL state, so the UI task never calls into grblHAL directly:
 * - snapshots state and position into the UI cache, without needing a host
 *   to poll with '?'
 * - hands queued UI commands to grbl.enqueue_gcode(), which only holds one
 *   line at a time and refuses while busy, so a command stays queued until
 *   it is accepted
 */
static void tft_poll (sys_state_t state)
{
    static uint32_t last_poll = 0;
    uint32_t now = hal.get_elapsed_ticks();

    if(now - last_poll >= TFT_POLL_INTERVAL_MS) {
        float mpos[N_AXIS], wpos[N_AXIS];

        last_poll = now;

        system_convert_array_steps_to_mpos(mpos, sys.position);
        for(uint_fast8_t idx = 0; idx < N_AXIS; idx++)
            wpos[idx] = mpos[idx] - gc_get_offset(idx, true);

        sys_state_t now_state = state_get();
        uint16_t alarm = (now_state & STATE_ALARM) ? (uint16_t)sys.alarm : 0;

#if TFT_DEBUG
        static sys_state_t last_state = (sys_state_t)-1;
        static uint16_t last_alarm = 0xFFFF;
        if(now_state != last_state || alarm != last_alarm) {
            tft_log("state 0x%X alarm %u", (unsigned)now_state, (unsigned)alarm);
            last_state = now_state;
            last_alarm = alarm;
        }
#endif

        ui_state_update_position(wpos, mpos);
        ui_state_update_system_state(now_state);
        ui_state_set_alarm(alarm);
        spindle_ptrs_t *spindle = spindle_get(0);
        struct spindle_param *sp = spindle ? spindle->param : NULL;

        ui_state_update_feed_spindle(st_get_realtime_rate(), sp && sp->state.on ? sp->rpm_overridden : 0.0f);
        ui_state_update_overrides(sys.override.feed_rate, sp ? sp->override_pct : 100);
        ui_state.probe_triggered = hal.probe.get_state && hal.probe.get_state().triggered;
        ui_state.z_homed = !!(sys.homed.mask & bit(Z_AXIS));
        ui_state.laser_mode = spindle && spindle->cap.laser;
        ui_state.spindle_max = spindle ? spindle->rpm_max : 0.0f;
        ui_state.tool = gc_state.tool ? gc_state.tool->tool_id : 0;
        ui_state.tool_pending = gc_state.tool_pending;
        ui_state.tc_mode = (uint8_t)settings.tool_change.mode;
        ui_state.host_connected = tft_logic_host_active(hal.stream.type == StreamType_Telnet || hal.stream.type == StreamType_WebSocket,
                                                        host_report_seen, host_report_at, hal.get_elapsed_ticks());

#if TOOLSETTER_ENABLE
        read_toolsetter_state();
#endif

#if TFT_NET_STATUS
        static uint_fast8_t net_count = 0;
        if(++net_count >= 10) {     // once a second
            net_count = 0;
            networking_enumerate_interfaces(get_net_status, NULL);
        }
#endif
    }

    tft_sd_poll();
    tft_settings_poll();
    laser_test_service();

    tft_cmd_t cmd;

    if(cmd_queue && xQueuePeek(cmd_queue, &cmd, 0) == pdTRUE && (int32_t)(now - cmd.queued_at) >= 0) {

        sys_state_t cur_state = state_get();
        bool chained = !!(cmd.flags & CMD_CHAINED);

        if(chained && (drop_chain || (cur_state & (STATE_ALARM|STATE_ESTOP)) || now - cmd.queued_at > TFT_CHAIN_TIMEOUT_MS)) {
            xQueueReceive(cmd_queue, &cmd, 0);
            drop_chain = true;
            tft_log("cmd dropped: %s", cmd.line);
        } else if(chained && (cur_state != STATE_IDLE || ((cmd.flags & CMD_NEEDS_PROBE) && !probe_done))) {
            // wait for the previous command to finish
        } else if((cmd.flags & CMD_NEEDS_PROBE) && !sys.flags.probe_succeeded) {
            xQueueReceive(cmd_queue, &cmd, 0);
            drop_chain = true;
            tft_log("cmd dropped, probe failed: %s", cmd.line);
        }
#if TOOLSETTER_ENABLE
        else if(cmd.kind != Cmd_Line && !build_tls_line(&cmd)) {
            xQueueReceive(cmd_queue, &cmd, 0);
            drop_chain = true;
            tft_log("cmd dropped, no G59.3");
        }
#endif
        else {
            if(!chained)
                drop_chain = false;     // a new chain starts
#if TOOLSETTER_ENABLE
            if(cmd.flags & CMD_PROBE)
                select_probe(!!(cmd.flags & CMD_TOOLSETTER));
#endif
            if(cmd.flags & CMD_PROBE)
                sys.flags.probe_succeeded = Off;
            if(grbl.enqueue_gcode(cmd.line)) {
                xQueueReceive(cmd_queue, &cmd, 0);
                if(cmd.flags & CMD_PROBE)
                    probe_done = false;
                tft_log("cmd accepted: %s", cmd.line);
            } else if(!chained && now - cmd.queued_at > TFT_CMD_TIMEOUT_MS) {
                xQueueReceive(cmd_queue, &cmd, 0);
                drop_chain = true;
                tft_log("cmd expired: %s", cmd.line);
            }
        }
    }

#if TOOLSETTER_ENABLE
    // Back to the touch plate once a tool measurement is over, so G38 from
    // a host or a job does not probe with the toolsetter
    if(toolsetter_selected && cmd_queue && uxQueueMessagesWaiting(cmd_queue) == 0 &&
        (state_get() == STATE_IDLE || (state_get() & STATE_ALARM)))
        select_probe(false);
#endif

#if TFT_DEBUG
    static bool announced = false;
    tft_log_t msg;

    if(!announced) {
        announced = true;
        hal.stream.write("[MSG:TFT poll running]" ASCII_EOL);
    }

    while(log_queue && xQueueReceive(log_queue, &msg, 0) == pdTRUE) {
        hal.stream.write("[MSG:TFT ");
        hal.stream.write(msg.text);
        hal.stream.write("]" ASCII_EOL);
    }
#endif

    on_execute_realtime(state);
}

// Called at the end of every probe cycle, failed ones too
static void tft_probe_completed (void)
{
    probe_done = true;

    if(on_probe_completed)
        on_probe_completed();
}

// Every status report is asked for by a host ('?'), unless reports are
// automatic ($481): note when, for the host badge
static void tft_realtime_report (stream_write_ptr stream_write, report_tracking_flags_t report)
{
    host_report_at = hal.get_elapsed_ticks();
    host_report_seen = true;

    if(on_realtime_report)
        on_realtime_report(stream_write, report);
}

void tft_interface_init (void)
{
    cmd_queue = xQueueCreate(TFT_CMD_QUEUE_LEN, sizeof(tft_cmd_t));
#if TFT_DEBUG
    log_queue = xQueueCreate(TFT_LOG_QUEUE_LEN, sizeof(tft_log_t));
#endif

    on_execute_realtime = grbl.on_execute_realtime;
    grbl.on_execute_realtime = tft_poll;

    on_probe_completed = grbl.on_probe_completed;
    grbl.on_probe_completed = tft_probe_completed;

    on_realtime_report = grbl.on_realtime_report;
    grbl.on_realtime_report = tft_realtime_report;
}

// Realtime commands (hold, resume, reset) are ISR safe in grblHAL.
static void send_rt (char c)
{
    bool ok = grbl.enqueue_realtime_command(c);
    tft_log("rt 0x%02X %s", (unsigned)(uint8_t)c, ok ? "sent" : "refused");
    (void)ok;
}

/*
 * Command Injection - safe to call from the UI task.
 * Lines are queued and passed to grblHAL from its own loop.
 */

static void queue_step (const char *cmd, uint32_t delay_ms, uint8_t flags, uint8_t kind)
{
    tft_cmd_t c;

    if(!cmd_queue || !cmd)
        return;

    strncpy(c.line, cmd, sizeof(c.line) - 1);
    c.line[sizeof(c.line) - 1] = '\0';

    // grblHAL takes a single line without terminator
    char *eol = strpbrk(c.line, "\r\n");
    if(eol)
        *eol = '\0';

    if(*c.line) {
        c.queued_at = hal.get_elapsed_ticks() + delay_ms;
        c.flags = flags;
        c.kind = kind;
        bool queued = xQueueSend(cmd_queue, &c, 0) == pdTRUE;
        tft_log("cmd %s: %s", queued ? "queued" : "DROPPED", c.line);
        (void)queued;
    }
}

static void queue_command (const char *cmd, uint32_t delay_ms, uint8_t flags)
{
    queue_step(cmd, delay_ms, flags, Cmd_Line);
}

static void queue_steps (const tft_step_t *step, size_t count)
{
    for(size_t idx = 0; idx < count; idx++)
        queue_command(step[idx].line, 0, step[idx].flags);
}

// Each line waits for the previous one to finish, see tft_interface.h
void tft_send_lines(const char *lines) {
    tft_step_t step[TFT_CMD_QUEUE_LEN];

    queue_steps(step, tft_logic_split_lines(lines, step, TFT_CMD_QUEUE_LEN));
}

void tft_send_command(const char *cmd) {
    queue_command(cmd, 0, 0);
}

void tft_send_command_delayed(const char *cmd, uint32_t delay_ms) {
    queue_command(cmd, delay_ms, 0);
}

void tft_send_command_fmt(const char *fmt, ...) {
    char buffer[TFT_CMD_MAX_LEN];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    tft_send_command(buffer);
}

/*
 * Motion Commands
 */

void tft_jog_axis(uint8_t axis, float distance, float speed) {
    if(axis >= N_AXIS)
        return;

    char axis_letter = 'X' + axis;
    tft_send_command_fmt("$J=G91 G21 %c%.3f F%.0f", axis_letter, distance, speed);
}

void tft_jog(float dx, float dy, float dz, float speed) {
    char line[TFT_CMD_MAX_LEN];

    if(tft_logic_jog_line(line, sizeof(line), dx, dy, dz, (int)speed))
        tft_send_command(line);
}

void tft_jog_cancel(void) {
    drop_chain = true;
    send_rt(CMD_JOG_CANCEL);
}

// Jog to work zero, cancellable like any jog
void tft_goto_zero_xy(void) {
    tft_send_command_fmt("$J=G90 G21 X0 Y0 F%d", TFT_JOG_FEED);
}

void tft_goto_zero_z(void) {
    tft_send_command_fmt("$J=G90 G21 Z0 F%d", TFT_JOG_FEED_Z);
}

static void queue_fmt (uint8_t flags, const char *fmt, ...)
{
    char buffer[TFT_CMD_MAX_LEN];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    queue_command(buffer, 0, flags);
}

/*
 * Probe Z with a touch plate on the work: a fast probe down from the current
 * position, then a slow one from 2 mm above. Z zero ends up on the work
 * surface (plate thickness taken off), and the tool TFT_PROBE_RETRACT above
 * the plate. Absolute moves only, so the distance mode is not left at G91.
 */
void tft_probe_z(float work_z) {
    const tft_probe_cfg_t cfg = {
        .plate = TFT_PROBE_THICKNESS,
        .distance = TFT_PROBE_DISTANCE,
        .feed = TFT_PROBE_FEED,
        .feed_slow = TFT_PROBE_FEED_SLOW,
        .retract = TFT_PROBE_RETRACT
    };
    tft_step_t step[6];

    queue_steps(step, tft_logic_probe_z(step, 6, work_z, &cfg));
}

#if TOOLSETTER_ENABLE

/*
 * Measure the tool on the toolsetter at G59.3: up to the Z home position,
 * over the setter, down to the G59.3 Z, then a fast and a slow probe. The
 * first measurement after homing sets the reference ($TLR), later ones set
 * the tool length offset from it (G43.1), so Z zero on the work stays valid
 * across tool changes. Ends back at the Z home position.
 * The lines here are only names for the log, the real ones are built when sent.
 */
void tft_tool_measure(void) {
    queue_step("safe Z", 0, 0, Cmd_TlsSafeZ);
    queue_step("setter XY", 0, CMD_CHAINED, Cmd_TlsXY);
    queue_step("setter Z", 0, CMD_CHAINED, Cmd_TlsZ);
    queue_step("probe fast", 0, CMD_CHAINED|CMD_PROBE|CMD_TOOLSETTER, Cmd_TlsProbeFast);
    queue_step("retract", 0, CMD_CHAINED|CMD_NEEDS_PROBE, Cmd_TlsRetract);
    queue_step("probe slow", 0, CMD_CHAINED|CMD_PROBE|CMD_TOOLSETTER, Cmd_TlsProbeSlow);
    queue_step("offset", 0, CMD_CHAINED|CMD_NEEDS_PROBE, Cmd_TlsOffset);
    queue_step("safe Z", 0, CMD_CHAINED|CMD_NEEDS_PROBE, Cmd_TlsSafeZ);
}

// Save the current machine position as the toolsetter position (G59.3)
void tft_toolsetter_set_here(void) {
    queue_step("set G59.3", 0, 0, Cmd_TlsSetHere);
}

#endif

void tft_frame(float min_x, float min_y, float max_x, float max_y) {
    // Laser: traced at low power, see tft_logic_frame()
    float s = ui_state.laser_mode && TFT_LASER_FRAME_POWER > 0.0f ? laser_s(TFT_LASER_FRAME_POWER) : 0.0f;
    tft_step_t step[6];

    queue_steps(step, tft_logic_frame(step, 6, min_x, min_y, max_x, max_y, TFT_JOG_FEED, s));
}

// S word for a laser power in percent, from $30
static float laser_s (float pct)
{
    return tft_logic_laser_s(ui_state.spindle_max, pct);
}

static volatile uint32_t laser_test_off_at;
static volatile bool laser_testing = false;

void tft_laser_test(bool on) {
    if(on && ui_state.laser_mode && !laser_testing) {
        // G1 without axis words selects a cutting motion mode, so M3 fires
        // while standing still. M3 in G0 would stay off.
        laser_test_off_at = hal.get_elapsed_ticks() + TFT_LASER_TEST_MAX_MS;
        laser_testing = true;
        queue_fmt(0, "G1 F%d M3 S%.1f", TFT_JOG_FEED, laser_s(TFT_LASER_TEST_POWER));
    } else if(!on && laser_testing) {
        laser_testing = false;
        tft_send_command("M5");
    }
}

// Called from the grblHAL loop: the laser test never outlasts its limit
static void laser_test_service (void)
{
    if(laser_testing && (int32_t)(hal.get_elapsed_ticks() - laser_test_off_at) >= 0)
        tft_laser_test(false);
}

void tft_spindle(bool on) {
    if(on && ui_state.laser_mode)
        return;     // M3 S<max> would fire the laser at full power

    if(on)
        tft_send_command_fmt("M3 S%d", TFT_SPINDLE_RPM);
    else
        tft_send_command("M5");
}

void tft_home_all(void) {
    tft_send_command("$H\n");
}

void tft_home_axis(uint8_t axis) {
    if(axis >= N_AXIS)
        return;

    char axis_letter = 'X' + axis;
    tft_send_command_fmt("$H%c\n", axis_letter);
}

/*
 * Work Coordinate Commands
 */

void tft_zero_axis(uint8_t axis) {
    if(axis >= N_AXIS)
        return;

    char axis_letter = 'X' + axis;
    tft_send_command_fmt("G10 L20 P0 %c0\n", axis_letter);
}

void tft_zero_axes(uint8_t mask) {
    char line[TFT_CMD_MAX_LEN];

    if(tft_logic_zero_line(line, sizeof(line), mask))
        tft_send_command(line);
}

void tft_zero_all(void) {
    // Build command for all active axes
    char cmd[64] = "G10 L20 P0";
    char *p = cmd + strlen(cmd);

    for(uint8_t axis = 0; axis < N_AXIS; axis++) {
        p += sprintf(p, " %c0", 'X' + axis);
    }
    *p++ = '\n';
    *p = '\0';

    tft_send_command(cmd);
}

void tft_set_axis_value(uint8_t axis, float value) {
    if(axis >= N_AXIS)
        return;

    char axis_letter = 'X' + axis;
    tft_send_command_fmt("G10 L20 P0 %c%.3f\n", axis_letter, value);
}

/*
 * Real-time Commands
 */

void tft_feed_hold(void) {
    send_rt(CMD_FEED_HOLD);
}

void tft_cycle_start(void) {
    send_rt(CMD_CYCLE_START);
}

/*
 * Resume after a hold, or after a tool change (M6). grblHAL only takes cycle
 * start in a tool change once the sender acknowledged it, so acknowledge
 * first. With $341=3 grblHAL then measures the new tool on the toolsetter.
 */
void tft_resume(void) {
    if(ui_state.system_state & STATE_TOOL_CHANGE)
        send_rt(CMD_TOOL_ACK);
    send_rt(CMD_CYCLE_START);
}

void tft_stop(void) {
    drop_chain = true;
    send_rt(CMD_STOP);
}

void tft_reset(void) {
    drop_chain = true;
    send_rt(CMD_RESET);
}

#if TFT_BEEP_ENABLE

static uint32_t beep_off_at;
static bool beeping = false;

void tft_beep(void) {
    if(!tft_settings_beep())
        return;

    DIGITAL_OUT(TFT_BEEP_PIN, 1);
    beep_off_at = hal.get_elapsed_ticks() + TFT_BEEP_MS;
    beeping = true;
}

void tft_beep_service(void) {
    if(beeping && (int32_t)(hal.get_elapsed_ticks() - beep_off_at) >= 0) {
        DIGITAL_OUT(TFT_BEEP_PIN, 0);
        beeping = false;
    }
}

#else

void tft_beep(void) {}
void tft_beep_service(void) {}

#endif

// dir > 0: +10%, dir < 0: -10%, 0: back to 100%
void tft_feed_override(int8_t dir) {
    send_rt(dir > 0 ? CMD_OVERRIDE_FEED_COARSE_PLUS : dir < 0 ? CMD_OVERRIDE_FEED_COARSE_MINUS : CMD_OVERRIDE_FEED_RESET);
}

void tft_spindle_override(int8_t dir) {
    send_rt(dir > 0 ? CMD_OVERRIDE_SPINDLE_COARSE_PLUS : dir < 0 ? CMD_OVERRIDE_SPINDLE_COARSE_MINUS : CMD_OVERRIDE_SPINDLE_RESET);
}

void tft_unlock(void) {
    // grblHAL refuses $X after a critical alarm (e.g. hard limit) until a
    // reset, so reset first. The machine is already stopped in alarm.
    if(ui_state.system_state & STATE_ALARM) {
        send_rt(CMD_RESET);
        // Let the reset finish first, it clears grblHAL's command buffer
        tft_send_command_delayed("$X", 300);
    } else
        tft_send_command("$X");
}

/*
 * State Queries
 */

sys_state_t tft_get_state(void) {
    return state_get();
}

alarm_code_t tft_get_alarm(void) {
    return sys.alarm;
}

uint8_t tft_get_error(void) {
    // grblHAL doesn't store last error code in sys structure
    // Return 0 for now - errors are reported via messages
    return 0;
}

bool tft_is_homing_enabled(void) {
    return settings.homing.flags.enabled;
}

bool tft_is_limits_enabled(void) {
    return settings.limits.flags.hard_enabled;
}

/*
 * Settings Commands
 */

float tft_get_setting(setting_id_t id) {
    const setting_detail_t *setting = setting_get_details(id, NULL);
    if(!setting)
        return 0.0f;

    // Get value based on type
    switch(setting->datatype) {
        case Format_Int8:
        case Format_Int16:
        case Format_Integer:
        case Format_Bool:
        case Format_Bitfield:
        case Format_XBitfield:
        case Format_AxisMask:
        case Format_RadioButtons:
            return (float)setting_get_int_value(setting, 0);

        case Format_Decimal:
            return setting_get_float_value(setting, 0);

        default:
            return 0.0f;
    }
}

bool tft_set_setting(setting_id_t id, float value) {
    // Format setting command
    tft_send_command_fmt("$%d=%.3f\n", (uint32_t)id, value);
    return true;
}

void tft_reset_settings(void) {
    tft_send_command("$RST=$\n");
}

#endif // TFT_ENABLE
