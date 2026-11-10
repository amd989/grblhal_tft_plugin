/*
 * screen_main.c - Main screen: machine state, position readout and basic controls
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320):
 *   top     machine state (colored), Files, Jog, WPos/MPos toggle
 *   middle  X/Y/Z position readout
 *   bottom  Home, Unlock, Hold/Resume, Reset
 *
 * Runs in the UI task only. Reads grblHAL state from the ui_state cache and
 * sends commands through tft_interface, never calling grblHAL directly.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "screen_main.h"
#include "ui_screen_manager.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_interface.h"

#define DRO_AXES        (N_AXIS < 3 ? N_AXIS : 3)
#define UPDATE_MS       100

static lv_style_t s_axis, s_value, s_reason, s_net;

static lv_obj_t *lbl_state, *lbl_reason, *lbl_coord, *lbl_hold, *lbl_net;
static lv_obj_t *lbl_value[DRO_AXES];

// Last shown text, so labels are only redrawn when they change
static char txt_state[24], txt_reason[48], txt_coord[8], txt_hold[8], txt_net[40];
static char txt_value[DRO_AXES][16];
static uint32_t last_update;

static void styles_init (void)
{
    lv_style_copy(&s_axis, &lv_style_plain);
    s_axis.text.font = UI_FONT_XLARGE;
    s_axis.text.color = UI_COLOR_PRIMARY;

    lv_style_copy(&s_value, &lv_style_plain);
    s_value.text.font = UI_FONT_XLARGE;
    s_value.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_net, &lv_style_plain);
    s_net.text.font = UI_FONT_MEDIUM;
    s_net.text.color = UI_COLOR_TEXT_DIM;

    lv_style_copy(&s_reason, &lv_style_plain);
    s_reason.text.font = UI_FONT_MEDIUM;
    s_reason.text.color = UI_COLOR_DANGER;
}

/*
 * Button handlers
 */

static void on_home (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn home clicked");
        tft_home_all();
    }
}

static void on_unlock (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn unlock clicked");
        tft_unlock();
    }
}

static void on_hold (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn hold clicked");
        if(ui_state.system_state & (STATE_HOLD|STATE_SAFETY_DOOR))
            tft_cycle_start();
        else
            tft_feed_hold();
    }
}

static void on_reset (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn reset clicked");
        tft_reset();
    }
}

static void on_coord (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn coord clicked");
        ui_state_toggle_coord_mode();
        last_update = 0;    // refresh now
    }
}

static void on_files (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn files clicked");
        ui_screen_manager_show(SCREEN_FILES);
    }
}

static void on_jog (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn jog clicked");
        ui_screen_manager_show(SCREEN_CONTROL);
    }
}

/*
 * Screen callbacks
 */

static void screen_create (lv_obj_t *parent)
{
    styles_init();
    lv_obj_set_style(parent, ui_widget_screen_style());

    // Top bar
    lbl_state = ui_widget_create_styled_label(parent, 16, 14, ui_widget_state_style(STATE_IDLE), "");
    txt_state[0] = '\0';

    ui_widget_create_button(parent, 148, 8, 96, 44, "Files", BUTTON_STYLE_PRIMARY, on_files);
    ui_widget_create_button(parent, 252, 8, 96, 44, "Jog", BUTTON_STYLE_PRIMARY, on_jog);

    // Alarm reason, empty unless in alarm
    lbl_reason = ui_widget_create_styled_label(parent, 16, 56, &s_reason, "");
    lv_label_set_long_mode(lbl_reason, LV_LABEL_LONG_CROP);
    lv_obj_set_width(lbl_reason, 452);
    txt_reason[0] = '\0';

    lv_obj_t *btn = ui_widget_create_button(parent, 356, 8, 112, 44, "WPos", BUTTON_STYLE_DEFAULT, on_coord);
    lbl_coord = lv_obj_get_child(btn, NULL);
    strcpy(txt_coord, "WPos");

    // Position readout
    for(uint_fast8_t idx = 0; idx < DRO_AXES; idx++) {
        lv_coord_t y = 80 + idx * 54;
        char name[2] = { ui_state_get_axis_name(idx), '\0' };

        ui_widget_create_styled_label(parent, 32, y, &s_axis, name);

        lbl_value[idx] = ui_widget_create_styled_label(parent, 96, y, &s_value, "-0000.000");
        lv_label_set_long_mode(lbl_value[idx], LV_LABEL_LONG_CROP);
        lv_label_set_align(lbl_value[idx], LV_LABEL_ALIGN_RIGHT);
        lv_obj_set_width(lbl_value[idx], 300);
        txt_value[idx][0] = '\0';
    }

    // Network status, empty without Wi-Fi/Ethernet support
    lbl_net = ui_widget_create_styled_label(parent, 16, 222, &s_net, "");
    txt_net[0] = '\0';

    // Controls
    ui_widget_create_button(parent, 12, 248, 105, 60, "Home", BUTTON_STYLE_PRIMARY, on_home);
    ui_widget_create_button(parent, 129, 248, 105, 60, "Unlock", BUTTON_STYLE_DEFAULT, on_unlock);
    btn = ui_widget_create_button(parent, 246, 248, 105, 60, "Hold", BUTTON_STYLE_DEFAULT, on_hold);
    lbl_hold = lv_obj_get_child(btn, NULL);
    strcpy(txt_hold, "Hold");
    ui_widget_create_button(parent, 363, 248, 105, 60, "Reset", BUTTON_STYLE_DANGER, on_reset);

    last_update = 0;
}

static void screen_show (void)
{
    last_update = 0;    // refresh now
}

static void screen_update (void)
{
    if(last_update && lv_tick_elaps(last_update) < UPDATE_MS)
        return;

    last_update = lv_tick_get();
    if(last_update == 0)
        last_update = 1;

    char buf[24];
    sys_state_t state = ui_state.system_state;

    // Machine state
    ui_widget_format_state(buf, sizeof(buf));
    if(strcmp(buf, txt_state)) {
        lv_label_set_style(lbl_state, LV_LABEL_STYLE_MAIN, ui_widget_state_style(state));
        ui_widget_set_text(lbl_state, txt_state, sizeof(txt_state), buf);
    }

    ui_widget_set_text(lbl_reason, txt_reason, sizeof(txt_reason), ui_state_get_alarm_string());
    ui_widget_set_text(lbl_net, txt_net, sizeof(txt_net), ui_state.net_status);

    // Position
    float *pos = ui_state.coord_mode == COORD_MODE_WORK ? ui_state.work_position : ui_state.machine_position;

    for(uint_fast8_t idx = 0; idx < DRO_AXES; idx++) {
        float value = pos[idx];
        if(fabsf(value) < 0.0005f)
            value = 0.0f;   // no "-0.000"
        snprintf(buf, sizeof(buf), "%.3f", value);
        ui_widget_set_text(lbl_value[idx], txt_value[idx], sizeof(txt_value[idx]), buf);
    }

    ui_widget_set_text(lbl_coord, txt_coord, sizeof(txt_coord), ui_state.coord_mode == COORD_MODE_WORK ? "WPos" : "MPos");
    ui_widget_set_text(lbl_hold, txt_hold, sizeof(txt_hold), (state & (STATE_HOLD|STATE_SAFETY_DOOR)) ? "Resume" : "Hold");
}

void screen_main_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = NULL,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_MAIN, &callbacks);
}

#endif // TFT_ENABLE
