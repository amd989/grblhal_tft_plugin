/*
 * screen_macros.c - Macro buttons
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (content area, between the tabs and the action bar):
 *   top     six macro buttons, two columns, from settings $450-$455
 *   bottom  status line, and how to set them up
 *
 * A macro runs only while the machine is idle. Its lines are queued and
 * each waits for the previous one to finish, see tft_send_lines().
 * Runs in the UI task only.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>

#include "screen_macros.h"
#include "ui_screen_manager.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_interface.h"
#include "tft_macros.h"

#define BTN_W       170
#define BTN_H       70
#define BTN_X(c)    (UI_CONTENT_X + (c) * 176)
#define BTN_Y(r)    (UI_CONTENT_Y + (r) * 76)
#define LABEL_LEN   24
#define UPDATE_MS   500

static lv_obj_t *btn_macro[TFT_N_MACROS], *lbl_macro[TFT_N_MACROS], *lbl_status;
static char txt_macro[TFT_N_MACROS][LABEL_LEN];
static uint32_t last_update;

static void refresh_labels (void)
{
    char label[LABEL_LEN];

    for(uint_fast8_t idx = 0; idx < TFT_N_MACROS; idx++) {
        bool used = tft_macro_label(idx, label, sizeof(label));
        lv_obj_set_hidden(btn_macro[idx], !used);
        if(used)
            ui_widget_set_text(lbl_macro[idx], txt_macro[idx], sizeof(txt_macro[idx]), label);
    }
}

static void on_macro (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    for(uint_fast8_t idx = 0; idx < TFT_N_MACROS; idx++) {
        if(obj == btn_macro[idx]) {
            char buf[48];
            if(ui_state.system_state != STATE_IDLE)
                lv_label_set_text(lbl_status, "Machine must be idle");
            else if(tft_macro_run(idx)) {
                tft_log("btn macro %u", (unsigned)idx + 1);
                snprintf(buf, sizeof(buf), "Running %s", txt_macro[idx]);
                lv_label_set_text(lbl_status, buf);
            } else
                lv_label_set_text(lbl_status, "No G-code in this macro");
            break;
        }
    }
}

static void screen_create (lv_obj_t *parent)
{
    lv_obj_set_style(parent, ui_widget_screen_style());

    for(uint_fast8_t idx = 0; idx < TFT_N_MACROS; idx++) {
        btn_macro[idx] = ui_widget_create_button(parent, BTN_X(idx % 2), BTN_Y(idx / 2), BTN_W, BTN_H, "", BUTTON_STYLE_DEFAULT, on_macro);
        lbl_macro[idx] = lv_obj_get_child(btn_macro[idx], NULL);
        lv_label_set_long_mode(lbl_macro[idx], LV_LABEL_LONG_BREAK);
        lv_label_set_align(lbl_macro[idx], LV_LABEL_ALIGN_CENTER);
        lv_obj_set_width(lbl_macro[idx], BTN_W - 12);
        txt_macro[idx][0] = '\0';
    }

    lbl_status = ui_widget_create_styled_label(parent, UI_CONTENT_X + 4, 272, ui_widget_text_style(UI_FONT_MEDIUM, UI_COLOR_TEXT), "");
    ui_widget_create_styled_label(parent, UI_CONTENT_X + 4, 298, ui_widget_text_style(UI_FONT_SMALL, UI_COLOR_TEXT_DIM),
                                  "Set with $450-$455 = Label|G-code|G-code");
}

static void screen_show (void)
{
    lv_label_set_text(lbl_status, "");
    refresh_labels();
    last_update = 0;
}

static void screen_update (void)
{
    if(last_update && lv_tick_elaps(last_update) < UPDATE_MS)
        return;

    last_update = lv_tick_get();
    if(last_update == 0)
        last_update = 1;

    refresh_labels();   // picks up $45x changes from a console
}

void screen_macros_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = NULL,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_MACROS, &callbacks);
}

#endif // TFT_ENABLE
