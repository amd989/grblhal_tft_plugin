/*
 * ui_chrome.c - Status bar, tabs and action bar shared by the screens
 *
 * Part of grblHAL TFT Plugin
 *
 * See ui_chrome.h. Layout (480x320):
 *   status bar  y 0-36: state pill, reason or position, spindle, host and Wi-Fi
 *   tabs        x 0-56, five of 56 px
 *   action bar  x 412-476: Hold/Resume 112 px tall, Home, Unlock, Reset
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "ui_chrome.h"
#include "ui_screen_manager.h"
#include "ui_widgets.h"
#include "ui_icons.h"
#include "tft_interface.h"

#define UPDATE_MS   100
#define N_TABS      5

static const struct {
    ui_screen_t screen;
    const char *icon, *text;
} tabs[N_TABS] = {
    { SCREEN_CONTROL, LV_SYMBOL_GPS,       "Jog" },
    { SCREEN_FILES,   LV_SYMBOL_DIRECTORY, "Files" },
    { SCREEN_PROBE,   LV_SYMBOL_DOWNLOAD,  "Probe" },
    { SCREEN_MACROS,  LV_SYMBOL_LIST,      "Macros" },
    { SCREEN_TOOL,    LV_SYMBOL_SETTINGS,  "Tools" },
};

static lv_style_t s_bar, s_pill[4], s_slash_gap, s_slash, s_host;
static lv_obj_t *status_bar, *pill, *lbl_pill, *lbl_mid, *btn_spindle, *lbl_net, *img_slash_gap, *img_slash, *img_host;
static lv_obj_t *tab_bar, *btn_tab[N_TABS], *lbl_tab_icon[N_TABS];
static lv_obj_t *action_bar, *btn_hold, *lbl_hold_icon, *lbl_hold, *btn_home, *btn_unlock;
static ui_confirm_btn_t reset_btn;

static ui_screen_t screen = SCREEN_NONE;
static uint32_t last_update;
static char txt_pill[24], txt_mid[64];
static int8_t pill_shown = -1, hold_shown = -1, unlock_shown = -1, spindle_shown = -1, net_shown = -1, host_shown = -1;

static bool tab_screen (ui_screen_t s)
{
    for(uint_fast8_t idx = 0; idx < N_TABS; idx++) {
        if(tabs[idx].screen == s)
            return true;
    }

    return false;
}

static bool held (sys_state_t state)
{
    return !!(state & (STATE_HOLD|STATE_SAFETY_DOOR|STATE_TOOL_CHANGE));
}

/*
 * Events
 */

static void on_tab (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    for(uint_fast8_t idx = 0; idx < N_TABS; idx++) {
        if(obj == btn_tab[idx] && tabs[idx].screen != screen) {
            tft_log("tab %s", tabs[idx].text);
            ui_screen_manager_show(tabs[idx].screen);
        }
    }
}

static void on_action (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    sys_state_t state = ui_state.system_state;

    if(obj == btn_hold) {
        tft_log("btn hold");
        if(held(state))
            tft_resume();
        else
            tft_feed_hold();
    } else if(obj == btn_home) {
        tft_log("btn home");
        tft_home_all();
    } else if(obj == btn_unlock) {
        tft_log("btn unlock");
        tft_unlock();
    } else if(obj == btn_spindle) {
        tft_log("badge spindle off");
        tft_spindle(false);
    }

    last_update = 0;
}

static void do_reset (void)
{
    tft_log("btn reset");
    tft_reset();
}

/*
 * Creation
 */

static lv_obj_t *stacked_button (lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                 const char *icon, const lv_font_t *icon_font, lv_obj_t **icon_label,
                                 const char *text, lv_obj_t **text_label, ui_button_style_t style, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent, NULL), *lbl;

    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_btn_set_layout(btn, LV_LAYOUT_CENTER);
    ui_widget_set_button_style(btn, style);
    lv_obj_set_event_cb(btn, cb);

    lbl = lv_label_create(btn, NULL);
    lv_label_set_text(lbl, icon);
    if(icon_font) {
        // Own font, colored like the button text by repaint_icon()
        lv_label_set_style(lbl, LV_LABEL_STYLE_MAIN, ui_widget_text_style(icon_font, UI_COLOR_TEXT));
    }
    if(icon_label)
        *icon_label = lbl;

    if(text) {
        lbl = lv_label_create(btn, NULL);
        lv_label_set_text(lbl, text);
        if(text_label)
            *text_label = lbl;
    }

    return btn;
}

static void repaint_icon (lv_obj_t *icon, const lv_font_t *font, ui_button_style_t style)
{
    lv_color_t color = style == BUTTON_STYLE_SUCCESS || style == BUTTON_STYLE_WARNING ? UI_COLOR_BACKGROUND :
                       style == BUTTON_STYLE_TAB ? UI_COLOR_TEXT_DIM : UI_COLOR_TEXT;

    lv_label_set_style(icon, LV_LABEL_STYLE_MAIN, ui_widget_text_style(font, color));
}

void ui_chrome_init (void)
{
    lv_obj_t *top = lv_layer_top();

    lv_style_copy(&s_bar, ui_widget_screen_style());
    s_bar.body.main_color = UI_COLOR_SURFACE;
    s_bar.body.grad_color = UI_COLOR_SURFACE;

    static const lv_color_t pill_color[4] = { UI_COLOR_SUCCESS, UI_COLOR_PRIMARY, UI_COLOR_WARNING, UI_COLOR_DANGER };
    for(uint_fast8_t idx = 0; idx < 4; idx++) {
        lv_style_copy(&s_pill[idx], &s_bar);
        s_pill[idx].body.main_color = pill_color[idx];
        s_pill[idx].body.grad_color = pill_color[idx];
        s_pill[idx].body.radius = 4;
        s_pill[idx].text.font = UI_FONT_MEDIUM;
        s_pill[idx].text.color = idx == 0 || idx == 2 ? UI_COLOR_BACKGROUND : UI_COLOR_TEXT;
    }

    // Status bar
    status_bar = lv_obj_create(top, NULL);
    lv_obj_set_style(status_bar, &s_bar);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_size(status_bar, TFT_DISPLAY_WIDTH, UI_STATUS_H);
    lv_obj_set_click(status_bar, false);

    pill = lv_obj_create(status_bar, NULL);
    lv_obj_set_style(pill, &s_pill[0]);
    lv_obj_set_pos(pill, 4, 4);
    lv_obj_set_size(pill, 104, 28);
    lv_obj_set_click(pill, false);
    lbl_pill = lv_label_create(pill, NULL);
    lv_label_set_text(lbl_pill, "");
    lv_obj_set_auto_realign(lbl_pill, true);
    lv_obj_align(lbl_pill, NULL, LV_ALIGN_CENTER, 0, 0);

    lbl_mid = ui_widget_create_styled_label(status_bar, 116, 10, ui_widget_text_style(UI_FONT_MEDIUM, UI_COLOR_TEXT), "");
    lv_label_set_long_mode(lbl_mid, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_mid, 262);

    btn_spindle = ui_widget_create_button(status_bar, 382, 4, 34, 28, LV_SYMBOL_REFRESH, BUTTON_STYLE_SUCCESS, on_action);
    lv_obj_set_hidden(btn_spindle, true);

    lbl_net = ui_widget_create_styled_label(status_bar, 444, 8, ui_widget_text_style(UI_FONT_MEDIUM, UI_COLOR_TEXT), LV_SYMBOL_WIFI);
    lv_obj_set_hidden(lbl_net, true);

    // Wi-Fi down: a slash across the gray symbol, with a gap cut around it
    lv_style_copy(&s_slash_gap, &lv_style_plain);
    s_slash_gap.image.color = UI_COLOR_SURFACE;
    lv_style_copy(&s_slash, &s_slash_gap);
    s_slash.image.color = UI_COLOR_TEXT_DIM;
    img_slash_gap = lv_img_create(status_bar, NULL);
    lv_img_set_src(img_slash_gap, &ui_slash_gap);
    lv_img_set_style(img_slash_gap, LV_IMG_STYLE_MAIN, &s_slash_gap);
    lv_obj_align(img_slash_gap, lbl_net, LV_ALIGN_CENTER, 0, 0);
    img_slash = lv_img_create(status_bar, NULL);
    lv_img_set_src(img_slash, &ui_slash);
    lv_img_set_style(img_slash, LV_IMG_STYLE_MAIN, &s_slash);
    lv_obj_align(img_slash, lbl_net, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_hidden(img_slash_gap, true);
    lv_obj_set_hidden(img_slash, true);

    // Host: a PC is talking to the machine (see tft_logic_host_active())
    lv_style_copy(&s_host, &lv_style_plain);
    s_host.image.color = UI_COLOR_TEXT;
    img_host = lv_img_create(status_bar, NULL);
    lv_img_set_src(img_host, &ui_host);
    lv_img_set_style(img_host, LV_IMG_STYLE_MAIN, &s_host);
    lv_obj_set_pos(img_host, 420, 9);
    lv_obj_set_hidden(img_host, true);

    // Tabs
    tab_bar = lv_obj_create(top, NULL);
    lv_obj_set_style(tab_bar, &s_bar);
    lv_obj_set_pos(tab_bar, 0, UI_STATUS_H);
    lv_obj_set_size(tab_bar, UI_TABS_W, TFT_DISPLAY_HEIGHT - UI_STATUS_H);
    lv_obj_set_click(tab_bar, false);

    for(uint_fast8_t idx = 0; idx < N_TABS; idx++)
        btn_tab[idx] = stacked_button(tab_bar, 0, idx * 57, UI_TABS_W, 56, tabs[idx].icon, UI_FONT_LARGE,
                                      &lbl_tab_icon[idx], tabs[idx].text, NULL, BUTTON_STYLE_TAB, on_tab);

    // Action bar, on the screen background
    action_bar = lv_obj_create(top, NULL);
    lv_obj_set_style(action_bar, ui_widget_screen_style());
    lv_obj_set_pos(action_bar, UI_ACTION_X - 4, UI_STATUS_H);
    lv_obj_set_size(action_bar, TFT_DISPLAY_WIDTH - UI_ACTION_X + 4, TFT_DISPLAY_HEIGHT - UI_STATUS_H);
    lv_obj_set_click(action_bar, false);

    btn_hold = stacked_button(action_bar, 4, 4, 64, 112, LV_SYMBOL_PAUSE, UI_FONT_XLARGE, &lbl_hold_icon,
                              "Hold", &lbl_hold, BUTTON_STYLE_DEFAULT, on_action);
    btn_home = stacked_button(action_bar, 4, 120, 64, 50, LV_SYMBOL_HOME, UI_FONT_LARGE, NULL,
                              NULL, NULL, BUTTON_STYLE_PRIMARY, on_action);
    btn_unlock = ui_widget_create_button(action_bar, 4, 174, 64, 50, "Unlock", BUTTON_STYLE_DEFAULT, on_action);
    ui_confirm_btn_create(&reset_btn, action_bar, 4, 228, 64, 52, "Reset", BUTTON_STYLE_OUTLINE_DANGER, do_reset);

    lv_obj_set_hidden(status_bar, true);
    lv_obj_set_hidden(tab_bar, true);
    lv_obj_set_hidden(action_bar, true);
}

/*
 * Screen changes
 */

void ui_chrome_set_screen (ui_screen_t s)
{
    bool tabbed = tab_screen(s);

    screen = s;

    lv_obj_set_hidden(status_bar, s == SCREEN_CALIBRATE || s == SCREEN_SPLASH);
    lv_obj_set_hidden(tab_bar, !tabbed);
    // Files has the room instead: nothing moves there but Frame, and its
    // panel turns Cancel into Stop while it does
    lv_obj_set_hidden(action_bar, !tabbed || s == SCREEN_FILES);

    for(uint_fast8_t idx = 0; idx < N_TABS; idx++) {
        ui_button_style_t style = tabs[idx].screen == s ? BUTTON_STYLE_TAB_SELECTED : BUTTON_STYLE_TAB;
        ui_widget_set_button_style(btn_tab[idx], style);
        repaint_icon(lbl_tab_icon[idx], UI_FONT_LARGE, style == BUTTON_STYLE_TAB_SELECTED ? BUTTON_STYLE_DEFAULT : BUTTON_STYLE_TAB);
    }

    ui_confirm_btn_cancel(&reset_btn);
    txt_mid[0] = '\0';
    last_update = 0;
}

/*
 * Updates
 */

static uint_fast8_t pill_kind (sys_state_t state)
{
    if(state == STATE_IDLE)
        return 0;
    if(state & (STATE_ALARM|STATE_ESTOP))
        return 3;
    if(state & (STATE_HOLD|STATE_SAFETY_DOOR|STATE_TOOL_CHANGE|STATE_CHECK_MODE|STATE_SLEEP))
        return 2;

    return 1;   // cycle, jog, homing
}

static void middle_text (char *buf, size_t size, sys_state_t state, lv_color_t *color)
{
    *color = UI_COLOR_TEXT;

    if(state & STATE_TOOL_CHANGE) {
        ui_widget_format_tool_change(buf, size);
        *color = UI_COLOR_WARNING;
    } else if(state & (STATE_ALARM|STATE_ESTOP)) {
        snprintf(buf, size, "%s", ui_state_get_alarm_string());
        *color = UI_COLOR_DANGER_TEXT;
    } else if(state & STATE_SAFETY_DOOR) {
        snprintf(buf, size, "Door open, close it and Resume");
        *color = UI_COLOR_WARNING;
    } else if(state & STATE_HOLD) {
        snprintf(buf, size, "Feed hold, Resume to continue");
        *color = UI_COLOR_WARNING;
    } else if(screen == SCREEN_HOME) {
        // Jog has its own large position, show the address instead. Nothing
        // when the network is down, the struck Wi-Fi symbol says so.
        snprintf(buf, size, "%s", ui_state.net_ip);
        *color = UI_COLOR_TEXT_DIM;
    } else {
        float *pos = ui_state.coord_mode == COORD_MODE_WORK ? ui_state.work_position : ui_state.machine_position;
        float v[3] = { 0.0f, 0.0f, 0.0f };
        for(uint_fast8_t idx = 0; idx < 3 && idx < N_AXIS; idx++)
            v[idx] = fabsf(pos[idx]) < 0.0005f ? 0.0f : pos[idx];
        snprintf(buf, size, "X %.3f  Y %.3f  Z %.3f", v[0], v[1], v[2]);
    }
}

void ui_chrome_update (void)
{
    if(lv_obj_get_hidden(status_bar))
        return;

    ui_confirm_btn_update(&reset_btn);

    if(last_update && lv_tick_elaps(last_update) < UPDATE_MS)
        return;

    last_update = lv_tick_get();
    if(last_update == 0)
        last_update = 1;

    sys_state_t state = ui_state.system_state;
    char buf[64];
    lv_color_t color;

    // State pill
    uint_fast8_t kind = pill_kind(state);
    if(kind != (uint_fast8_t)pill_shown) {
        pill_shown = kind;
        lv_obj_set_style(pill, &s_pill[kind]);
    }
    ui_widget_format_state(buf, sizeof(buf));
    ui_widget_set_text(lbl_pill, txt_pill, sizeof(txt_pill), buf);

    // Reason, network or position
    middle_text(buf, sizeof(buf), state, &color);
    if(strcmp(buf, txt_mid)) {
        lv_label_set_style(lbl_mid, LV_LABEL_STYLE_MAIN, ui_widget_text_style(UI_FONT_MEDIUM, color));
        ui_widget_set_text(lbl_mid, txt_mid, sizeof(txt_mid), buf);
    }

    // Spindle badge: tap to turn it off, not during a job
    int8_t spindle = ui_state.spindle_speed > 0.0f && !ui_state.laser_mode && screen != SCREEN_JOB;
    if(spindle != spindle_shown) {
        spindle_shown = spindle;
        lv_obj_set_hidden(btn_spindle, !spindle);
    }

    // Host connected or polling
    int8_t host = ui_state.host_connected;
    if(host != host_shown) {
        host_shown = host;
        lv_obj_set_hidden(img_host, !host);
    }

    // Network: white when connected, gray and struck through when not,
    // hidden without networking
    int8_t net = ui_state.net_link;
    if(net != net_shown) {
        net_shown = net;
        lv_obj_set_hidden(lbl_net, net == NET_LINK_NONE);
        lv_obj_set_hidden(img_slash_gap, net != NET_LINK_DOWN);
        lv_obj_set_hidden(img_slash, net != NET_LINK_DOWN);
        lv_label_set_style(lbl_net, LV_LABEL_STYLE_MAIN, ui_widget_text_style(UI_FONT_MEDIUM, net == NET_LINK_UP ? UI_COLOR_TEXT : UI_COLOR_TEXT_DIM));
    }

    if(lv_obj_get_hidden(action_bar))
        return;

    // Hold, Resume (green, orange in a tool change), or Hold doing nothing
    int8_t hold = state & STATE_TOOL_CHANGE ? 3 : held(state) ? 2 : (state & (STATE_CYCLE|STATE_JOG|STATE_HOMING)) ? 1 : 0;
    if(hold != hold_shown) {
        ui_button_style_t style = hold == 3 ? BUTTON_STYLE_WARNING : hold == 2 ? BUTTON_STYLE_SUCCESS : hold == 1 ? BUTTON_STYLE_WARNING : BUTTON_STYLE_DEFAULT;
        hold_shown = hold;
        ui_widget_set_button_style(btn_hold, style);
        lv_label_set_text(lbl_hold_icon, hold >= 2 ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
        lv_label_set_text(lbl_hold, hold >= 2 ? "Resume" : "Hold");
        repaint_icon(lbl_hold_icon, UI_FONT_XLARGE, style);
    }

    // Unlock: orange in an alarm
    int8_t unlock = !!(state & STATE_ALARM);
    if(unlock != unlock_shown) {
        unlock_shown = unlock;
        ui_widget_set_button_style(btn_unlock, unlock ? BUTTON_STYLE_WARNING : BUTTON_STYLE_DEFAULT);
    }
}

#endif // TFT_ENABLE
