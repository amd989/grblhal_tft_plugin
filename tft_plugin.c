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
 * Threading: LVGL only runs in the UI task. grblHAL state reaches the UI
 * through the ui_state cache, which tft_interface.c fills from grblHAL's
 * foreground loop. UI commands go the other way through a queue.
 */

#include "driver.h"

#if TFT_ENABLE

#if defined(BOARD_MKS_DLC32_V2P0)
#error "TFT plugin: the stock MKS DLC32 map gives the LCD pins to grblHAL. Use BOARD_MY_MACHINE with tft/boards/mks_dlc32_2_0_tft_map.h, see the plugin README."
#endif

#include <lvgl.h>
#include "grbl/hal.h"

#include "tft_plugin.h"
#include "tft_config.h"
#include "tft_driver.h"
#include "tft_interface.h"
#include "lvgl_init.h"
#include "ui_state.h"
#include "ui_screen_manager.h"
#include "ui_widgets.h"
#include "screens/screen_main.h"
#include "screens/screen_jog.h"
#include "screens/screen_calibrate.h"
#include "tft_touch.h"

static on_report_options_ptr on_report_options;

/*
 * Report: Add plugin info to $I command
 */
static void tft_report_options(bool newopt) {
    // Chain to previous handler first
    if(on_report_options)
        on_report_options(newopt);

    // Report this plugin
    if(!newopt)
        hal.stream.write("[PLUGIN:TFT UI v0.2]" ASCII_EOL);
}

/*
 * $TFTCAL: show the touch calibration screen. Not while the machine moves,
 * saving to flash stalls both cores.
 */
static status_code_t tft_calibrate_cmd (sys_state_t state, char *args)
{
    if(!(state == STATE_IDLE || (state & (STATE_ALARM|STATE_ESTOP|STATE_CHECK_MODE))))
        return Status_IdleError;

    tft_touch_request_calibration();

    return Status_OK;
}

static const sys_command_t tft_command_list[] = {
    { "TFTCAL", tft_calibrate_cmd, { .noargs = On, .allow_blocking = On }, { .str = "calibrate the TFT touchscreen" } }
};

static sys_commands_t tft_commands = {
    .n_commands = sizeof(tft_command_list) / sizeof(sys_command_t),
    .commands = tft_command_list
};

/*
 * UI Task - runs on Core 0, owns LVGL
 */
static void tft_ui_task(void *param) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(TFT_LVGL_REFRESH_MS);

    // Initialize foundation components
    ui_screen_manager_init();
    ui_widgets_init();
    screen_main_register();
    screen_jog_register();
    screen_calibrate_register();

    // Breathing effect during startup (2 seconds)
    uint32_t splash_count = 0;
    const uint32_t splash_duration = TFT_SPLASH_DURATION_MS / TFT_LVGL_REFRESH_MS;  // 400 iterations
    const uint32_t backlight_on_count = TFT_SPLASH_BACKLIGHT_DELAY_MS / TFT_LVGL_REFRESH_MS;  // 100 iterations

    // Create splash screen label
    lv_obj_t *label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(label, "grblHAL TFT");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

    // Set larger font
    static lv_style_t splash_style;
    lv_style_copy(&splash_style, &lv_style_plain);
    splash_style.text.font = &lv_font_roboto_28;
    splash_style.text.color = UI_COLOR_PRIMARY;
    lv_obj_set_style(label, &splash_style);
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

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
        } else if(tft_touch_calibration_requested())
            ui_screen_manager_show(SCREEN_CALIBRATE);

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
    tft_touch_init();

    // UI state cache and grblHAL link (command queue, state polling)
    ui_state_init();
    tft_interface_init();

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

    on_report_options = grbl.on_report_options;
    grbl.on_report_options = tft_report_options;

    system_register_commands(&tft_commands);

    // Print initialization message
    hal.stream.write("[TFT Plugin initialized]" ASCII_EOL);
}

#endif // TFT_ENABLE
