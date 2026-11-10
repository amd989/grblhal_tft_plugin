/*
 * screen_settings.c - Brightness, touch beep, touch calibration and info
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320):
 *   top     Back, title, state
 *   middle  brightness slider, touch beep On/Off, Calibrate touch
 *   bottom  network, versions, screen memory
 *
 * Brightness and beep are grblHAL settings $456 and $457 (tft_settings.c),
 * so a console can change them too; the screen follows. The slider sets the
 * backlight while it is dragged and saves the value when released.
 * Runs in the UI task only.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>

#include "grbl/grbl.h"

#include "screen_settings.h"
#include "ui_screen_manager.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_plugin.h"
#include "tft_driver.h"
#include "tft_interface.h"
#include "tft_settings.h"
#include "tft_touch.h"

#define UPDATE_MS   500
#define ROW_X       160

static lv_style_t s_title, s_label, s_text, s_slider_bg, s_slider_indic, s_slider_knob;
static lv_obj_t *lbl_state, *slider, *lbl_brightness, *btn_beep, *lbl_beep, *lbl_status;
static lv_obj_t *lbl_net, *lbl_version, *lbl_mem;
static char txt_state[24], txt_brightness[8], txt_net[96], txt_mem[48];
static uint32_t last_update;
static int8_t beep_shown = -1;

static void show_brightness (uint8_t percent)
{
    char buf[8];

    snprintf(buf, sizeof(buf), "%u%%", (unsigned)percent);
    ui_widget_set_text(lbl_brightness, txt_brightness, sizeof(txt_brightness), buf);
}

static void show_beep (void)
{
    bool on = tft_settings_beep();

    if(beep_shown == (int8_t)on)
        return;

    beep_shown = on;
    lv_label_set_text(lbl_beep, on ? "On" : "Off");
    ui_widget_set_button_style(btn_beep, on ? BUTTON_STYLE_SUCCESS : BUTTON_STYLE_DEFAULT);
}

static void on_slider (lv_obj_t *obj, lv_event_t event)
{
    uint8_t percent = (uint8_t)lv_slider_get_value(obj);

    if(event == LV_EVENT_VALUE_CHANGED) {
        show_brightness(percent);
        tft_set_backlight((uint8_t)((uint32_t)percent * 255 / 100));
    } else if(event == LV_EVENT_RELEASED || event == LV_EVENT_PRESS_LOST) {
        tft_log("brightness %u", (unsigned)percent);
        tft_settings_set_brightness(percent);
    }
}

static void on_beep (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    tft_settings_set_beep(!tft_settings_beep());
    tft_log("beep %s", tft_settings_beep() ? "on" : "off");
    show_beep();
}

static void on_calibrate (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    // Saving the calibration to flash stalls both cores, as for $TFTCAL
    if(ui_state.system_state == STATE_IDLE || (ui_state.system_state & STATE_ALARM)) {
        tft_log("btn calibrate");
        tft_touch_request_calibration();
    } else
        lv_label_set_text(lbl_status, "Machine must be idle");
}

static void on_back (lv_obj_t *obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED)
        ui_screen_manager_show(SCREEN_MAIN);
}

static void styles_init (void)
{
    lv_style_copy(&s_title, &lv_style_plain);
    s_title.text.font = UI_FONT_LARGE;
    s_title.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_label, &lv_style_plain);
    s_label.text.font = UI_FONT_MEDIUM;
    s_label.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_text, &lv_style_plain);
    s_text.text.font = UI_FONT_MEDIUM;
    s_text.text.color = UI_COLOR_TEXT_DIM;

    lv_style_copy(&s_slider_bg, &lv_style_plain);
    s_slider_bg.body.main_color = LV_COLOR_MAKE(0x50, 0x50, 0x50);
    s_slider_bg.body.grad_color = s_slider_bg.body.main_color;
    s_slider_bg.body.radius = LV_RADIUS_CIRCLE;
    s_slider_bg.body.padding.left = s_slider_bg.body.padding.right = 0;
    s_slider_bg.body.padding.top = s_slider_bg.body.padding.bottom = 0;

    lv_style_copy(&s_slider_indic, &s_slider_bg);
    s_slider_indic.body.main_color = UI_COLOR_PRIMARY;
    s_slider_indic.body.grad_color = UI_COLOR_PRIMARY;

    lv_style_copy(&s_slider_knob, &s_slider_bg);
    s_slider_knob.body.main_color = UI_COLOR_TEXT;
    s_slider_knob.body.grad_color = UI_COLOR_TEXT;
    s_slider_knob.body.padding.left = s_slider_knob.body.padding.right = 6;
    s_slider_knob.body.padding.top = s_slider_knob.body.padding.bottom = 6;
}

static void screen_create (lv_obj_t *parent)
{
    char buf[64];

    styles_init();
    lv_obj_set_style(parent, ui_widget_screen_style());

    ui_widget_create_button(parent, 12, 8, 90, 44, LV_SYMBOL_LEFT " Back", BUTTON_STYLE_DEFAULT, on_back);
    ui_widget_create_styled_label(parent, 116, 18, &s_title, "Settings");
    lbl_state = ui_widget_create_styled_label(parent, 300, 18, ui_widget_state_style(STATE_IDLE), "");
    txt_state[0] = '\0';

    // Brightness
    ui_widget_create_styled_label(parent, 16, 82, &s_label, "Brightness");
    slider = lv_slider_create(parent, NULL);
    lv_obj_set_pos(slider, ROW_X, 80);
    lv_obj_set_size(slider, 230, 24);
    lv_slider_set_range(slider, TFT_BRIGHTNESS_MIN, 100);
    lv_slider_set_style(slider, LV_SLIDER_STYLE_BG, &s_slider_bg);
    lv_slider_set_style(slider, LV_SLIDER_STYLE_INDIC, &s_slider_indic);
    lv_slider_set_style(slider, LV_SLIDER_STYLE_KNOB, &s_slider_knob);
    lv_obj_set_event_cb(slider, on_slider);
    lbl_brightness = ui_widget_create_styled_label(parent, 410, 82, &s_label, "");
    txt_brightness[0] = '\0';

    // Touch beep
    ui_widget_create_styled_label(parent, 16, 138, &s_label, "Touch beep");
    btn_beep = ui_widget_create_button(parent, ROW_X, 124, 120, 48, "", BUTTON_STYLE_DEFAULT, on_beep);
    lbl_beep = lv_obj_get_child(btn_beep, NULL);
#if !TFT_BEEP_ENABLE
    lv_obj_set_hidden(btn_beep, true);
    ui_widget_create_styled_label(parent, ROW_X, 138, &s_text, "Not in this build");
#endif

    // Touch calibration
    ui_widget_create_styled_label(parent, 16, 198, &s_label, "Touch");
    ui_widget_create_button(parent, ROW_X, 184, 160, 48, "Calibrate", BUTTON_STYLE_DEFAULT, on_calibrate);
    lbl_status = ui_widget_create_styled_label(parent, 336, 198, &s_text, "");

    // Info
    lbl_net = ui_widget_create_styled_label(parent, 16, 246, &s_text, "");
    lv_label_set_long_mode(lbl_net, LV_LABEL_LONG_CROP);
    lv_obj_set_width(lbl_net, 448);
    txt_net[0] = '\0';

    snprintf(buf, sizeof(buf), "grblHAL %s.%u, TFT plugin v%s", GRBL_VERSION, (unsigned)GRBL_BUILD, TFT_PLUGIN_VERSION);
    lbl_version = ui_widget_create_styled_label(parent, 16, 270, &s_text, buf);

    lbl_mem = ui_widget_create_styled_label(parent, 16, 294, &s_text, "");
    txt_mem[0] = '\0';
}

static void refresh_info (void)
{
    char buf[96];   // hostname (32) + ".local, " + net status (39)
    lv_mem_monitor_t mon;

    if(*ui_state.hostname)
        snprintf(buf, sizeof(buf), "%s.local, %s", ui_state.hostname, ui_state.net_status);
    else
        snprintf(buf, sizeof(buf), "%s", *ui_state.net_status ? ui_state.net_status : "No network");
    ui_widget_set_text(lbl_net, txt_net, sizeof(txt_net), buf);

    lv_mem_monitor(&mon);
    snprintf(buf, sizeof(buf), "Screen memory %u of %u KB used",
             (unsigned)((mon.total_size - mon.free_size + 512) / 1024), (unsigned)(mon.total_size / 1024));
    ui_widget_set_text(lbl_mem, txt_mem, sizeof(txt_mem), buf);
}

static void screen_show (void)
{
    uint8_t percent = tft_settings_brightness();

    lv_slider_set_value(slider, percent, LV_ANIM_OFF);
    show_brightness(percent);
    beep_shown = -1;
    show_beep();
    lv_label_set_text(lbl_status, "");
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

    // Follow $456/$457 changes from a console
    uint8_t percent = tft_settings_brightness();
    if(!lv_slider_is_dragged(slider) && lv_slider_get_value(slider) != percent) {
        lv_slider_set_value(slider, percent, LV_ANIM_OFF);
        show_brightness(percent);
    }
    show_beep();

    refresh_info();
}

void screen_settings_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = NULL,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_SETTINGS, &callbacks);
}

#endif // TFT_ENABLE
