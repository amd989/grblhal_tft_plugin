/*
 * screen_jog.c - Jog, the home screen
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (content area, between the tabs and the action bar):
 *   top     X, Y, Z position cells: tap one to zero it
 *   left    3x3 pad: diagonals in the corners, Stop (jog cancel) in the middle
 *   middle  Z+ / Z- bar, apart from the pad
 *   right   step button (tap: next size, hold: previous), Go XY0, Go Z0
 *
 * Steps 0.1 / 1 / 10 / 100 mm jog once per tap: $J=G91 G21 ... F<feed>.
 * Cont. jogs while a button is held and cancels the jog when it is let go
 * (also when the finger slides off). Going to zero is a jog too, so Stop
 * cancels it. Spindle and laser test are on the Tools tab, the probe on the
 * Probe tab. Runs in the UI task only.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "screen_jog.h"
#include "ui_screen_manager.h"
#include "ui_chrome.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "ui_icons.h"
#include "tft_interface.h"

#define UPDATE_MS       100

#define PAD_X           UI_CONTENT_X
#define PAD_Y           102
#define CELL            62
#define PITCH           66
#define ZBAR_X          258
#define COL_X           326
#define COL_W           80

#define N_PAD           9
#define PAD_STOP        4

// Pad buttons, row by row: X and Y direction, and the label of the straight ones
static const struct {
    int8_t dx, dy;
    const char *text;
} pad[N_PAD] = {
    { -1,  1, NULL }, {  0,  1, LV_SYMBOL_UP "\nY+" },   {  1,  1, NULL },
    { -1,  0, LV_SYMBOL_LEFT " X-" }, { 0, 0, NULL },     {  1,  0, "X+ " LV_SYMBOL_RIGHT },
    { -1, -1, NULL }, {  0, -1, "Y-\n" LV_SYMBOL_DOWN }, {  1, -1, NULL },
};

static lv_obj_t *btn_pad[N_PAD], *btn_zup, *btn_zdown, *btn_step, *lbl_step_value, *lbl_step_unit;
static lv_obj_t *btn_go_xy, *btn_go_z;
static lv_obj_t *btn_cell[3], *lbl_cell_axis[3], *lbl_cell_value[3];
static lv_obj_t *zero_scrim, *zero_panel, *lbl_zero_title, *btn_zero_one, *btn_zero_two, *btn_zero_cancel;
static lv_obj_t *lbl_zero_one, *lbl_zero_two;

static uint8_t step_idx = 1;            // 1 mm
static uint8_t zero_axis;
static bool cont_jogging;
static uint32_t last_update;
static char txt_value[3][16], txt_axis[3][12];

static float pos_value (uint_fast8_t axis)
{
    float *pos = ui_state.coord_mode == COORD_MODE_WORK ? ui_state.work_position : ui_state.machine_position;
    float v = axis < N_AXIS ? pos[axis] : 0.0f;

    return fabsf(v) < 0.0005f ? 0.0f : v;   // no "-0.000"
}

static void show_step (void)
{
    lv_label_set_text(lbl_step_value, tft_logic_step_text(step_idx));
    lv_label_set_text(lbl_step_unit, step_idx == TFT_STEP_CONT ? "hold to jog" : "mm step");
}

static void jog (int8_t dx, int8_t dy, int8_t dz, float distance)
{
    float feed = dz ? TFT_JOG_FEED_Z : TFT_JOG_FEED;

    tft_jog(dx * distance, dy * distance, dz * distance, feed);
}

static void stop_cont (void)
{
    if(cont_jogging) {
        cont_jogging = false;
        tft_jog_cancel();
    }
}

/*
 * Events
 */

// Pad and Z buttons: a step per tap, or Cont. while held
static void on_jog (lv_obj_t *obj, lv_event_t event)
{
    int8_t dx = 0, dy = 0, dz = 0;

    if(obj == btn_zup)
        dz = 1;
    else if(obj == btn_zdown)
        dz = -1;
    else for(uint_fast8_t idx = 0; idx < N_PAD; idx++) {
        if(obj == btn_pad[idx]) {
            dx = pad[idx].dx;
            dy = pad[idx].dy;
        }
    }

    if(step_idx == TFT_STEP_CONT) {
        if(event == LV_EVENT_PRESSED) {
            tft_log("jog cont %d %d %d", dx, dy, dz);
            cont_jogging = true;
            jog(dx, dy, dz, dz ? TFT_JOG_CONT_DISTANCE_Z : TFT_JOG_CONT_DISTANCE);
        } else if(event == LV_EVENT_RELEASED || event == LV_EVENT_PRESS_LOST)
            stop_cont();
    } else if(event == LV_EVENT_CLICKED) {
        tft_log("jog %d %d %d %s", dx, dy, dz, tft_logic_step_text(step_idx));
        jog(dx, dy, dz, tft_logic_step_mm(step_idx));
    }
}

static void on_stop (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_PRESSED) {     // at once, not on release
        tft_log("btn jog stop");
        cont_jogging = false;
        tft_jog_cancel();
    }
}

// Tap: next step size, hold: the one before
static void on_step (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_SHORT_CLICKED)
        step_idx = tft_logic_step_next(step_idx, 1);
    else if(event == LV_EVENT_LONG_PRESSED)
        step_idx = tft_logic_step_next(step_idx, -1);
    else
        return;

    tft_log("step %s", tft_logic_step_text(step_idx));
    show_step();
}

static void on_go (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(obj == btn_go_xy) {
        tft_log("btn go xy0");
        tft_goto_zero_xy();
    } else {
        tft_log("btn go z0");
        tft_goto_zero_z();
    }
}

/*
 * Zero popup: tap a position cell
 */

static void zero_close (void)
{
    lv_obj_set_hidden(zero_scrim, true);
    lv_obj_set_hidden(zero_panel, true);
}

static void zero_open (uint8_t axis)
{
    char buf[40];
    bool idle = ui_state.system_state == STATE_IDLE;

    zero_axis = axis;
    if(idle)
        snprintf(buf, sizeof(buf), "Zero %c at %.3f?", ui_state_get_axis_name(axis), ui_state.work_position[axis]);
    else
        strcpy(buf, "Machine must be idle");
    lv_label_set_text(lbl_zero_title, buf);

    snprintf(buf, sizeof(buf), "Zero %c", ui_state_get_axis_name(axis));
    lv_label_set_text(lbl_zero_one, buf);
    lv_label_set_text(lbl_zero_two, axis == Z_AXIS ? "Probe Z" : "Zero XY");
    lv_obj_set_hidden(btn_zero_one, !idle);
    lv_obj_set_hidden(btn_zero_two, !idle);

    lv_obj_set_hidden(zero_scrim, false);
    lv_obj_set_hidden(zero_panel, false);
}

static void on_cell (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    for(uint_fast8_t idx = 0; idx < 3; idx++) {
        if(obj == btn_cell[idx])
            zero_open(idx);
    }
}

static void on_zero (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(obj == btn_zero_one) {
        tft_log("zero %c", ui_state_get_axis_name(zero_axis));
        tft_zero_axes(1 << zero_axis);
    } else if(obj == btn_zero_two) {
        if(zero_axis == Z_AXIS) {
            zero_close();
            ui_screen_manager_show(SCREEN_PROBE);
            return;
        }
        tft_log("zero xy");
        tft_zero_axes((1 << X_AXIS) | (1 << Y_AXIS));
    }

    zero_close();
}

/*
 * Creation
 */

// Diagonal chevron, an image colored like the button text (ui_icons.c)
static void add_arrow (lv_obj_t *btn, int8_t dx, int8_t dy)
{
    lv_obj_t *img = lv_img_create(btn, NULL);

    lv_img_set_src(img, dy > 0 ? (dx > 0 ? &ui_arrow_up_right : &ui_arrow_up_left)
                               : (dx > 0 ? &ui_arrow_down_right : &ui_arrow_down_left));
    lv_obj_set_click(img, false);
    lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
}

static lv_obj_t *label_in (lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent, NULL);

    lv_label_set_style(lbl, LV_LABEL_STYLE_MAIN, ui_widget_text_style(font, color));
    lv_label_set_text(lbl, text);

    return lbl;
}

static void zero_popup_create (lv_obj_t *parent)
{
    zero_scrim = ui_widget_create_scrim(parent);
    lv_obj_set_hidden(zero_scrim, true);

    zero_panel = ui_widget_create_panel(parent, 88, 84, 290, 164);
    lv_obj_set_click(zero_panel, true);

    lbl_zero_title = ui_widget_create_styled_label(zero_panel, 12, 12, ui_widget_text_style(UI_FONT_LARGE, UI_COLOR_TEXT), "");

    btn_zero_one = ui_widget_create_button(zero_panel, 12, 50, 128, 52, "", BUTTON_STYLE_PRIMARY, on_zero);
    lbl_zero_one = lv_obj_get_child(btn_zero_one, NULL);
    btn_zero_two = ui_widget_create_button(zero_panel, 150, 50, 128, 52, "", BUTTON_STYLE_PRIMARY, on_zero);
    lbl_zero_two = lv_obj_get_child(btn_zero_two, NULL);
    btn_zero_cancel = ui_widget_create_button(zero_panel, 12, 108, 266, 44, "Cancel", BUTTON_STYLE_DEFAULT, on_zero);

    lv_obj_set_hidden(zero_panel, true);
}

static void screen_create (lv_obj_t *parent)
{
    lv_obj_set_style(parent, ui_widget_screen_style());

    // Position cells
    for(uint_fast8_t idx = 0; idx < 3; idx++) {
        lv_obj_t *btn = lv_btn_create(parent, NULL);
        lv_obj_set_pos(btn, UI_CONTENT_X + idx * 116, UI_CONTENT_Y);
        lv_obj_set_size(btn, idx == 2 ? 114 : 112, 56);
        lv_btn_set_layout(btn, LV_LAYOUT_OFF);
        ui_widget_set_button_style(btn, BUTTON_STYLE_PANEL);
        lv_obj_set_event_cb(btn, on_cell);

        lbl_cell_axis[idx] = label_in(btn, UI_FONT_SMALL, UI_COLOR_TEXT_DIM, "");
        lv_obj_set_pos(lbl_cell_axis[idx], 8, 4);
        lbl_cell_value[idx] = label_in(btn, UI_FONT_LARGE, UI_COLOR_TEXT, "");
        lv_label_set_long_mode(lbl_cell_value[idx], LV_LABEL_LONG_CROP);
        lv_label_set_align(lbl_cell_value[idx], LV_LABEL_ALIGN_RIGHT);
        lv_obj_set_width(lbl_cell_value[idx], 100);
        lv_obj_set_pos(lbl_cell_value[idx], 4, 24);

        btn_cell[idx] = btn;
        txt_value[idx][0] = txt_axis[idx][0] = '\0';
    }

    // XY pad
    for(uint_fast8_t idx = 0; idx < N_PAD; idx++) {
        lv_coord_t x = PAD_X + (idx % 3) * PITCH, y = PAD_Y + (idx / 3) * PITCH;

        if(idx == PAD_STOP) {
            btn_pad[idx] = ui_widget_create_button(parent, x, y, CELL, CELL, LV_SYMBOL_STOP, BUTTON_STYLE_DANGER, on_stop);
            continue;
        }

        btn_pad[idx] = ui_widget_create_button(parent, x, y, CELL, CELL, pad[idx].text ? pad[idx].text : "", BUTTON_STYLE_DEFAULT, on_jog);
        if(pad[idx].text)
            lv_label_set_align(lv_obj_get_child(btn_pad[idx], NULL), LV_LABEL_ALIGN_CENTER);
        else {
            lv_btn_set_layout(btn_pad[idx], LV_LAYOUT_OFF);
            lv_obj_del(lv_obj_get_child(btn_pad[idx], NULL));   // no label, an arrow
            add_arrow(btn_pad[idx], pad[idx].dx, pad[idx].dy);
        }
    }

    // Z bar, apart from the pad
    btn_zup = ui_widget_create_button(parent, ZBAR_X, PAD_Y, 64, 95, LV_SYMBOL_UP "\nZ+", BUTTON_STYLE_DEFAULT, on_jog);
    lv_label_set_align(lv_obj_get_child(btn_zup, NULL), LV_LABEL_ALIGN_CENTER);
    btn_zdown = ui_widget_create_button(parent, ZBAR_X, PAD_Y + 99, 64, 95, "Z-\n" LV_SYMBOL_DOWN, BUTTON_STYLE_DEFAULT, on_jog);
    lv_label_set_align(lv_obj_get_child(btn_zdown, NULL), LV_LABEL_ALIGN_CENTER);

    // Step button: the value large
    btn_step = lv_btn_create(parent, NULL);
    lv_obj_set_pos(btn_step, COL_X, PAD_Y);
    lv_obj_set_size(btn_step, COL_W, 95);
    lv_btn_set_layout(btn_step, LV_LAYOUT_CENTER);
    ui_widget_set_button_style(btn_step, BUTTON_STYLE_DEFAULT);
    lv_obj_set_event_cb(btn_step, on_step);
    lbl_step_value = label_in(btn_step, UI_FONT_XLARGE, UI_COLOR_TEXT, "");
    lbl_step_unit = label_in(btn_step, UI_FONT_SMALL, UI_COLOR_TEXT_DIM, "");
    show_step();

    btn_go_xy = ui_widget_create_button(parent, COL_X, PAD_Y + 99, COL_W, 45, "Go XY0", BUTTON_STYLE_PRIMARY, on_go);
    btn_go_z = ui_widget_create_button(parent, COL_X, PAD_Y + 148, COL_W, 46, "Go Z0", BUTTON_STYLE_PRIMARY, on_go);

    lv_obj_t *hint = ui_widget_create_styled_label(parent, UI_CONTENT_X, 300, ui_widget_text_style(UI_FONT_SMALL, UI_COLOR_TEXT_DIM),
                                                   "Tap a position to zero it. Hold Step to go back.");
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);    // never under the action bar
    lv_obj_set_width(hint, UI_CONTENT_W);

    zero_popup_create(parent);
    last_update = 0;
}

static void screen_show (void)
{
    zero_close();
    cont_jogging = false;
    last_update = 0;
}

static void screen_hide (void)
{
    stop_cont();
    zero_close();
}

static void screen_update (void)
{
    if(last_update && lv_tick_elaps(last_update) < UPDATE_MS)
        return;

    last_update = lv_tick_get();
    if(last_update == 0)
        last_update = 1;

    char buf[16];

    for(uint_fast8_t idx = 0; idx < 3 && idx < N_AXIS; idx++) {
        snprintf(buf, sizeof(buf), "%.3f", pos_value(idx));
        ui_widget_set_text(lbl_cell_value[idx], txt_value[idx], sizeof(txt_value[idx]), buf);
        snprintf(buf, sizeof(buf), "%c %s", ui_state_get_axis_name(idx), ui_state.coord_mode == COORD_MODE_WORK ? "work" : "machine");
        ui_widget_set_text(lbl_cell_axis[idx], txt_axis[idx], sizeof(txt_axis[idx]), buf);
    }
}

void screen_jog_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = screen_hide,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_CONTROL, &callbacks);
}

#endif // TFT_ENABLE
