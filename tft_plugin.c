/*
 * tft_plugin.c - TFT touchscreen UI plugin for grblHAL
 *
 * Part of grblHAL
 *
 * Copyright (c) 2025
 *
 * This plugin provides a touchscreen user interface using:
 * - TFT_eSPI for display driver
 * - LVGL for graphics framework
 * - FreeRTOS task on Core 0 for UI updates
 *
 */

#include "driver.h"

#if TFT_ENABLE

#include <string.h>
#include "grbl/hal.h"
#include "grbl/nuts_bolts.h"
#include "grbl/state_machine.h"
#include "grbl/report.h"
#include "grbl/gcode.h"

#include "tft_plugin.h"

// Plugin metadata
static const char *plugin_id = "grblhal_tft_ui";
static const char *plugin_info = "TFT Touchscreen UI v0.1";

// Event handler chain (save previous handlers)
static on_state_change_ptr on_state_change;
static on_realtime_report_ptr on_realtime_report;
static on_program_completed_ptr on_program_completed;
static driver_reset_ptr driver_reset;
static on_report_options_ptr on_report_options;

// UI state cache
static struct {
    sys_state_t state;
    float mpos[N_AXIS];
    float wpos[N_AXIS];
    float feed_rate;
    uint32_t line_number;
} ui_state = {0};

// Forward declarations
static void tft_state_changed(sys_state_t state);
static void tft_realtime_report(stream_write_ptr stream_write, report_tracking_flags_t report);
static void tft_program_completed(program_flow_t program_flow, bool check_mode);
static void tft_reset(void);
static void tft_report_options(bool newopt);

/*
 * Event: State changed (Idle, Run, Hold, Alarm, etc.)
 */
static void tft_state_changed(sys_state_t state) {
    ui_state.state = state;

    // TODO: Update UI based on state
    // switch(state) {
    //     case STATE_IDLE:
    //         // Show ready screen
    //         break;
    //     case STATE_CYCLE:
    //         // Show job running screen
    //         break;
    //     case STATE_HOLD:
    //         // Show paused status
    //         break;
    //     case STATE_ALARM:
    //         // Show alarm screen
    //         break;
    //     case STATE_HOMING:
    //         // Show homing status
    //         break;
    // }

    // Chain to previous handler
    if(on_state_change)
        on_state_change(state);
}

/*
 * Event: Realtime report (position and status updates)
 */
static void tft_realtime_report(stream_write_ptr stream_write, report_tracking_flags_t report) {
    // Get current machine position
    system_convert_array_steps_to_mpos(ui_state.mpos, sys.position);

    // Get work position (accounting for work coordinate system)
    // In grblHAL, coordinate offsets are in gc_state.modal
    for(int i = 0; i < N_AXIS; i++) {
        ui_state.wpos[i] = ui_state.mpos[i] - gc_state.modal.coord_system.xyz[i];
    }

    // Get current feed rate
    ui_state.feed_rate = st_get_realtime_rate();

    // TODO: Update UI with position and feed rate
    // ui_update_position(ui_state.mpos, ui_state.wpos);
    // ui_update_feed_rate(ui_state.feed_rate);

    // Chain to previous handler
    if(on_realtime_report)
        on_realtime_report(stream_write, report);
}

/*
 * Event: Program completed
 */
static void tft_program_completed(program_flow_t program_flow, bool check_mode) {
    if(!check_mode) {
        // TODO: Show job complete popup
        // ui_show_popup("Job Complete", "Program finished successfully");
    }

    // Chain to previous handler
    if(on_program_completed)
        on_program_completed(program_flow, check_mode);
}

/*
 * Event: System reset
 */
static void tft_reset(void) {
    // Reset UI state
    memset(&ui_state, 0, sizeof(ui_state));

    // TODO: Reset UI to ready screen
    // ui_show_screen(SCREEN_READY);

    // Chain to previous handler
    if(driver_reset)
        driver_reset();
}

/*
 * Report: Add plugin info to $I command
 */
static void tft_report_options(bool newopt) {
    // Chain to previous handler first
    if(on_report_options)
        on_report_options(newopt);

    // Report this plugin
    if(!newopt)
        hal.stream.write("[PLUGIN:TFT UI v0.1]" ASCII_EOL);
}

/*
 * Plugin initialization
 * Called by grblHAL during startup
 */
void tft_plugin_init(void) {
    // TODO: Initialize TFT display hardware
    // tft_init();

    // TODO: Initialize LVGL graphics library
    // lvgl_init();

    // TODO: Create FreeRTOS UI task on Core 0
    // ui_task_create();

    // Hook into grblHAL event system
    on_state_change = grbl.on_state_change;
    grbl.on_state_change = tft_state_changed;

    on_realtime_report = grbl.on_realtime_report;
    grbl.on_realtime_report = tft_realtime_report;

    on_program_completed = grbl.on_program_completed;
    grbl.on_program_completed = tft_program_completed;

    driver_reset = hal.driver_reset;
    hal.driver_reset = tft_reset;

    on_report_options = grbl.on_report_options;
    grbl.on_report_options = tft_report_options;

    // Print initialization message
    hal.stream.write("[TFT Plugin initialized]" ASCII_EOL);
}

#endif // TFT_ENABLE
