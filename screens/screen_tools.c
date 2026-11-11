/*
 * screen_tools.c - Tools tab: spindle, screen settings, calibration, info
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (content area):
 *   Spindle on (confirmed, $458) / off (one tap), or in laser mode a test
 *   fire at TFT_LASER_TEST_POWER while held
 *   Brightness - value + ($456)
 *   Touch beep ($457), confirm by tapping twice or holding ($458)
 *   Position shown: work or machine, Calibrate touch (idle or alarm only)
 *   Network, versions, screen memory
 *
 * The spindle sits here, not next to the jog buttons: starting it while
 * jogging is rare and risky. The status bar badge turns it off with one tap.
 * Runs in the UI task only.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>

#include "grbl/grbl.h"

#include "screen_tools.h"
#include "ui_screen_manager.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_plugin.h"
#include "tft_driver.h"
#include "tft_interface.h"
#include "tft_settings.h"
#include "tft_touch.h"

#define UPDATE_MS       250
#define ROW1            UI_CONTENT_Y
#define ROW2            104
#define ROW3            160
#define ROW4            216
#define INFO_Y          270
#define COL2            (UI_CONTENT_X + 176)

static ui_confirm_btn_t spindle_btn;
static lv_obj_t *btn_laser, *lbl_laser, *lbl_spindle_info, *lbl_bright, *btn_beep, *btn_confirm, *btn_coord;
static lv_obj_t *lbl_beep, *lbl_confirm, *lbl_coord, *lbl_info;
static char txt_spindle_info[48], txt_bright[8], txt_info[160];
static int8_t spindle_shown = -1, laser_shown = -1, beep_shown = -1, confirm_shown = -1, coord_shown = -1;
static bool laser_firing;
static uint32_t last_update;

static bool spindle_on (void)
{
    return ui_state.spindle_speed > 0.0f;
}

/*
 * Events
 */

static void toggle_spindle (void)
{
    tft_log("btn spindle %s", spindle_on() ? "off" : "on");
    tft_spindle(!spindle_on());
    last_update = 0;
}

// Laser mode: fires at low power while held, never longer than TFT_LASER_TEST_MAX_MS
static void on_laser (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_PRESSED && ui_state.system_state == STATE_IDLE) {
        tft_log("btn laser test on");
        laser_firing = true;
        tft_laser_test(true);
    } else if((event == LV_EVENT_RELEASED || event == LV_EVENT_PRESS_LOST) && laser_firing) {
        tft_log("btn laser test off");
        laser_firing = false;
        tft_laser_test(false);
    } else
        return;

    ui_widget_set_button_style(btn_laser, laser_firing ? BUTTON_STYLE_WARNING : BUTTON_STYLE_DEFAULT);
    lv_label_set_text(lbl_laser, laser_firing ? "Firing" : "Laser test");
}

static void on_bright (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED && event != LV_EVENT_LONG_PRESSED_REPEAT)
        return;

    int step = lv_obj_get_x(obj) < COL2 ? -10 : 10;
    int value = tft_settings_brightness() + step;

    tft_settings_set_brightness(tft_logic_brightness(value));
    last_update = 0;
}

static void on_toggle (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(obj == btn_beep)
        tft_settings_set_beep(!tft_settings_beep());
    else if(obj == btn_confirm)
        tft_settings_set_confirm_hold(!tft_settings_confirm_hold());
    else if(obj == btn_coord)
        ui_state_toggle_coord_mode();

    last_update = 0;
}

static void on_calibrate (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    // Saving the calibration to flash stalls both cores, as for $TFTCAL
    if(ui_state.system_state == STATE_IDLE || (ui_state.system_state & STATE_ALARM)) {
        tft_log("btn calibrate");
        tft_touch_request_calibration();
    } else
        lv_label_set_text(lbl_info, "Calibrate only while idle or in alarm");
}

/*
 * Creation
 */

static lv_obj_t *toggle (lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_obj_t **label)
{
    lv_obj_t *btn = ui_widget_create_button(parent, x, y, 170, 48, "", BUTTON_STYLE_DEFAULT, on_toggle);

    *label = lv_obj_get_child(btn, NULL);

    return btn;
}

static void screen_create (lv_obj_t *parent)
{
    lv_obj_set_style(parent, ui_widget_screen_style());

    // Spindle, or the laser test
    ui_confirm_btn_create(&spindle_btn, parent, UI_CONTENT_X, ROW1, 170, 56, LV_SYMBOL_REFRESH " Spindle on",
                          BUTTON_STYLE_DEFAULT, toggle_spindle);
    btn_laser = ui_widget_create_button(parent, UI_CONTENT_X, ROW1, 170, 56, "Laser test", BUTTON_STYLE_DEFAULT, on_laser);
    lbl_laser = lv_obj_get_child(btn_laser, NULL);
    lbl_spindle_info = ui_widget_create_styled_label(parent, COL2 + 4, ROW1 + 10, ui_widget_text_style(UI_FONT_SMALL, UI_COLOR_TEXT_DIM), "");
    txt_spindle_info[0] = '\0';

    // Brightness - value +
    ui_widget_create_styled_label(parent, UI_CONTENT_X + 4, ROW2 + 14, ui_widget_text_style(UI_FONT_MEDIUM, UI_COLOR_TEXT), "Brightness");
    ui_widget_create_button(parent, COL2 - 64, ROW2, 56, 48, LV_SYMBOL_MINUS, BUTTON_STYLE_DEFAULT, on_bright);
    ui_widget_create_panel(parent, COL2 - 4, ROW2, 112, 48);
    lbl_bright = ui_widget_create_styled_label(parent, COL2 + 26, ROW2 + 12, ui_widget_text_style(UI_FONT_LARGE, UI_COLOR_TEXT), "");
    txt_bright[0] = '\0';
    ui_widget_create_button(parent, COL2 + 114, ROW2, 56, 48, LV_SYMBOL_PLUS, BUTTON_STYLE_DEFAULT, on_bright);

    btn_beep = toggle(parent, UI_CONTENT_X, ROW3, &lbl_beep);
    btn_confirm = toggle(parent, COL2, ROW3, &lbl_confirm);
    btn_coord = toggle(parent, UI_CONTENT_X, ROW4, &lbl_coord);
    ui_widget_create_button(parent, COL2, ROW4, 170, 48, "Calibrate touch", BUTTON_STYLE_DEFAULT, on_calibrate);

    lbl_info = ui_widget_create_styled_label(parent, UI_CONTENT_X + 4, INFO_Y, ui_widget_text_style(UI_FONT_SMALL, UI_COLOR_TEXT_DIM), "");
    txt_info[0] = '\0';

    last_update = 0;
}

static void screen_show (void)
{
    spindle_shown = laser_shown = beep_shown = confirm_shown = coord_shown = -1;
    ui_confirm_btn_cancel(&spindle_btn);
    txt_info[0] = '\0';
    last_update = 0;
}

static void screen_hide (void)
{
    if(laser_firing) {
        laser_firing = false;
        tft_laser_test(false);
    }
}

static void screen_update (void)
{
    ui_confirm_btn_update(&spindle_btn);

    if(last_update && lv_tick_elaps(last_update) < UPDATE_MS)
        return;

    last_update = lv_tick_get();
    if(last_update == 0)
        last_update = 1;

    char buf[160];
    lv_mem_monitor_t mon;
    bool hold = tft_settings_confirm_hold();

    // Spindle on/off, or the laser test in laser mode
    if(ui_state.laser_mode != laser_shown) {
        laser_shown = ui_state.laser_mode;
        lv_obj_set_hidden(spindle_btn.btn, ui_state.laser_mode);
        lv_obj_set_hidden(btn_laser, !ui_state.laser_mode);
        spindle_shown = -1;
    }

    int8_t on = spindle_on();
    if(!ui_state.laser_mode && on != spindle_shown) {
        spindle_shown = on;
        ui_confirm_btn_set(&spindle_btn, on ? LV_SYMBOL_STOP " Spindle off" : LV_SYMBOL_REFRESH " Spindle on",
                           on ? BUTTON_STYLE_SUCCESS : BUTTON_STYLE_DEFAULT, on);
    }

    if(ui_state.laser_mode)
        snprintf(buf, sizeof(buf), "Hold to fire at %.0f%%\nfor at most %u s", (double)TFT_LASER_TEST_POWER, (unsigned)(TFT_LASER_TEST_MAX_MS / 1000));
    else if(on)
        snprintf(buf, sizeof(buf), "Running at %.0f rpm\none tap stops it", ui_state.spindle_speed);
    else
        snprintf(buf, sizeof(buf), "Starts at %d rpm\n%s", TFT_SPINDLE_RPM, hold ? "hold 1 s to start" : "tap twice to start");
    ui_widget_set_text(lbl_spindle_info, txt_spindle_info, sizeof(txt_spindle_info), buf);

    snprintf(buf, sizeof(buf), "%u%%", (unsigned)tft_settings_brightness());
    ui_widget_set_text(lbl_bright, txt_bright, sizeof(txt_bright), buf);

    int8_t beep = tft_settings_beep();
    if(beep != beep_shown) {
        beep_shown = beep;
        ui_widget_set_button_style(btn_beep, beep ? BUTTON_STYLE_SUCCESS : BUTTON_STYLE_DEFAULT);
        lv_label_set_text(lbl_beep, beep ? "Touch beep on" : "Touch beep off");
    }

    if(hold != confirm_shown) {
        confirm_shown = hold;
        lv_label_set_text(lbl_confirm, hold ? "Confirm: hold 1 s" : "Confirm: tap twice");
    }

    int8_t coord = ui_state.coord_mode;
    if(coord != coord_shown) {
        coord_shown = coord;
        lv_label_set_text(lbl_coord, coord == COORD_MODE_WORK ? "Show work pos" : "Show machine pos");
    }

    // Network, versions, screen memory
    lv_mem_monitor(&mon);
    snprintf(buf, sizeof(buf), "%s%s%s\ngrblHAL %s.%u, TFT plugin v%s\nScreen memory %u of %u KB",
             *ui_state.hostname ? ui_state.hostname : "", *ui_state.hostname ? ".local, " : "",
             *ui_state.net_status ? ui_state.net_status : "No network",
             GRBL_VERSION, (unsigned)GRBL_BUILD, TFT_PLUGIN_VERSION,
             (unsigned)((mon.total_size - mon.free_size + 512) / 1024), (unsigned)(mon.total_size / 1024));
    ui_widget_set_text(lbl_info, txt_info, sizeof(txt_info), buf);
}

void screen_tools_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = screen_hide,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_TOOL, &callbacks);
}

#endif // TFT_ENABLE
