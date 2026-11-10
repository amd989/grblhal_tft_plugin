/*
 * screen_calibrate.c - Touch calibration
 *
 * Part of grblHAL TFT Plugin
 *
 * Started with the $TFTCAL command. Shows a target near each corner, then one
 * in the center to check the result. The calibration is only saved if the
 * center tap lands within MAX_ERROR pixels, otherwise it starts over.
 *
 * Reads raw touches itself; LVGL gets no touches while this screen is shown.
 * Runs in the UI task only.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <stdlib.h>

#include "screen_calibrate.h"
#include "ui_screen_manager.h"
#include "ui_widgets.h"
#include "tft_interface.h"
#include "tft_touch.h"

#define W               TFT_DISPLAY_WIDTH
#define H               TFT_DISPLAY_HEIGHT
#define MARGIN          40      // corner targets, in from the edges
#define N_CORNERS       4
#define N_POINTS        (N_CORNERS + 1)
#define SAMPLES         8       // raw readings averaged per target
#define RELEASE_MS      150
#define IDLE_TIMEOUT_MS 30000
#define DONE_MS         2000
#define MAX_ERROR       20
#define CROSS           40      // target size

typedef enum {
    Cal_WaitPress,
    Cal_Sampling,
    Cal_WaitRelease,
    Cal_Done
} cal_step_t;

// Top left, top right, bottom left, bottom right, center
static const lv_coord_t target_x[N_POINTS] = { MARGIN, W - MARGIN, MARGIN, W - MARGIN, W / 2 };
static const lv_coord_t target_y[N_POINTS] = { MARGIN, MARGIN, H - MARGIN, H - MARGIN, H / 2 };

static lv_style_t s_title, s_status, s_cross, s_cross_done;
static lv_obj_t *lbl_status, *cross_h, *cross_v;
static char txt_status[64];

static cal_step_t step;
static uint_fast8_t point, n_samples;
static uint32_t sum_x, sum_y, t_mark;
static uint16_t raw_x[N_POINTS], raw_y[N_POINTS];
static tft_touch_cal_t pending;

static void styles_init (void)
{
    lv_style_copy(&s_title, &lv_style_plain);
    s_title.text.font = UI_FONT_LARGE;
    s_title.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_status, &lv_style_plain);
    s_status.text.font = UI_FONT_MEDIUM;
    s_status.text.color = UI_COLOR_TEXT_DIM;

    lv_style_copy(&s_cross, &lv_style_plain);
    s_cross.body.main_color = s_cross.body.grad_color = UI_COLOR_DANGER;
    s_cross.body.radius = 0;

    lv_style_copy(&s_cross_done, &s_cross);
    s_cross_done.body.main_color = s_cross_done.body.grad_color = UI_COLOR_SUCCESS;
}

static void set_status (const char *text)
{
    ui_widget_set_text(lbl_status, txt_status, sizeof(txt_status), text);
}

static void show_target (uint_fast8_t idx, bool done)
{
    lv_style_t *style = done ? &s_cross_done : &s_cross;

    lv_obj_set_style(cross_h, style);
    lv_obj_set_style(cross_v, style);
    lv_obj_set_pos(cross_h, target_x[idx] - CROSS / 2, target_y[idx] - 1);
    lv_obj_set_pos(cross_v, target_x[idx] - 1, target_y[idx] - CROSS / 2);
}

static void restart (const char *msg)
{
    point = 0;
    step = Cal_WaitPress;
    t_mark = lv_tick_get();
    show_target(0, false);
    set_status(msg);
}

static void finish (const char *msg)
{
    step = Cal_Done;
    t_mark = lv_tick_get();
    lv_obj_set_hidden(cross_h, true);
    lv_obj_set_hidden(cross_v, true);
    set_status(msg);
}

// Fit the corner readings: which raw axis follows screen x, and the raw value
// at each screen edge, extrapolated out from the targets.
static bool compute (tft_touch_cal_t *cal)
{
    int32_t a[N_CORNERS], b[N_CORNERS];
    int32_t moved_x = abs(raw_x[1] - raw_x[0]) + abs(raw_x[3] - raw_x[2]);
    int32_t moved_y = abs(raw_y[1] - raw_y[0]) + abs(raw_y[3] - raw_y[2]);

    cal->swap = moved_y > moved_x;

    for(uint_fast8_t idx = 0; idx < N_CORNERS; idx++) {
        a[idx] = cal->swap ? raw_y[idx] : raw_x[idx];
        b[idx] = cal->swap ? raw_x[idx] : raw_y[idx];
    }

    int32_t left = (a[0] + a[2]) / 2, right = (a[1] + a[3]) / 2;
    int32_t top = (b[0] + b[1]) / 2, bottom = (b[2] + b[3]) / 2;
    int32_t ext_x = (right - left) * MARGIN / (W - 2 * MARGIN);
    int32_t ext_y = (bottom - top) * MARGIN / (H - 2 * MARGIN);

    cal->x0 = left - ext_x;
    cal->x1 = right + ext_x;
    cal->y0 = top - ext_y;
    cal->y1 = bottom + ext_y;

    return abs(cal->x1 - cal->x0) > 500 && abs(cal->y1 - cal->y0) > 500;
}

static void next_point (void)
{
    char buf[64];

    point++;

    if(point == N_CORNERS) {
        if(compute(&pending)) {
            show_target(point, false);
            set_status("Now tap the center");
        } else
            restart("Could not read the corners, try again");

    } else if(point == N_POINTS) {
        int32_t x, y;

        tft_touch_convert(&pending, raw_x[N_CORNERS], raw_y[N_CORNERS], &x, &y);
        int32_t err = abs(x - target_x[N_CORNERS]);
        if(abs(y - target_y[N_CORNERS]) > err)
            err = abs(y - target_y[N_CORNERS]);

        tft_log("cal x %d..%d y %d..%d swap %u err %d", pending.x0, pending.x1,
                 pending.y0, pending.y1, (unsigned)pending.swap, (int)err);

        if(err > MAX_ERROR) {
            snprintf(buf, sizeof(buf), "Center was off by %d px, try again", (int)err);
            restart(buf);
        } else
            finish(tft_touch_set_calibration(&pending) ? "Calibration saved" : "Applied, but saving failed");

    } else {
        show_target(point, false);
        set_status("");
    }
}

static void screen_create (lv_obj_t *parent)
{
    styles_init();
    lv_obj_set_style(parent, ui_widget_screen_style());

    lv_obj_t *title = ui_widget_create_styled_label(parent, 0, 0, &s_title, "Touch calibration");
    lv_obj_align(title, NULL, LV_ALIGN_CENTER, 0, -70);

    lv_obj_t *hint = ui_widget_create_styled_label(parent, 0, 0, &s_status, "Tap and hold the center of each cross");
    lv_obj_align(hint, NULL, LV_ALIGN_CENTER, 0, -40);

    lbl_status = ui_widget_create_styled_label(parent, 0, 0, &s_status, "");
    lv_obj_set_auto_realign(lbl_status, true);
    lv_obj_align(lbl_status, NULL, LV_ALIGN_CENTER, 0, 50);
    txt_status[0] = '\0';

    cross_h = lv_obj_create(parent, NULL);
    lv_obj_set_size(cross_h, CROSS, 3);
    cross_v = lv_obj_create(parent, NULL);
    lv_obj_set_size(cross_v, 3, CROSS);
}

static void screen_show (void)
{
    tft_touch_set_calibrating(true);
    lv_obj_set_hidden(cross_h, false);
    lv_obj_set_hidden(cross_v, false);
    restart("");
}

static void screen_hide (void)
{
    tft_touch_set_calibrating(false);
}

static void screen_update (void)
{
    uint16_t rx, ry;
    bool pressed = step != Cal_Done && tft_touch_read_raw(&rx, &ry, TOUCH_Z_THRESHOLD_CAL);

    switch(step) {

        case Cal_WaitPress:
            if(pressed) {
                sum_x = rx;
                sum_y = ry;
                n_samples = 1;
                step = Cal_Sampling;
            } else if(lv_tick_elaps(t_mark) > IDLE_TIMEOUT_MS)
                finish("Timed out, calibration unchanged");
            break;

        case Cal_Sampling:
            if(!pressed) {
                // Let go too early, take this target again
                step = Cal_WaitPress;
                t_mark = lv_tick_get();
            } else {
                sum_x += rx;
                sum_y += ry;
                if(++n_samples == SAMPLES) {
                    raw_x[point] = sum_x / SAMPLES;
                    raw_y[point] = sum_y / SAMPLES;
                    show_target(point, true);
                    step = Cal_WaitRelease;
                    t_mark = lv_tick_get();
                }
            }
            break;

        case Cal_WaitRelease:
            if(pressed)
                t_mark = lv_tick_get();
            else if(lv_tick_elaps(t_mark) > RELEASE_MS) {
                step = Cal_WaitPress;
                next_point();
                if(step == Cal_WaitPress)
                    t_mark = lv_tick_get();
            }
            break;

        case Cal_Done:
            if(lv_tick_elaps(t_mark) > DONE_MS)
                ui_screen_manager_show(SCREEN_MAIN);
            break;
    }
}

void screen_calibrate_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = screen_hide,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_CALIBRATE, &callbacks);
}

#endif // TFT_ENABLE
