/*
 * screen_job.c - Running job: progress, overrides, hold and stop
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320, the status bar stays, no tabs or action bar so the job
 * cannot be left from the screen):
 *   top edge    progress bar
 *   left        file name, percent and line, time done and left, and
 *               during a tool change what to do
 *   top right   Hold / Resume, large: the button most needed. Done once the
 *               job has ended, back to Jog.
 *   middle      feed and spindle (laser) overrides: - value +, tapping the
 *               value sets 100%
 *   bottom right Stop, small, confirmed ($458)
 *
 * On a tool change (M6, with $341 set) Resume continues; with $341=3
 * grblHAL then measures the new tool on the toolsetter before going on.
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
#include "ui_chrome.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_interface.h"
#include "tft_sd.h"

#define UPDATE_MS       200
#define ROW_FEED        180
#define ROW_SPINDLE     244

static lv_obj_t *bar, *lbl_name, *lbl_info, *lbl_time, *lbl_tool;
static lv_obj_t *btn_hold, *lbl_hold_icon, *lbl_hold;
static lv_obj_t *lbl_row_name[2], *btn_row[2][3], *lbl_row_value[2], *lbl_row_actual[2];
static ui_confirm_btn_t stop_btn;

static char txt_name[56], txt_info[48], txt_time[48], txt_tool[48], txt_value[2][8], txt_actual[2][32], txt_row_name[2][12];
static int8_t hold_shown = -1, tool_shown = -1;
static uint32_t last_update;
static bool ended;
static int16_t shown_pct = -1;

static bool held (sys_state_t state)
{
    return !!(state & (STATE_HOLD|STATE_SAFETY_DOOR|STATE_TOOL_CHANGE));
}

/*
 * Events
 */

static void on_override (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    for(uint_fast8_t row = 0; row < 2; row++) {
        for(uint_fast8_t idx = 0; idx < 3; idx++) {
            if(obj == btn_row[row][idx]) {
                int8_t dir = idx == 0 ? -1 : idx == 2 ? 1 : 0;   // the value resets to 100%
                if(row == 0)
                    tft_feed_override(dir);
                else
                    tft_spindle_override(dir);
            }
        }
    }
    last_update = 0;
}

static void on_hold (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(ended) {
        ui_screen_manager_show(SCREEN_HOME);
        return;
    }

    tft_log("btn job hold");
    if(held(ui_state.system_state))
        tft_resume();
    else
        tft_feed_hold();
    ui_confirm_btn_cancel(&stop_btn);
    last_update = 0;
}

static void do_stop (void)
{
    tft_log("btn job stop confirmed");
    tft_stop();
    last_update = 0;
}

/*
 * Creation
 */

static lv_obj_t *label (lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const lv_font_t *font, lv_color_t color)
{
    return ui_widget_create_styled_label(parent, x, y, ui_widget_text_style(font, color), "");
}

static void override_row (lv_obj_t *parent, uint_fast8_t row, lv_coord_t y)
{
    lbl_row_name[row] = label(parent, 8, y + 18, UI_FONT_MEDIUM, UI_COLOR_TEXT_DIM);
    btn_row[row][0] = ui_widget_create_button(parent, 72, y, 64, 56, LV_SYMBOL_MINUS, BUTTON_STYLE_DEFAULT, on_override);
    btn_row[row][1] = ui_widget_create_button(parent, 140, y, 100, 56, "", BUTTON_STYLE_PANEL, on_override);
    lbl_row_value[row] = lv_obj_get_child(btn_row[row][1], NULL);
    lv_label_set_style(lbl_row_value[row], LV_LABEL_STYLE_MAIN, ui_widget_text_style(UI_FONT_LARGE, UI_COLOR_TEXT));
    btn_row[row][2] = ui_widget_create_button(parent, 244, y, 64, 56, LV_SYMBOL_PLUS, BUTTON_STYLE_DEFAULT, on_override);
    lbl_row_actual[row] = label(parent, 316, y + 10, UI_FONT_SMALL, UI_COLOR_TEXT_DIM);
}

static void screen_create (lv_obj_t *parent)
{
    lv_obj_set_style(parent, ui_widget_screen_style());

    bar = ui_widget_create_progress_bar(parent, 0, UI_STATUS_H, TFT_DISPLAY_WIDTH, 8);

    lbl_name = label(parent, 8, 52, UI_FONT_LARGE, UI_COLOR_TEXT);
    lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_name, 232);
    lbl_info = label(parent, 8, 82, UI_FONT_MEDIUM, UI_COLOR_TEXT_DIM);
    lbl_time = label(parent, 8, 104, UI_FONT_MEDIUM, UI_COLOR_TEXT_DIM);
    lbl_tool = label(parent, 8, 128, UI_FONT_MEDIUM, UI_COLOR_WARNING);
    lv_label_set_long_mode(lbl_tool, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(lbl_tool, 232);

    // Hold, large in the top right, where the action bar's Hold sits on the other screens
    btn_hold = lv_btn_create(parent, NULL);
    lv_obj_set_pos(btn_hold, 248, 50);
    lv_obj_set_size(btn_hold, 224, 122);
    lv_btn_set_layout(btn_hold, LV_LAYOUT_CENTER);
    ui_widget_set_button_style(btn_hold, BUTTON_STYLE_WARNING);
    lv_obj_set_event_cb(btn_hold, on_hold);
    lbl_hold_icon = lv_label_create(btn_hold, NULL);
    lbl_hold = lv_label_create(btn_hold, NULL);

    override_row(parent, 0, ROW_FEED);
    override_row(parent, 1, ROW_SPINDLE);

    ui_confirm_btn_create(&stop_btn, parent, 408, ROW_SPINDLE, 64, 68, LV_SYMBOL_STOP "\nStop", BUTTON_STYLE_OUTLINE_DANGER, do_stop);
}

static void screen_show (void)
{
    txt_name[0] = txt_info[0] = txt_time[0] = txt_tool[0] = '\0';
    for(uint_fast8_t row = 0; row < 2; row++)
        txt_value[row][0] = txt_actual[row][0] = txt_row_name[row][0] = '\0';
    hold_shown = tool_shown = -1;
    shown_pct = -1;
    ended = false;
    ui_confirm_btn_cancel(&stop_btn);
    lv_obj_set_hidden(stop_btn.btn, false);
    last_update = 0;
}

static void screen_update (void)
{
    if(!ended)
        ui_confirm_btn_update(&stop_btn);

    if(last_update && lv_tick_elaps(last_update) < UPDATE_MS)
        return;

    last_update = lv_tick_get();
    if(last_update == 0)
        last_update = 1;

    const tft_sd_job_t *job = tft_sd_job();
    sys_state_t state = ui_state.system_state;
    char buf[64], time[16];

    if(!job->active && !ended) {
        ended = true;
        lv_obj_set_hidden(stop_btn.btn, true);
    }

    ui_widget_set_text(lbl_name, txt_name, sizeof(txt_name), job->name);

    int16_t pct = job->size ? (int16_t)((uint64_t)job->pos_done * 100 / job->size) : 0;
    if(pct != shown_pct) {
        shown_pct = pct;
        lv_bar_set_value(bar, pct, LV_ANIM_OFF);
    }

    // Percent and line, or the result once ended
    if(ended)
        snprintf(buf, sizeof(buf), "%s at line %u", job->finished ? "Finished" : "Stopped", (unsigned)job->line_done);
    else
        snprintf(buf, sizeof(buf), "%d%%, line %u", pct, (unsigned)job->line_done);
    ui_widget_set_text(lbl_info, txt_info, sizeof(txt_info), buf);

    // Time done, and left while running from the average rate so far
    tft_logic_format_time(time, sizeof(time), job->elapsed_ms);
    int n = snprintf(buf, sizeof(buf), "%s done", time);
    uint32_t ms;
    if(!ended && tft_logic_eta(job->elapsed_ms, job->size, job->pos_done, &ms)) {
        char left[16];
        tft_logic_format_time(left, sizeof(left), ms);
        snprintf(buf + n, sizeof(buf) - n, ", %s left", left);
    }
    ui_widget_set_text(lbl_time, txt_time, sizeof(txt_time), buf);

    // Tool change: what to do
    int8_t tool_change = !ended && (state & STATE_TOOL_CHANGE);
    if(tool_change != tool_shown) {
        tool_shown = tool_change;
        lv_obj_set_hidden(lbl_tool, !tool_change);
    }
    if(tool_change) {
        ui_widget_format_tool_change(buf, sizeof(buf));
        ui_widget_set_text(lbl_tool, txt_tool, sizeof(txt_tool), buf);
    }

    // Hold, Resume (orange in a tool change), Done
    int8_t hold = ended ? 3 : tool_change ? 2 : held(state) ? 1 : 0;
    if(hold != hold_shown) {
        static const char *const icon[4] = { LV_SYMBOL_PAUSE, LV_SYMBOL_PLAY, LV_SYMBOL_PLAY, LV_SYMBOL_OK };
        static const char *const text[4] = { "Hold", "Resume", "Resume", "Done" };
        ui_button_style_t style = hold == 3 ? BUTTON_STYLE_PRIMARY : hold == 1 ? BUTTON_STYLE_SUCCESS : BUTTON_STYLE_WARNING;
        lv_color_t color = hold == 3 ? UI_COLOR_TEXT : UI_COLOR_BACKGROUND;

        hold_shown = hold;
        ui_widget_set_button_style(btn_hold, style);
        lv_label_set_style(lbl_hold_icon, LV_LABEL_STYLE_MAIN, ui_widget_text_style(UI_FONT_XLARGE, color));
        lv_label_set_style(lbl_hold, LV_LABEL_STYLE_MAIN, ui_widget_text_style(UI_FONT_XLARGE, color));
        lv_label_set_text(lbl_hold_icon, icon[hold]);
        lv_label_set_text(lbl_hold, text[hold]);
    }

    // Overrides
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)ui_state.feed_override);
    ui_widget_set_text(lbl_row_value[0], txt_value[0], sizeof(txt_value[0]), buf);
    snprintf(buf, sizeof(buf), "%.0f\nmm/min", ui_state.feed_rate);
    ui_widget_set_text(lbl_row_actual[0], txt_actual[0], sizeof(txt_actual[0]), buf);
    ui_widget_set_text(lbl_row_name[0], txt_row_name[0], sizeof(txt_row_name[0]), "Feed");

    snprintf(buf, sizeof(buf), "%u%%", (unsigned)ui_state.spindle_override);
    ui_widget_set_text(lbl_row_value[1], txt_value[1], sizeof(txt_value[1]), buf);
    if(ui_state.laser_mode)
        snprintf(buf, sizeof(buf), "power\n%.0f%%", ui_state.spindle_max > 0.0f ? ui_state.spindle_speed * 100.0f / ui_state.spindle_max : 0.0f);
    else
        snprintf(buf, sizeof(buf), "%.0f\nrpm", ui_state.spindle_speed);
    ui_widget_set_text(lbl_row_actual[1], txt_actual[1], sizeof(txt_actual[1]), buf);
    ui_widget_set_text(lbl_row_name[1], txt_row_name[1], sizeof(txt_row_name[1]), ui_state.laser_mode ? "Laser" : "Spindle");
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
