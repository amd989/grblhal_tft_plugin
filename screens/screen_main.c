/*
 * screen_main.c - Main screen: machine state, position readout and basic controls
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320):
 *   top     machine state (colored), WPos/MPos toggle
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

static lv_style_t s_bg, s_state_idle, s_state_run, s_state_warn, s_state_alarm, s_axis, s_value;

static lv_obj_t *lbl_state, *lbl_coord, *lbl_hold;
static lv_obj_t *lbl_value[DRO_AXES];

// Last shown text, so labels are only redrawn when they change
static char txt_state[24], txt_coord[8], txt_hold[8];
static char txt_value[DRO_AXES][16];
static uint32_t last_update;

static void text_style (lv_style_t *style, const lv_font_t *font, lv_color_t color)
{
    lv_style_copy(style, &lv_style_plain);
    style->text.font = font;
    style->text.color = color;
}

static void styles_init (void)
{
    lv_style_copy(&s_bg, &lv_style_plain);
    s_bg.body.main_color = UI_COLOR_BACKGROUND;
    s_bg.body.grad_color = UI_COLOR_BACKGROUND;
    s_bg.body.border.width = 0;
    s_bg.body.radius = 0;
    s_bg.text.color = UI_COLOR_TEXT;

    text_style(&s_state_idle, UI_FONT_XLARGE, UI_COLOR_SUCCESS);
    text_style(&s_state_run, UI_FONT_XLARGE, UI_COLOR_PRIMARY);
    text_style(&s_state_warn, UI_FONT_XLARGE, UI_COLOR_WARNING);
    text_style(&s_state_alarm, UI_FONT_XLARGE, UI_COLOR_DANGER);
    text_style(&s_axis, UI_FONT_XLARGE, UI_COLOR_PRIMARY);
    text_style(&s_value, UI_FONT_XLARGE, UI_COLOR_TEXT);
}

static lv_obj_t *make_label (lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_style_t *style, const char *text)
{
    lv_obj_t *label = lv_label_create(parent, NULL);

    lv_label_set_style(label, LV_LABEL_STYLE_MAIN, style);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);

    return label;
}

static void set_text (lv_obj_t *label, char *shown, size_t size, const char *text)
{
    if(strcmp(shown, text)) {
        strncpy(shown, text, size - 1);
        shown[size - 1] = '\0';
        lv_label_set_text(label, text);
    }
}

static lv_style_t *state_style (sys_state_t state)
{
    if(state == STATE_IDLE)
        return &s_state_idle;
    if(state & (STATE_ALARM|STATE_ESTOP))
        return &s_state_alarm;
    if(state & (STATE_HOLD|STATE_SAFETY_DOOR|STATE_TOOL_CHANGE|STATE_CHECK_MODE|STATE_SLEEP))
        return &s_state_warn;

    return &s_state_run;    // cycle, jog, homing
}

/*
 * Button handlers
 */

static void on_home (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_PRESSED || event == LV_EVENT_CLICKED)
        tft_log("btn home %s", event == LV_EVENT_CLICKED ? "clicked" : "pressed");

    if(event == LV_EVENT_CLICKED)
        tft_home_all();
}

static void on_unlock (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_PRESSED || event == LV_EVENT_CLICKED)
        tft_log("btn unlock %s", event == LV_EVENT_CLICKED ? "clicked" : "pressed");

    if(event == LV_EVENT_CLICKED)
        tft_unlock();
}

static void on_hold (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_PRESSED || event == LV_EVENT_CLICKED)
        tft_log("btn hold %s", event == LV_EVENT_CLICKED ? "clicked" : "pressed");

    if(event == LV_EVENT_CLICKED) {
        if(ui_state.system_state & (STATE_HOLD|STATE_SAFETY_DOOR))
            tft_cycle_start();
        else
            tft_feed_hold();
    }
}

static void on_reset (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_PRESSED || event == LV_EVENT_CLICKED)
        tft_log("btn reset %s", event == LV_EVENT_CLICKED ? "clicked" : "pressed");

    if(event == LV_EVENT_CLICKED)
        tft_reset();
}

static void on_coord (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_PRESSED || event == LV_EVENT_CLICKED)
        tft_log("btn coord %s", event == LV_EVENT_CLICKED ? "clicked" : "pressed");

    if(event == LV_EVENT_CLICKED) {
        ui_state_toggle_coord_mode();
        last_update = 0;    // refresh now
    }
}

/*
 * Screen callbacks
 */

static void screen_create (lv_obj_t *parent)
{
    styles_init();
    lv_obj_set_style(parent, &s_bg);

    // Top bar
    lbl_state = make_label(parent, 16, 14, &s_state_idle, "");
    txt_state[0] = '\0';

    lv_obj_t *btn = ui_widget_create_button(parent, 352, 8, 116, 44, "WPos", BUTTON_STYLE_DEFAULT, on_coord);
    lbl_coord = lv_obj_get_child(btn, NULL);
    strcpy(txt_coord, "WPos");

    // Position readout
    for(uint_fast8_t idx = 0; idx < DRO_AXES; idx++) {
        lv_coord_t y = 70 + idx * 56;
        char name[2] = { ui_state_get_axis_name(idx), '\0' };

        make_label(parent, 32, y, &s_axis, name);

        lbl_value[idx] = make_label(parent, 96, y, &s_value, "-0000.000");
        lv_label_set_long_mode(lbl_value[idx], LV_LABEL_LONG_CROP);
        lv_label_set_align(lbl_value[idx], LV_LABEL_ALIGN_RIGHT);
        lv_obj_set_width(lbl_value[idx], 300);
        txt_value[idx][0] = '\0';
    }

    // Controls
    ui_widget_create_button(parent, 12, 248, 105, 60, "Home", BUTTON_STYLE_PRIMARY, on_home);
    ui_widget_create_button(parent, 129, 248, 105, 60, "Unlock", BUTTON_STYLE_DEFAULT, on_unlock);
    btn = ui_widget_create_button(parent, 246, 248, 105, 60, "Hold", BUTTON_STYLE_DEFAULT, on_hold);
    lbl_hold = lv_obj_get_child(btn, NULL);
    strcpy(txt_hold, "Hold");
    ui_widget_create_button(parent, 363, 248, 105, 60, "Reset", BUTTON_STYLE_DANGER, on_reset);

    last_update = 0;
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
    if((state & STATE_ALARM) && ui_state.alarm_code)
        snprintf(buf, sizeof(buf), "ALARM %u", (unsigned)ui_state.alarm_code);
    else
        snprintf(buf, sizeof(buf), "%s", ui_state_get_state_string());

    if(strcmp(buf, txt_state)) {
        lv_label_set_style(lbl_state, LV_LABEL_STYLE_MAIN, state_style(state));
        set_text(lbl_state, txt_state, sizeof(txt_state), buf);
    }

    // Position
    float *pos = ui_state.coord_mode == COORD_MODE_WORK ? ui_state.work_position : ui_state.machine_position;

    for(uint_fast8_t idx = 0; idx < DRO_AXES; idx++) {
        float value = pos[idx];
        if(fabsf(value) < 0.0005f)
            value = 0.0f;   // no "-0.000"
        snprintf(buf, sizeof(buf), "%.3f", value);
        set_text(lbl_value[idx], txt_value[idx], sizeof(txt_value[idx]), buf);
    }

    set_text(lbl_coord, txt_coord, sizeof(txt_coord), ui_state.coord_mode == COORD_MODE_WORK ? "WPos" : "MPos");
    set_text(lbl_hold, txt_hold, sizeof(txt_hold), (state & (STATE_HOLD|STATE_SAFETY_DOOR)) ? "Resume" : "Hold");
}

void screen_main_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = NULL,
        .hide = NULL,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_MAIN, &callbacks);
}

#endif // TFT_ENABLE
