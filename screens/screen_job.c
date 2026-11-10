/*
 * screen_job.c - Running job: progress, overrides, hold and stop
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320):
 *   top     machine state, file name
 *   middle  progress bar, percent / line / elapsed / remaining time, work
 *           position, feed and spindle with -10% / 100% / +10% override buttons
 *   bottom  Hold/Resume and Stop (tap twice), Done once the job has ended
 *
 * On a tool change (M6, with $341 set) the position line asks for the next
 * tool and Resume continues. With $341=3 grblHAL then measures the new tool
 * on the toolsetter before going on.
 *
 * Shown by the UI loop whenever an SD job starts, also one started from a
 * host with $F=. Stop sends grblHAL's stop command (0x19): motion ramps down,
 * spindle and coolant turn off, the job ends and the position is kept.
 * Runs in the UI task only.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "screen_job.h"
#include "ui_screen_manager.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_interface.h"
#include "tft_sd.h"

#define UPDATE_MS       200
#define CONFIRM_MS      3000
#define DRO_AXES        (N_AXIS < 3 ? N_AXIS : 3)
// Remaining time is extrapolated from the bytes done, once there is enough
#define ETA_MIN_MS      15000
#define ETA_MIN_PERMILLE 10

static lv_style_t s_name, s_info, s_value, s_tool;

static lv_obj_t *lbl_state, *lbl_name, *bar, *lbl_info, *lbl_pos, *lbl_tool, *lbl_feed, *lbl_spindle;
static lv_obj_t *btn_feed[3], *btn_spindle[3], *btn_hold, *lbl_hold, *btn_stop, *lbl_stop;

static char txt_state[24], txt_name[56], txt_info[64], txt_pos[64], txt_feed[40], txt_spindle[40];
static char txt_hold[16], txt_stop[24], txt_tool[48];
static int8_t tool_shown = -1;
static uint32_t last_update, stop_armed_at;
static bool stop_armed, ended;
static int16_t shown_pct = -1;

static const int8_t ovr_dir[3] = { -1, 0, 1 };
static const char *ovr_text[3] = { "-10", "100", "+10" };

static void styles_init (void)
{
    lv_style_copy(&s_name, &lv_style_plain);
    s_name.text.font = UI_FONT_LARGE;
    s_name.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_info, &lv_style_plain);
    s_info.text.font = UI_FONT_MEDIUM;
    s_info.text.color = UI_COLOR_TEXT_DIM;

    lv_style_copy(&s_value, &s_info);
    s_value.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_tool, &s_name);
    s_tool.text.color = UI_COLOR_WARNING;
}

static void format_time (char *buf, size_t size, uint32_t ms)
{
    uint32_t s = ms / 1000;

    if(s >= 3600)
        snprintf(buf, size, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    else
        snprintf(buf, size, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

static void disarm_stop (void)
{
    stop_armed = false;
}

/*
 * Events
 */

static void on_override (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    for(uint_fast8_t idx = 0; idx < 3; idx++) {
        if(obj == btn_feed[idx])
            tft_feed_override(ovr_dir[idx]);
        else if(obj == btn_spindle[idx])
            tft_spindle_override(ovr_dir[idx]);
    }
    last_update = 0;
}

static void on_hold (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED || ended)
        return;

    tft_log("btn job hold clicked");
    if(ui_state.system_state & (STATE_HOLD|STATE_SAFETY_DOOR|STATE_TOOL_CHANGE))
        tft_resume();
    else
        tft_feed_hold();
    disarm_stop();
    last_update = 0;
}

static void on_stop (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(ended)
        ui_screen_manager_show(SCREEN_MAIN);
    else if(!stop_armed) {
        stop_armed = true;
        stop_armed_at = lv_tick_get();
    } else {
        tft_log("btn job stop confirmed");
        tft_stop();
        disarm_stop();
    }
    last_update = 0;
}

/*
 * Screen callbacks
 */

static lv_obj_t *override_row (lv_obj_t *parent, lv_coord_t y, lv_obj_t **btns, lv_obj_t **label)
{
    *label = ui_widget_create_styled_label(parent, 16, y + 12, &s_value, "");

    for(uint_fast8_t idx = 0; idx < 3; idx++)
        btns[idx] = ui_widget_create_button(parent, 252 + idx * 74, y, 68, 44, ovr_text[idx], BUTTON_STYLE_DEFAULT, on_override);

    return *label;
}

static void screen_create (lv_obj_t *parent)
{
    styles_init();
    lv_obj_set_style(parent, ui_widget_screen_style());

    lbl_state = ui_widget_create_styled_label(parent, 16, 6, ui_widget_state_style(STATE_CYCLE), "");

    lbl_name = ui_widget_create_styled_label(parent, 16, 42, &s_name, "");
    lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_name, 448);

    bar = ui_widget_create_progress_bar(parent, 16, 74, 448, 20);

    lbl_info = ui_widget_create_styled_label(parent, 16, 100, &s_info, "");
    lbl_pos = ui_widget_create_styled_label(parent, 16, 124, &s_value, "");
    lbl_tool = ui_widget_create_styled_label(parent, 16, 118, &s_tool, "");
    lv_obj_set_hidden(lbl_tool, true);

    override_row(parent, 150, btn_feed, &lbl_feed);
    override_row(parent, 198, btn_spindle, &lbl_spindle);

    btn_hold = ui_widget_create_button(parent, 12, 252, 222, 60, "Hold", BUTTON_STYLE_DEFAULT, on_hold);
    lbl_hold = lv_obj_get_child(btn_hold, NULL);
    btn_stop = ui_widget_create_button(parent, 246, 252, 222, 60, "Stop", BUTTON_STYLE_DANGER, on_stop);
    lbl_stop = lv_obj_get_child(btn_stop, NULL);
}

static void screen_show (void)
{
    txt_state[0] = txt_name[0] = txt_info[0] = txt_pos[0] = txt_feed[0] = txt_spindle[0] = '\0';
    txt_hold[0] = txt_stop[0] = txt_tool[0] = '\0';
    tool_shown = -1;
    shown_pct = -1;
    ended = false;
    disarm_stop();
    lv_obj_set_hidden(btn_hold, false);
    ui_widget_set_button_style(btn_stop, BUTTON_STYLE_DANGER);
    last_update = 0;
}

static void screen_update (void)
{
    if(last_update && lv_tick_elaps(last_update) < UPDATE_MS)
        return;

    last_update = lv_tick_get();
    if(last_update == 0)
        last_update = 1;

    const tft_sd_job_t *job = tft_sd_job();
    sys_state_t state = ui_state.system_state;
    char buf[64], time[16];

    if(stop_armed && lv_tick_elaps(stop_armed_at) > CONFIRM_MS)
        disarm_stop();

    if(!job->active && !ended) {
        ended = true;
        lv_obj_set_hidden(btn_hold, true);
        ui_widget_set_button_style(btn_stop, BUTTON_STYLE_PRIMARY);
    }

    // State, or the result once ended
    bool finished = job->finished;
    if(ended)
        strcpy(buf, finished ? "Finished" : "Stopped");
    else
        ui_widget_format_state(buf, sizeof(buf));
    if(strcmp(buf, txt_state)) {
        lv_label_set_style(lbl_state, LV_LABEL_STYLE_MAIN, ui_widget_state_style(ended ? (finished ? STATE_IDLE : STATE_ALARM) : state));
        ui_widget_set_text(lbl_state, txt_state, sizeof(txt_state), buf);
    }

    ui_widget_set_text(lbl_name, txt_name, sizeof(txt_name), job->name);

    int16_t pct = job->size ? (int16_t)((uint64_t)job->pos_done * 100 / job->size) : 0;
    if(pct != shown_pct) {
        shown_pct = pct;
        lv_bar_set_value(bar, pct, LV_ANIM_OFF);
    }

    format_time(time, sizeof(time), job->elapsed_ms);
    int n = snprintf(buf, sizeof(buf), "%d%%   line %u   %s", pct, (unsigned)job->line_done, time);

    // Remaining time while running, from the average rate so far
    if(!ended && job->size && job->pos_done < job->size && job->elapsed_ms >= ETA_MIN_MS &&
        (uint64_t)job->pos_done * 1000 >= (uint64_t)job->size * ETA_MIN_PERMILLE) {
        char left[16];
        uint32_t ms = (uint32_t)((uint64_t)job->elapsed_ms * (job->size - job->pos_done) / job->pos_done);
        format_time(left, sizeof(left), ms);
        snprintf(buf + n, sizeof(buf) - n, "   %s left", left);
    }
    ui_widget_set_text(lbl_info, txt_info, sizeof(txt_info), buf);

    // Tool change: ask for the tool in place of the position
    int8_t tool_change = !ended && (state & STATE_TOOL_CHANGE);
    if(tool_change != tool_shown) {
        tool_shown = tool_change;
        lv_obj_set_hidden(lbl_tool, !tool_change);
        lv_obj_set_hidden(lbl_pos, tool_change);
        ui_widget_set_button_style(btn_hold, tool_change ? BUTTON_STYLE_WARNING : BUTTON_STYLE_DEFAULT);
    }
    if(tool_change) {
        ui_widget_format_tool_change(buf, sizeof(buf));
        ui_widget_set_text(lbl_tool, txt_tool, sizeof(txt_tool), buf);
    }

    // Work position
    char *p = buf;
    for(uint_fast8_t idx = 0; idx < DRO_AXES; idx++) {
        float v = ui_state.work_position[idx];
        if(fabsf(v) < 0.0005f)
            v = 0.0f;
        p += snprintf(p, sizeof(buf) - (p - buf), "%c %.3f   ", ui_state_get_axis_name(idx), v);
    }
    ui_widget_set_text(lbl_pos, txt_pos, sizeof(txt_pos), buf);

    snprintf(buf, sizeof(buf), "Feed %u%%   %.0f mm/min", (unsigned)ui_state.feed_override, ui_state.feed_rate);
    ui_widget_set_text(lbl_feed, txt_feed, sizeof(txt_feed), buf);
    if(ui_state.laser_mode)
        snprintf(buf, sizeof(buf), "Laser %u%%   power %.0f%%", (unsigned)ui_state.spindle_override,
                  ui_state.spindle_max > 0.0f ? ui_state.spindle_speed * 100.0f / ui_state.spindle_max : 0.0f);
    else
        snprintf(buf, sizeof(buf), "Spindle %u%%   %.0f rpm", (unsigned)ui_state.spindle_override, ui_state.spindle_speed);
    ui_widget_set_text(lbl_spindle, txt_spindle, sizeof(txt_spindle), buf);

    ui_widget_set_text(lbl_hold, txt_hold, sizeof(txt_hold), (state & (STATE_HOLD|STATE_SAFETY_DOOR|STATE_TOOL_CHANGE)) ? LV_SYMBOL_PLAY " Resume" : LV_SYMBOL_PAUSE " Hold");
    ui_widget_set_text(lbl_stop, txt_stop, sizeof(txt_stop), ended ? "Done" : stop_armed ? "Tap again to stop" : LV_SYMBOL_STOP " Stop");
}

void screen_job_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = NULL,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_JOB, &callbacks);
}

#endif // TFT_ENABLE
