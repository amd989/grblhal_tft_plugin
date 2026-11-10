/*
 * screen_macros.c - Macro buttons
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320):
 *   top     Back, title
 *   middle  six macro buttons, two columns, from settings $450-$455
 *   bottom  status line, or how to set them up
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

#define BTN_W       222
#define BTN_H       56
#define BTN_X(c)    (12 + (c) * 234)
#define BTN_Y(r)    (64 + (r) * 64)
#define LABEL_LEN   24
#define UPDATE_MS   500

static lv_style_t s_title, s_text;
static lv_obj_t *btn_macro[TFT_N_MACROS], *lbl_macro[TFT_N_MACROS], *lbl_status, *lbl_state;
static char txt_macro[TFT_N_MACROS][LABEL_LEN], txt_state[24];
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

static void on_back (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED)
        ui_screen_manager_show(SCREEN_MAIN);
}

static void screen_create (lv_obj_t *parent)
{
    lv_style_copy(&s_title, &lv_style_plain);
    s_title.text.font = UI_FONT_LARGE;
    s_title.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_text, &lv_style_plain);
    s_text.text.font = UI_FONT_MEDIUM;
    s_text.text.color = UI_COLOR_TEXT_DIM;

    lv_obj_set_style(parent, ui_widget_screen_style());

    ui_widget_create_button(parent, 12, 8, 90, 44, LV_SYMBOL_LEFT " Back", BUTTON_STYLE_DEFAULT, on_back);
    ui_widget_create_styled_label(parent, 116, 18, &s_title, "Macros");
    lbl_state = ui_widget_create_styled_label(parent, 300, 18, ui_widget_state_style(STATE_IDLE), "");
    txt_state[0] = '\0';

    for(uint_fast8_t idx = 0; idx < TFT_N_MACROS; idx++) {
        btn_macro[idx] = ui_widget_create_button(parent, BTN_X(idx % 2), BTN_Y(idx / 2), BTN_W, BTN_H, "", BUTTON_STYLE_DEFAULT, on_macro);
        lbl_macro[idx] = lv_obj_get_child(btn_macro[idx], NULL);
        txt_macro[idx][0] = '\0';
    }

    lbl_status = ui_widget_create_styled_label(parent, 16, 262, &s_text, "");
    ui_widget_create_styled_label(parent, 16, 290, &s_text, "Set with $450-$455 = Label|G-code|G-code");
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

    char buf[24];

    ui_widget_format_state(buf, sizeof(buf));
    if(strcmp(buf, txt_state)) {
        lv_label_set_style(lbl_state, LV_LABEL_STYLE_MAIN, ui_widget_state_style(ui_state.system_state));
        ui_widget_set_text(lbl_state, txt_state, sizeof(txt_state), buf);
    }

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
