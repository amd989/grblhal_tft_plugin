/*
 * screen_jog.c - Jog screen: jogging, work zero, Z probe, spindle
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320):
 *   top     Back, machine state, Zero XY, Zero Z
 *   left    XY pad (Y+, X-, X+, Y-) with Stop (jog cancel) in the middle
 *   middle  Z+ / Z-
 *   right   step size 0.1 / 1 / 10 / 100 mm, go to XY0 / Z0, probe Z, spindle
 *   bottom  work position
 *   overlay confirm before probing, with the live probe input
 *   overlay tool setter (TOOLSETTER_ENABLE): set its position, measure a tool
 *
 * Each tap jogs one step: $J=G91 G21 <axis><step> F<feed>. Going to zero is
 * a jog too, so Stop cancels it. Spindle on takes two taps, off one.
 * In laser mode ($32=1) the Spindle button is Laser: it fires at
 * TFT_LASER_TEST_POWER while held, see tft_laser_test().
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
#define CONFIRM_MS      3000
#define N_STEPS         4
#define N_JOG           6

#define PAD_X           12
#define PAD_Y           58
#define CELL            66
#define PITCH           72

// Right column: two buttons per row
#define COL_X           306
#define COL_W           78
#define COL_PITCH       84
#define ROW_Y(r)        (58 + (r) * 50)
#define ROW_H           44

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

static lv_style_t s_pos, s_text, s_title, s_probe_open, s_probe_hit;

static lv_obj_t *btn_jog[N_JOG];
static lv_obj_t *btn_step[N_STEPS];
static lv_obj_t *btn_zero_xy, *btn_zero_z, *btn_go_xy, *btn_go_z, *btn_probe, *btn_spindle, *lbl_spindle;
static lv_obj_t *lbl_state, *lbl_pos;
static lv_obj_t *probe_panel, *lbl_probe_input, *lbl_probe_status, *btn_probe_start, *btn_probe_cancel;
#if TOOLSETTER_ENABLE
static lv_obj_t *btn_probe_tool, *tool_panel, *lbl_tool_input, *lbl_tool_pos, *lbl_tool_ofs, *lbl_tool_status;
static lv_obj_t *btn_tool_back, *btn_tool_set, *btn_tool_measure, *lbl_tool_set;
static char txt_tool_input[32], txt_tool_pos[64], txt_tool_ofs[64], txt_tool_set[16];
static int8_t tool_shown = -1, tool_set_shown = -1;
static uint32_t tool_set_armed_at;
static bool tool_set_armed;
#endif

static uint8_t step_idx = 1;    // 1 mm
static char txt_state[24], txt_pos[64], txt_spindle[16], txt_probe_input[32];
static uint32_t last_update, spindle_armed_at;
static bool spindle_armed, laser_firing;
static int8_t spindle_shown = -1;
static int8_t probe_shown = -1;

static void select_step (uint8_t idx)
{
    step_idx = idx;
    for(uint_fast8_t i = 0; i < N_STEPS; i++)
        ui_widget_set_button_style(btn_step[i], i == idx ? BUTTON_STYLE_PRIMARY : BUTTON_STYLE_DEFAULT);
}

static bool spindle_on (void)
{
    return ui_state.spindle_speed > 0.0f;
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

static void on_action (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(obj == btn_zero_xy) {
        tft_log("btn zero xy");
        tft_send_command("G10 L20 P0 X0 Y0");
    } else if(obj == btn_zero_z) {
        tft_log("btn zero z");
        tft_zero_axis(Z_AXIS);
    } else if(obj == btn_go_xy) {
        tft_log("btn go xy0");
        tft_goto_zero_xy();
    } else if(obj == btn_go_z) {
        tft_log("btn go z0");
        tft_goto_zero_z();
    } else if(obj == btn_probe) {
        lv_label_set_text(lbl_probe_status, "");
        probe_shown = -1;
        lv_obj_set_hidden(probe_panel, false);
    } else if(obj == btn_spindle && !ui_state.laser_mode) {
        if(spindle_on()) {
            tft_log("btn spindle off");
            tft_spindle(false);
            spindle_armed = false;
        } else if(!spindle_armed) {
            spindle_armed = true;
            spindle_armed_at = lv_tick_get();
        } else {
            tft_log("btn spindle on");
            tft_spindle(true);
            spindle_armed = false;
        }
        spindle_shown = -1;
        last_update = 0;
    }
}

// Laser mode: the Spindle button fires the laser at low power while held
static void on_spindle (lv_obj_t *obj, lv_event_t event)
{
    if(!ui_state.laser_mode) {
        on_action(obj, event);
        return;
    }

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

    spindle_shown = -1;
    last_update = 0;
}

static void on_probe_panel (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(obj == btn_probe_cancel)
        lv_obj_set_hidden(probe_panel, true);
#if TOOLSETTER_ENABLE
    else if(obj == btn_probe_tool) {
        lv_obj_set_hidden(probe_panel, true);
        lv_label_set_text(lbl_tool_status, "");
        tool_shown = tool_set_shown = -1;
        tool_set_armed = false;
        txt_tool_pos[0] = txt_tool_ofs[0] = '\0';
        lv_obj_set_hidden(tool_panel, false);
    }
#endif
    else if(obj == btn_probe_start) {
        if(ui_state.system_state != STATE_IDLE)
            lv_label_set_text(lbl_probe_status, "Machine must be idle");
        else if(ui_state.probe_triggered)
            lv_label_set_text(lbl_probe_status, "Probe already touching");
        else {
            tft_log("btn probe start");
            tft_probe_z(ui_state.work_position[Z_AXIS]);
            lv_obj_set_hidden(probe_panel, true);
        }
    }
}

#if TOOLSETTER_ENABLE

static void on_tool_panel (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(obj == btn_tool_back)
        lv_obj_set_hidden(tool_panel, true);
    else if(ui_state.system_state != STATE_IDLE)
        lv_label_set_text(lbl_tool_status, "Machine must be idle");
    else if(obj == btn_tool_set) {
        // Two taps, it overwrites the saved position
        if(!tool_set_armed) {
            tool_set_armed = true;
            tool_set_armed_at = lv_tick_get();
        } else {
            tft_log("btn toolsetter set");
            tft_toolsetter_set_here();
            tool_set_armed = false;
            lv_label_set_text(lbl_tool_status, "Toolsetter position saved");
        }
        tool_set_shown = -1;
    } else if(obj == btn_tool_measure) {
        if(!ui_state.z_homed)
            lv_label_set_text(lbl_tool_status, "Home the machine first");
        else if(!ui_state.tls_pos_set)
            lv_label_set_text(lbl_tool_status, "Set the toolsetter position first");
        else if(ui_state.toolsetter_triggered)
            lv_label_set_text(lbl_tool_status, "Toolsetter already triggered");
        else {
            tft_log("btn tool measure");
            tft_tool_measure();
            lv_label_set_text(lbl_tool_status, ui_state.tlo_ref_set ? "Measuring, sets the offset" : "Measuring, sets the reference");
        }
    }
}

#endif

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

static lv_obj_t *col_button (lv_obj_t *parent, uint_fast8_t row, uint_fast8_t col, const char *text, lv_event_cb_t cb)
{
    return ui_widget_create_button(parent, COL_X + col * COL_PITCH, ROW_Y(row), COL_W, ROW_H, text, BUTTON_STYLE_DEFAULT, cb);
}

static void probe_panel_create (lv_obj_t *parent)
{
    char buf[64];

    probe_panel = lv_obj_create(parent, NULL);
    lv_obj_set_size(probe_panel, TFT_DISPLAY_WIDTH, TFT_DISPLAY_HEIGHT);
    lv_obj_set_style(probe_panel, ui_widget_screen_style());

    ui_widget_create_styled_label(probe_panel, 16, 14, &s_title, "Probe Z");
    ui_widget_create_styled_label(probe_panel, 16, 54, &s_text, "Put the touch plate under the bit and clip it on.");
    snprintf(buf, sizeof(buf), "The bit must be less than %.0f mm above the plate.", (double)TFT_PROBE_DISTANCE);
    ui_widget_create_styled_label(probe_panel, 16, 80, &s_text, buf);
    snprintf(buf, sizeof(buf), "Plate %.2f mm. Sets Z0 on the work surface.", (double)TFT_PROBE_THICKNESS);
    ui_widget_create_styled_label(probe_panel, 16, 106, &s_text, buf);

    lbl_probe_input = ui_widget_create_styled_label(probe_panel, 16, 146, &s_probe_open, "");
    txt_probe_input[0] = '\0';
    lbl_probe_status = ui_widget_create_styled_label(probe_panel, 16, 196, &s_text, "");

#if TOOLSETTER_ENABLE
    btn_probe_cancel = ui_widget_create_button(probe_panel, 12, 248, 144, 60, "Cancel", BUTTON_STYLE_DEFAULT, on_probe_panel);
    btn_probe_tool = ui_widget_create_button(probe_panel, 168, 248, 144, 60, "Tool setter", BUTTON_STYLE_DEFAULT, on_probe_panel);
    btn_probe_start = ui_widget_create_button(probe_panel, 324, 248, 144, 60, LV_SYMBOL_DOWN " Probe", BUTTON_STYLE_SUCCESS, on_probe_panel);
#else
    btn_probe_cancel = ui_widget_create_button(probe_panel, 12, 248, 200, 60, "Cancel", BUTTON_STYLE_DEFAULT, on_probe_panel);
    btn_probe_start = ui_widget_create_button(probe_panel, 268, 248, 200, 60, LV_SYMBOL_DOWN " Probe", BUTTON_STYLE_SUCCESS, on_probe_panel);
#endif

    lv_obj_set_hidden(probe_panel, true);
}

#if TOOLSETTER_ENABLE

static void tool_panel_create (lv_obj_t *parent)
{
    tool_panel = lv_obj_create(parent, NULL);
    lv_obj_set_size(tool_panel, TFT_DISPLAY_WIDTH, TFT_DISPLAY_HEIGHT);
    lv_obj_set_style(tool_panel, ui_widget_screen_style());

    ui_widget_create_styled_label(tool_panel, 16, 14, &s_title, "Tool setter");
    ui_widget_create_styled_label(tool_panel, 16, 50, &s_text, "Set: jog the bit a few mm above the setter.");
    ui_widget_create_styled_label(tool_panel, 16, 74, &s_text, "Measure the first tool after Probe Z, later");
    ui_widget_create_styled_label(tool_panel, 16, 98, &s_text, "tools get an offset and keep the same Z0.");

    lbl_tool_input = ui_widget_create_styled_label(tool_panel, 16, 128, &s_probe_open, "");
    txt_tool_input[0] = '\0';
    lbl_tool_pos = ui_widget_create_styled_label(tool_panel, 16, 168, &s_text, "");
    lbl_tool_ofs = ui_widget_create_styled_label(tool_panel, 16, 192, &s_text, "");
    lbl_tool_status = ui_widget_create_styled_label(tool_panel, 16, 218, &s_text, "");

    btn_tool_back = ui_widget_create_button(tool_panel, 12, 248, 144, 60, "Back", BUTTON_STYLE_DEFAULT, on_tool_panel);
    btn_tool_set = ui_widget_create_button(tool_panel, 168, 248, 144, 60, "Set here", BUTTON_STYLE_DEFAULT, on_tool_panel);
    lbl_tool_set = lv_obj_get_child(btn_tool_set, NULL);
    txt_tool_set[0] = '\0';
    btn_tool_measure = ui_widget_create_button(tool_panel, 324, 248, 144, 60, LV_SYMBOL_DOWN " Measure", BUTTON_STYLE_SUCCESS, on_tool_panel);

    lv_obj_set_hidden(tool_panel, true);
}

static void tool_panel_update (void)
{
    char buf[64];

    int8_t hit = ui_state.toolsetter_triggered;
    if(hit != tool_shown) {
        tool_shown = hit;
        lv_label_set_style(lbl_tool_input, LV_LABEL_STYLE_MAIN, hit ? &s_probe_hit : &s_probe_open);
        ui_widget_set_text(lbl_tool_input, txt_tool_input, sizeof(txt_tool_input),
                            hit ? "Setter: triggered" : "Setter: open");
    }

    if(ui_state.tls_pos_set)
        snprintf(buf, sizeof(buf), "At X %.1f  Y %.1f  Z %.1f (machine)",
                  ui_state.tls_pos[0], ui_state.tls_pos[1], ui_state.tls_pos[2]);
    else
        strcpy(buf, "Position not set");
    ui_widget_set_text(lbl_tool_pos, txt_tool_pos, sizeof(txt_tool_pos), buf);

    if(ui_state.tlo_ref_set)
        snprintf(buf, sizeof(buf), "Reference set, tool offset Z %.3f", ui_state.tlo_z);
    else
        strcpy(buf, "No reference yet (homing clears it)");
    ui_widget_set_text(lbl_tool_ofs, txt_tool_ofs, sizeof(txt_tool_ofs), buf);

    // Set here: orange while waiting for the second tap
    if(tool_set_armed && lv_tick_elaps(tool_set_armed_at) > CONFIRM_MS)
        tool_set_armed = false;

    if(tool_set_armed != tool_set_shown) {
        tool_set_shown = tool_set_armed;
        ui_widget_set_button_style(btn_tool_set, tool_set_armed ? BUTTON_STYLE_WARNING : BUTTON_STYLE_DEFAULT);
        ui_widget_set_text(lbl_tool_set, txt_tool_set, sizeof(txt_tool_set), tool_set_armed ? "Sure?" : "Set here");
    }
}

#endif

static void screen_create (lv_obj_t *parent)
{
    lv_style_copy(&s_pos, &lv_style_plain);
    s_pos.text.font = UI_FONT_LARGE;
    s_pos.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_text, &lv_style_plain);
    s_text.text.font = UI_FONT_MEDIUM;
    s_text.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_title, &s_pos);

    lv_style_copy(&s_probe_open, &s_pos);
    s_probe_open.text.color = UI_COLOR_TEXT_DIM;
    lv_style_copy(&s_probe_hit, &s_pos);
    s_probe_hit.text.color = UI_COLOR_SUCCESS;

    lv_obj_set_style(parent, ui_widget_screen_style());

    // Top bar
    ui_widget_create_button(parent, 8, 8, 80, 44, "Back", BUTTON_STYLE_DEFAULT, on_back);
    lbl_state = ui_widget_create_styled_label(parent, 98, 14, ui_widget_state_style(STATE_IDLE), "");
    txt_state[0] = '\0';
    btn_zero_xy = ui_widget_create_button(parent, COL_X, 8, COL_W, 44, "Zero XY", BUTTON_STYLE_DEFAULT, on_action);
    btn_zero_z = ui_widget_create_button(parent, COL_X + COL_PITCH, 8, COL_W, 44, "Zero Z", BUTTON_STYLE_DEFAULT, on_action);

    // XY pad and Z column
    for(uint_fast8_t i = 0; i < N_JOG; i++) {
        if(jogs[i].axis < N_AXIS)
            btn_jog[i] = ui_widget_create_button(parent, jogs[i].x, jogs[i].y, CELL, CELL, jogs[i].text, BUTTON_STYLE_DEFAULT, on_jog);
        else
            btn_jog[i] = NULL;
    }
    ui_widget_create_button(parent, PAD_X + PITCH, PAD_Y + PITCH, CELL, CELL, "Stop", BUTTON_STYLE_DANGER, on_stop);

    // Step size, two rows of two
    for(uint_fast8_t i = 0; i < N_STEPS; i++)
        btn_step[i] = col_button(parent, i / 2, i % 2, step_text[i], on_step);
    select_step(step_idx);

    btn_go_xy = col_button(parent, 2, 0, "Go XY0", on_action);
    btn_go_z = col_button(parent, 2, 1, "Go Z0", on_action);
    btn_probe = col_button(parent, 3, 0, "Probe Z", on_action);
    btn_spindle = col_button(parent, 3, 1, "Spindle", on_spindle);
    lbl_spindle = lv_obj_get_child(btn_spindle, NULL);
    txt_spindle[0] = '\0';

    // Work position
    lbl_pos = ui_widget_create_styled_label(parent, 12, 282, &s_pos, "");
    txt_pos[0] = '\0';

    probe_panel_create(parent);
#if TOOLSETTER_ENABLE
    tool_panel_create(parent);
#endif

    last_update = 0;
}

static void screen_show (void)
{
    lv_obj_set_hidden(probe_panel, true);
#if TOOLSETTER_ENABLE
    lv_obj_set_hidden(tool_panel, true);
#endif
    spindle_armed = false;
    if(laser_firing) {
        laser_firing = false;
        tft_laser_test(false);
    }
    spindle_shown = -1;
    last_update = 0;    // refresh now
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

    // Spindle: green while on, orange while waiting for the second tap
    if(spindle_armed && lv_tick_elaps(spindle_armed_at) > CONFIRM_MS)
        spindle_armed = false;

    // Laser mode: Laser button, orange while firing.
    int8_t sp = ui_state.laser_mode ? (laser_firing ? 4 : 3) : spindle_on() ? 2 : spindle_armed ? 1 : 0;
    if(sp != spindle_shown) {
        spindle_shown = sp;
        ui_widget_set_button_style(btn_spindle, sp == 2 ? BUTTON_STYLE_SUCCESS : (sp == 1 || sp == 4) ? BUTTON_STYLE_WARNING : BUTTON_STYLE_DEFAULT);
        ui_widget_set_text(lbl_spindle, txt_spindle, sizeof(txt_spindle),
                            sp == 1 ? "Sure?" : sp == 3 ? "Laser" : sp == 4 ? "Firing" : "Spindle");
    }

    // Live probe input on the probe panel, to check the clip before probing
    if(!lv_obj_get_hidden(probe_panel)) {
        int8_t hit = ui_state.probe_triggered;
        if(hit != probe_shown) {
            probe_shown = hit;
            lv_label_set_style(lbl_probe_input, LV_LABEL_STYLE_MAIN, hit ? &s_probe_hit : &s_probe_open);
            ui_widget_set_text(lbl_probe_input, txt_probe_input, sizeof(txt_probe_input),
                                hit ? "Probe: touching" : "Probe: open, touch clip to plate");
        }
    }

#if TOOLSETTER_ENABLE
    if(!lv_obj_get_hidden(tool_panel))
        tool_panel_update();
#endif
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
