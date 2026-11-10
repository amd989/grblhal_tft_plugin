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

#define CMD_CHAINED     0x01    // wait for idle after the previous command
#define CMD_PROBE       0x02    // clear the probe result before sending
#define CMD_NEEDS_PROBE 0x04    // drop the chain unless the last probe succeeded

typedef struct {
    uint32_t queued_at;     // not handed to grblHAL before this time
    uint8_t flags;
    char line[TFT_CMD_MAX_LEN];
} tft_cmd_t;

static QueueHandle_t cmd_queue = NULL;
static volatile bool drop_chain = false;
static bool probe_done = true;          // the last probe sent has completed
static on_probe_completed_ptr on_probe_completed;
static on_execute_realtime_ptr on_execute_realtime;

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
    }

    tft_sd_poll();

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
        } else {
            if(!chained)
                drop_chain = false;     // a new chain starts
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

static void queue_command (const char *cmd, uint32_t delay_ms, uint8_t flags)
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
        bool queued = xQueueSend(cmd_queue, &c, 0) == pdTRUE;
        tft_log("cmd %s: %s", queued ? "queued" : "DROPPED", c.line);
    }
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
    const float plate = TFT_PROBE_THICKNESS;

    queue_fmt(CMD_PROBE, "G21 G90 G38.2 Z%.3f F%d", work_z - TFT_PROBE_DISTANCE, TFT_PROBE_FEED);
    queue_fmt(CMD_CHAINED|CMD_NEEDS_PROBE, "G10 L20 P0 Z%.3f", plate);
    queue_fmt(CMD_CHAINED|CMD_NEEDS_PROBE, "G0 Z%.3f", plate + 2.0f);
    queue_fmt(CMD_CHAINED|CMD_PROBE, "G38.2 Z%.3f F%d", plate - 3.0f, TFT_PROBE_FEED_SLOW);
    queue_fmt(CMD_CHAINED|CMD_NEEDS_PROBE, "G10 L20 P0 Z%.3f", plate);
    queue_fmt(CMD_CHAINED|CMD_NEEDS_PROBE, "G0 Z%.3f", plate + TFT_PROBE_RETRACT);
}

void tft_spindle(bool on) {
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

void tft_stop(void) {
    drop_chain = true;
    send_rt(CMD_STOP);
}

void tft_reset(void) {
    drop_chain = true;
    send_rt(CMD_RESET);
}

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
