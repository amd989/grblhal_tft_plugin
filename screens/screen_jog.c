/*
 * screen_jog.c - Jog screen: step jogging of X/Y/Z, step size, set work zero
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320):
 *   top     Back, machine state, Zero XYZ
 *   left    XY pad (Y+, X-, X+, Y-) with Stop (jog cancel) in the middle
 *   middle  Z+ / Z-
 *   right   step size 0.1 / 1 / 10 / 100 mm
 *   bottom  work position
 *
 * Each tap jogs one step: $J=G91 G21 <axis><step> F<feed>.
 * Runs in the UI task only, see screen_main.c.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "screen_jog.h"
#include "ui_screen_manager.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_interface.h"

#define UPDATE_MS       100
#define N_STEPS         4
#define N_JOG           6

#define PAD_X           12
#define PAD_Y           58
#define CELL            72
#define PITCH           78

static const float steps[N_STEPS] = { 0.1f, 1.0f, 10.0f, 100.0f };
static const char *step_text[N_STEPS] = { "0.1", "1", "10", "100" };

// Jog buttons: axis and direction for each
static const struct {
    uint8_t axis;
    int8_t dir;
    lv_coord_t x, y;
    const char *text;
} jogs[N_JOG] = {
    { X_AXIS, -1, PAD_X,             PAD_Y + PITCH,     "X-" },
    { X_AXIS,  1, PAD_X + 2 * PITCH, PAD_Y + PITCH,     "X+" },
    { Y_AXIS,  1, PAD_X + PITCH,     PAD_Y,             "Y+" },
    { Y_AXIS, -1, PAD_X + PITCH,     PAD_Y + 2 * PITCH, "Y-" },
    { Z_AXIS,  1, PAD_X + 3 * PITCH, PAD_Y,             "Z+" },
    { Z_AXIS, -1, PAD_X + 3 * PITCH, PAD_Y + 2 * PITCH, "Z-" },
};

static lv_style_t s_pos, s_caption;

static lv_obj_t *btn_jog[N_JOG];
static lv_obj_t *btn_step[N_STEPS];
static lv_obj_t *lbl_state, *lbl_pos;

static uint8_t step_idx = 1;    // 1 mm
static char txt_state[24], txt_pos[64];
static uint32_t last_update;

static void select_step (uint8_t idx)
{
    step_idx = idx;
    for(uint_fast8_t i = 0; i < N_STEPS; i++)
        ui_widget_set_button_style(btn_step[i], i == idx ? BUTTON_STYLE_PRIMARY : BUTTON_STYLE_DEFAULT);
}

/*
 * Button handlers
 */

static void on_jog (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    for(uint_fast8_t i = 0; i < N_JOG; i++) {
        if(obj == btn_jog[i]) {
            float feed = jogs[i].axis == Z_AXIS ? TFT_JOG_FEED_Z : TFT_JOG_FEED;
            tft_log("btn jog %s %s", jogs[i].text, step_text[step_idx]);
            tft_jog_axis(jogs[i].axis, jogs[i].dir * steps[step_idx], feed);
            break;
        }
    }
}

static void on_step (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    for(uint_fast8_t i = 0; i < N_STEPS; i++) {
        if(obj == btn_step[i]) {
            tft_log("btn step %s", step_text[i]);
            select_step(i);
            break;
        }
    }
}

static void on_stop (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn jog stop");
        tft_jog_cancel();
    }
}

static void on_zero (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn zero xyz");
        tft_zero_all();
    }
}

static void on_back (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        tft_log("btn back");
        ui_screen_manager_show(SCREEN_MAIN);
    }
}

/*
 * Screen callbacks
 */

static void screen_create (lv_obj_t *parent)
{
    lv_style_copy(&s_pos, &lv_style_plain);
    s_pos.text.font = UI_FONT_LARGE;
    s_pos.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_caption, &lv_style_plain);
    s_caption.text.font = UI_FONT_MEDIUM;
    s_caption.text.color = UI_COLOR_TEXT_DIM;

    lv_obj_set_style(parent, ui_widget_screen_style());

    // Top bar
    ui_widget_create_button(parent, 8, 8, 96, 44, "Back", BUTTON_STYLE_DEFAULT, on_back);
    lbl_state = ui_widget_create_styled_label(parent, 120, 14, ui_widget_state_style(STATE_IDLE), "");
    txt_state[0] = '\0';
    ui_widget_create_button(parent, 340, 8, 128, 44, "Zero XYZ", BUTTON_STYLE_DEFAULT, on_zero);

    // XY pad and Z column
    for(uint_fast8_t i = 0; i < N_JOG; i++) {
        if(jogs[i].axis < N_AXIS)
            btn_jog[i] = ui_widget_create_button(parent, jogs[i].x, jogs[i].y, CELL, CELL, jogs[i].text, BUTTON_STYLE_DEFAULT, on_jog);
        else
            btn_jog[i] = NULL;
    }
    ui_widget_create_button(parent, PAD_X + PITCH, PAD_Y + PITCH, CELL, CELL, "Stop", BUTTON_STYLE_DANGER, on_stop);

    // Step size
    ui_widget_create_styled_label(parent, 348, 60, &s_caption, "Step mm");
    for(uint_fast8_t i = 0; i < N_STEPS; i++)
        btn_step[i] = ui_widget_create_button(parent, 348, 86 + i * 50, 120, 44, step_text[i], BUTTON_STYLE_DEFAULT, on_step);
    select_step(step_idx);

    // Work position
    lbl_pos = ui_widget_create_styled_label(parent, 12, 292, &s_pos, "");
    txt_pos[0] = '\0';

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

    char buf[64];

    ui_widget_format_state(buf, sizeof(buf));
    if(strcmp(buf, txt_state)) {
        lv_label_set_style(lbl_state, LV_LABEL_STYLE_MAIN, ui_widget_state_style(ui_state.system_state));
        ui_widget_set_text(lbl_state, txt_state, sizeof(txt_state), buf);
    }

    float pos[3] = {0};
    for(uint_fast8_t idx = 0; idx < 3 && idx < N_AXIS; idx++) {
        pos[idx] = ui_state.work_position[idx];
        if(fabsf(pos[idx]) < 0.0005f)
            pos[idx] = 0.0f;    // no "-0.000"
    }
    snprintf(buf, sizeof(buf), "X %.3f   Y %.3f   Z %.3f", pos[0], pos[1], pos[2]);
    ui_widget_set_text(lbl_pos, txt_pos, sizeof(txt_pos), buf);
}

void screen_jog_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = NULL,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_CONTROL, &callbacks);
}

#endif // TFT_ENABLE
