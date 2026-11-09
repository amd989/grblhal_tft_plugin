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

#if defined(BOARD_MKS_DLC32_V2P0)
#error "TFT plugin: the stock MKS DLC32 map gives the LCD pins to grblHAL. Use BOARD_MY_MACHINE with tft/boards/mks_dlc32_2_0_tft_map.h, see the plugin README."
#endif

#include <string.h>
#include <lvgl.h>
#include "grbl/hal.h"
#include "grbl/nuts_bolts.h"
#include "grbl/state_machine.h"
#include "grbl/report.h"
#include "grbl/gcode.h"

#include "tft_plugin.h"
#include "tft_config.h"
#include "tft_driver.h"
#include "lvgl_init.h"
#include "ui_state.h"
#include "ui_screen_manager.h"
#include "ui_widgets.h"

// Plugin metadata
static const char *plugin_id = "grblhal_tft_ui";
static const char *plugin_info = "TFT Touchscreen UI v0.1";

// Event handler chain (save previous handlers)
static on_state_change_ptr on_state_change;
static on_realtime_report_ptr on_realtime_report;
static on_program_completed_ptr on_program_completed;
static driver_reset_ptr driver_reset;
static on_report_options_ptr on_report_options;

// Legacy UI state cache (for backward compatibility with existing code)
static struct {
    sys_state_t state;
    float mpos[N_AXIS];
    float wpos[N_AXIS];
    float feed_rate;
    uint32_t line_number;
} legacy_ui_state = {0};

// Forward declarations
static void tft_state_changed(sys_state_t state);
static void tft_realtime_report(stream_write_ptr stream_write, report_tracking_flags_t report);
static void tft_program_completed(program_flow_t program_flow, bool check_mode);
static void tft_reset(void);
static void tft_report_options(bool newopt);
static void tft_ui_task(void *param);

/*
 * Event: State changed (Idle, Run, Hold, Alarm, etc.)
 */
static void tft_state_changed(sys_state_t state) {
    legacy_ui_state.state = state;

    // Update foundation UI state
    ui_state_update_system_state(state);

    // Screen transitions based on state
    switch(state) {
        case STATE_IDLE:
            if(ui_state_get_screen() == SCREEN_JOB) {
                // Job completed, return to main screen
                ui_screen_manager_show(SCREEN_MAIN);
            }
            break;
        case STATE_CYCLE:
            // Auto-switch to job screen when running
            if(ui_state_get_screen() != SCREEN_JOB) {
                ui_screen_manager_show(SCREEN_JOB);
            }
            break;
        case STATE_ALARM:
            // Show alarm on current screen (status bar will show red)
            break;
        case STATE_HOMING:
            // Show homing status (status bar will update)
            break;
        default:
            break;
    }

    // Chain to previous handler
    if(on_state_change)
        on_state_change(state);
}

/*
 * Event: Realtime report (position and status updates)
 */
static void tft_realtime_report(stream_write_ptr stream_write, report_tracking_flags_t report) {
    // Get current machine position
    system_convert_array_steps_to_mpos(legacy_ui_state.mpos, sys.position);

    // Get work position (accounting for work coordinate system)
    // In grblHAL, coordinate offsets are in gc_state.modal
    for(int i = 0; i < N_AXIS; i++) {
        legacy_ui_state.wpos[i] = legacy_ui_state.mpos[i] - gc_state.modal.coord_system.xyz[i];
    }

    // Get current feed rate
    legacy_ui_state.feed_rate = st_get_realtime_rate();

    // Update foundation UI state with position data
    ui_state_update_position(legacy_ui_state.wpos, legacy_ui_state.mpos);
    ui_state_update_feed_spindle(legacy_ui_state.feed_rate, 0.0f);  // Spindle speed will be added later

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
    // Reset legacy UI state
    memset(&legacy_ui_state, 0, sizeof(legacy_ui_state));

    // Reset foundation UI state
    ui_state_init();

    // Return to main screen
    ui_screen_manager_show(SCREEN_MAIN);

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
 * UI Task - runs on Core 0, handles LVGL updates
 */
static void tft_ui_task(void *param) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(TFT_LVGL_REFRESH_MS);

    // Initialize foundation components
    ui_state_init();
    ui_screen_manager_init();
    ui_widgets_init();

    // Breathing effect during startup (2 seconds)
    uint32_t splash_count = 0;
    const uint32_t splash_duration = TFT_SPLASH_DURATION_MS / TFT_LVGL_REFRESH_MS;  // 400 iterations
    const uint32_t backlight_on_count = TFT_SPLASH_BACKLIGHT_DELAY_MS / TFT_LVGL_REFRESH_MS;  // 100 iterations

    // Create splash screen label
    lv_obj_t *label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(label, "grblHAL TFT\n\nPhase 3 Foundation\n\nReady!");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

    // Set larger font
    static lv_style_t splash_style;
    lv_style_copy(&splash_style, &lv_style_plain);
    splash_style.text.font = &lv_font_roboto_28;
    splash_style.text.color = UI_COLOR_PRIMARY;
    lv_obj_set_style(label, &splash_style);

    // Main UI loop
    while(1) {
        // Breathing effect and splash screen
        if(splash_count < splash_duration) {
            tft_backlight_breathing();
            splash_count++;

            // Turn on backlight after delay
            if(splash_count == backlight_on_count) {
                tft_backlight_on(255);  // Full brightness
            }

            // After splash, show main screen
            if(splash_count == splash_duration - 1) {
                tft_backlight_on(255);  // Breathing can stop at any level
                ui_screen_manager_show(SCREEN_MAIN);
            }
        }

        // Update screen manager (calls current screen's update callback)
        ui_screen_manager_update();

        // Process LVGL tasks (event handling, animations, updates)
        lvgl_task_handler();

        // Precise timing using relative delay
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/*
 * Plugin initialization
 * Called by grblHAL during startup
 */
void tft_plugin_init(void) {
    // Initialize TFT display hardware
    tft_driver_init();

    // Initialize LVGL graphics library
    lvgl_init();

    // Create FreeRTOS UI task on Core 0
    xTaskCreatePinnedToCore(
        tft_ui_task,                // Task function
        "TFT_UI",                   // Task name
        TFT_TASK_STACK_SIZE,        // Stack size (8192 bytes)
        NULL,                       // Parameters
        TFT_TASK_PRIORITY,          // Priority (2)
        NULL,                       // Task handle
        TFT_TASK_CORE               // Core 0 (UI), Core 1 is for motion
    );

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
