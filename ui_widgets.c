/*
 * ui_widgets.c - Reusable LVGL widget library implementation (LVGL v6 compatible)
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 */

#include "tft_config.h"    // first, so my_machine.h comes before grblHAL config.h
#include "ui_widgets.h"
#include "tft_logic.h"
#include "tft_settings.h"
#include <stdio.h>
#include <string.h>

// Button styles (LVGL v6)
static lv_style_t style_btn_default_rel, style_btn_default_pr;
static lv_style_t style_btn_primary_rel, style_btn_primary_pr;
static lv_style_t style_btn_success_rel, style_btn_success_pr;
static lv_style_t style_btn_warning_rel, style_btn_warning_pr;
static lv_style_t style_btn_danger_rel, style_btn_danger_pr;
static lv_style_t style_btn_outline_danger_rel;
static lv_style_t style_tab_rel, style_tab_pr, style_tab_sel;
static lv_style_t style_panel, style_scrim;

// Screen background
static lv_style_t style_screen;

// Machine state label styles
static lv_style_t style_state_idle;
static lv_style_t style_state_run;
static lv_style_t style_state_warn;
static lv_style_t style_state_alarm;

// Shared text styles, created on first use
#define N_TEXT_STYLES 16
static struct {
    const lv_font_t *font;
    lv_color_t color;
    lv_style_t style;
} text_styles[N_TEXT_STYLES];
static uint_fast8_t n_text_styles;

// Status bar components (stored for updates) - global storage for LVGL v6
typedef struct {
    lv_obj_t *state_label;
    lv_obj_t *position_label;
    lv_obj_t *feed_label;
    lv_obj_t *spindle_label;
} status_bar_components_t;

static status_bar_components_t *g_status_bar_components = NULL;

// Axis display components - global storage for LVGL v6
typedef struct {
    lv_obj_t *label;
    uint8_t axis;
} axis_display_data_t;

static axis_display_data_t g_axis_displays[6] = {0};  // Support up to 6 axes

static void fill (lv_style_t *style, const lv_style_t *from, lv_color_t body, lv_color_t border, lv_color_t text)
{
    lv_style_copy(style, from);
    style->body.main_color = body;
    style->body.grad_color = body;
    style->body.border.color = border;
    style->text.color = text;
    style->image.color = text;
    style->line.color = text;
}

/*
 * Initialize widget system (LVGL v6)
 */
void ui_widgets_init(void) {
    // Default button: dark fill, 2 px border, lighter when pressed. Lines
    // drawn on buttons (the diagonal jog arrows) take the text color.
    lv_style_copy(&style_btn_default_rel, &lv_style_btn_rel);
    style_btn_default_rel.body.border.width = 2;
    style_btn_default_rel.body.radius = 4;
    style_btn_default_rel.body.shadow.width = 0;
    style_btn_default_rel.body.opa = LV_OPA_COVER;
    style_btn_default_rel.body.border.opa = LV_OPA_COVER;
    style_btn_default_rel.body.padding.left = style_btn_default_rel.body.padding.right = 2;
    style_btn_default_rel.body.padding.top = style_btn_default_rel.body.padding.bottom = 2;
    style_btn_default_rel.body.padding.inner = 2;
    style_btn_default_rel.text.font = UI_FONT_MEDIUM;
    style_btn_default_rel.line.width = 3;
    style_btn_default_rel.line.rounded = 1;
    fill(&style_btn_default_rel, &style_btn_default_rel, UI_COLOR_SURFACE2, UI_COLOR_BORDER, UI_COLOR_TEXT);
    fill(&style_btn_default_pr, &style_btn_default_rel, UI_COLOR_BORDER, UI_COLOR_TEXT_DIM, UI_COLOR_TEXT);

    fill(&style_btn_primary_rel, &style_btn_default_rel, UI_COLOR_PRIMARY, UI_COLOR_PRIMARY, UI_COLOR_TEXT);
    fill(&style_btn_primary_pr, &style_btn_default_rel, LV_COLOR_MAKE(0x10, 0x5C, 0xB0), UI_COLOR_PRIMARY, UI_COLOR_TEXT);
    fill(&style_btn_success_rel, &style_btn_default_rel, UI_COLOR_SUCCESS, UI_COLOR_SUCCESS, UI_COLOR_BACKGROUND);
    fill(&style_btn_success_pr, &style_btn_default_rel, LV_COLOR_MAKE(0x16, 0x76, 0x32), UI_COLOR_SUCCESS, UI_COLOR_TEXT);
    fill(&style_btn_warning_rel, &style_btn_default_rel, UI_COLOR_WARNING, UI_COLOR_WARNING, UI_COLOR_BACKGROUND);
    fill(&style_btn_warning_pr, &style_btn_default_rel, LV_COLOR_MAKE(0xB0, 0x72, 0x00), UI_COLOR_WARNING, UI_COLOR_BACKGROUND);
    fill(&style_btn_danger_rel, &style_btn_default_rel, UI_COLOR_DANGER, UI_COLOR_DANGER, UI_COLOR_TEXT);
    fill(&style_btn_danger_pr, &style_btn_default_rel, LV_COLOR_MAKE(0x9A, 0x20, 0x2A), UI_COLOR_DANGER, UI_COLOR_TEXT);
    fill(&style_btn_outline_danger_rel, &style_btn_default_rel, UI_COLOR_SURFACE2, UI_COLOR_DANGER, UI_COLOR_DANGER_TEXT);

    // Tabs: flat, the selected one lighter with a blue edge on the left
    fill(&style_tab_rel, &style_btn_default_rel, UI_COLOR_SURFACE, UI_COLOR_SURFACE, UI_COLOR_TEXT_DIM);
    style_tab_rel.body.radius = 0;
    style_tab_rel.body.border.width = 0;
    style_tab_rel.text.font = UI_FONT_SMALL;
    fill(&style_tab_pr, &style_tab_rel, UI_COLOR_BORDER, UI_COLOR_BORDER, UI_COLOR_TEXT);
    fill(&style_tab_sel, &style_tab_rel, UI_COLOR_SURFACE2, UI_COLOR_PRIMARY, UI_COLOR_TEXT);
    style_tab_sel.body.border.width = 4;
    style_tab_sel.body.border.part = LV_BORDER_LEFT;

    // Panels and position cells
    fill(&style_panel, &style_btn_default_rel, UI_COLOR_SURFACE, UI_COLOR_SURFACE, UI_COLOR_TEXT);
    style_panel.body.border.width = 0;

    lv_style_copy(&style_scrim, &lv_style_plain);
    style_scrim.body.main_color = LV_COLOR_BLACK;
    style_scrim.body.grad_color = LV_COLOR_BLACK;
    style_scrim.body.opa = LV_OPA_70;
    style_scrim.body.radius = 0;

    // Screen background
    lv_style_copy(&style_screen, &lv_style_plain);
    style_screen.body.main_color = UI_COLOR_BACKGROUND;
    style_screen.body.grad_color = UI_COLOR_BACKGROUND;
    style_screen.body.border.width = 0;
    style_screen.body.radius = 0;
    style_screen.text.color = UI_COLOR_TEXT;

    // Machine state label styles
    lv_style_copy(&style_state_idle, &lv_style_plain);
    style_state_idle.text.font = UI_FONT_XLARGE;
    style_state_idle.text.color = UI_COLOR_SUCCESS;
    lv_style_copy(&style_state_run, &style_state_idle);
    style_state_run.text.color = UI_COLOR_PRIMARY;
    lv_style_copy(&style_state_warn, &style_state_idle);
    style_state_warn.text.color = UI_COLOR_WARNING;
    lv_style_copy(&style_state_alarm, &style_state_idle);
    style_state_alarm.text.color = UI_COLOR_DANGER;
}

void ui_widget_set_button_style(lv_obj_t *btn, ui_button_style_t style) {
    const lv_style_t *rel, *pr;

    switch(style) {
        case BUTTON_STYLE_PRIMARY:        rel = &style_btn_primary_rel; pr = &style_btn_primary_pr; break;
        case BUTTON_STYLE_SUCCESS:        rel = &style_btn_success_rel; pr = &style_btn_success_pr; break;
        case BUTTON_STYLE_WARNING:        rel = &style_btn_warning_rel; pr = &style_btn_warning_pr; break;
        case BUTTON_STYLE_DANGER:         rel = &style_btn_danger_rel;  pr = &style_btn_danger_pr; break;
        case BUTTON_STYLE_OUTLINE_DANGER: rel = &style_btn_outline_danger_rel; pr = &style_btn_danger_pr; break;
        case BUTTON_STYLE_TAB:            rel = &style_tab_rel; pr = &style_tab_pr; break;
        case BUTTON_STYLE_TAB_SELECTED:   rel = &style_tab_sel; pr = &style_tab_pr; break;
        case BUTTON_STYLE_PANEL:          rel = &style_panel; pr = &style_btn_default_pr; break;
        default:                          rel = &style_btn_default_rel; pr = &style_btn_default_pr; break;
    }

    lv_btn_set_style(btn, LV_BTN_STYLE_REL, rel);
    lv_btn_set_style(btn, LV_BTN_STYLE_PR, pr);
}

lv_obj_t *ui_widget_create_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t *panel = lv_obj_create(parent, NULL);

    lv_obj_set_style(panel, &style_panel);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_click(panel, false);

    return panel;
}

lv_obj_t *ui_widget_create_scrim(lv_obj_t *parent) {
    lv_obj_t *scrim = lv_obj_create(parent, NULL);

    lv_obj_set_style(scrim, &style_scrim);
    lv_obj_set_pos(scrim, UI_TABS_W, UI_STATUS_H);
    lv_obj_set_size(scrim, UI_ACTION_X - UI_TABS_W, TFT_DISPLAY_HEIGHT - UI_STATUS_H);
    lv_obj_set_click(scrim, true);  // swallows touches meant for what is under it

    return scrim;
}

lv_style_t *ui_widget_text_style(const lv_font_t *font, lv_color_t color) {
    for(uint_fast8_t idx = 0; idx < n_text_styles; idx++) {
        if(text_styles[idx].font == font && text_styles[idx].color.full == color.full)
            return &text_styles[idx].style;
    }

    if(n_text_styles == N_TEXT_STYLES)
        return &style_screen;   // out of slots: still readable

    lv_style_t *style = &text_styles[n_text_styles].style;
    text_styles[n_text_styles].font = font;
    text_styles[n_text_styles].color = color;
    n_text_styles++;
    lv_style_copy(style, &lv_style_plain);
    style->text.font = font;
    style->text.color = color;

    return style;
}

lv_style_t *ui_widget_screen_style(void) {
    return &style_screen;
}

lv_obj_t *ui_widget_create_styled_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                        lv_style_t *style, const char *text) {
    lv_obj_t *label = lv_label_create(parent, NULL);

    lv_label_set_style(label, LV_LABEL_STYLE_MAIN, style);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);

    return label;
}

void ui_widget_set_text(lv_obj_t *label, char *shown, size_t size, const char *text) {
    if(strcmp(shown, text)) {
        strncpy(shown, text, size - 1);
        shown[size - 1] = '\0';
        lv_label_set_text(label, text);
    }
}

void ui_widget_format_state(char *buf, size_t size) {
    if((ui_state.system_state & STATE_ALARM) && ui_state.alarm_code)
        snprintf(buf, size, "ALARM %u", (unsigned)ui_state.alarm_code);
    else
        snprintf(buf, size, "%s", ui_state_get_state_string());
}

void ui_widget_format_tool_change(char *buf, size_t size) {
#if TOOLSETTER_ENABLE
    bool setter_missing = ui_state.tc_mode == 3 && !ui_state.tls_pos_set;
#else
    bool setter_missing = false;
#endif
    tft_logic_format_tool_change(buf, size, ui_state.tool_pending, setter_missing);
}

lv_style_t *ui_widget_state_style(sys_state_t state) {
    if(state == STATE_IDLE)
        return &style_state_idle;
    if(state & (STATE_ALARM|STATE_ESTOP))
        return &style_state_alarm;
    if(state & (STATE_HOLD|STATE_SAFETY_DOOR|STATE_TOOL_CHANGE|STATE_CHECK_MODE|STATE_SLEEP))
        return &style_state_warn;

    return &style_state_run;    // cycle, jog, homing
}

/*
 * Create styled button (LVGL v6)
 */
lv_obj_t* ui_widget_create_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                   lv_coord_t width, lv_coord_t height,
                                   const char *text, ui_button_style_t style,
                                   lv_event_cb_t callback) {
    // Create button
    lv_obj_t *btn = lv_btn_create(parent, NULL);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, width, height);

    ui_widget_set_button_style(btn, style);

    // Create label
    lv_obj_t *label = lv_label_create(btn, NULL);
    lv_label_set_text(label, text);
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

    // Set event callback
    if(callback) {
        lv_obj_set_event_cb(btn, callback);
    }

    return btn;
}

/*
 * Create label (LVGL v6)
 */
lv_obj_t* ui_widget_create_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                  const char *text, const lv_font_t *font,
                                  lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent, NULL);
    lv_obj_set_pos(label, x, y);
    lv_label_set_text(label, text);

    // Apply font and color (LVGL v6). Styles are referenced, not copied, so
    // each label needs its own.
    lv_style_t *label_style = (lv_style_t *)lv_mem_alloc(sizeof(lv_style_t));
    if(label_style) {
        lv_style_copy(label_style, &lv_style_plain);
        if(font) {
            label_style->text.font = font;
        }
        label_style->text.color = color;
        lv_label_set_style(label, LV_LABEL_STYLE_MAIN, label_style);
    }

    return label;
}

/*
 * Create status bar (LVGL v6)
 */
lv_obj_t* ui_widget_create_status_bar(lv_obj_t *parent) {
    // Create container
    lv_obj_t *container = lv_obj_create(parent, NULL);
    lv_obj_set_size(container, TFT_DISPLAY_WIDTH, 40);
    lv_obj_set_pos(container, 0, 0);

    // Apply container style
    static lv_style_t container_style;
    lv_style_copy(&container_style, &lv_style_plain);
    container_style.body.main_color = UI_COLOR_BACKGROUND;
    container_style.body.grad_color = UI_COLOR_BACKGROUND;
    container_style.body.border.width = 1;
    container_style.body.border.color = UI_COLOR_TEXT_DIM;
    lv_obj_set_style(container, &container_style);

    // Allocate status bar components and store in global
    if(!g_status_bar_components) {
        g_status_bar_components = lv_mem_alloc(sizeof(status_bar_components_t));
    }
    if(!g_status_bar_components) return container;

    // State label (left side)
    g_status_bar_components->state_label = ui_widget_create_label(container, 10, 10, "Idle",
                                                      UI_FONT_MEDIUM, UI_COLOR_SUCCESS);

    // Position label (center-left)
    g_status_bar_components->position_label = ui_widget_create_label(container, 100, 10, "X:0.00 Y:0.00 Z:0.00",
                                                          UI_FONT_SMALL, UI_COLOR_TEXT);

    // Feed label (center-right)
    g_status_bar_components->feed_label = ui_widget_create_label(container, 300, 10, "F:0",
                                                      UI_FONT_SMALL, UI_COLOR_TEXT);

    // Spindle label (right side)
    g_status_bar_components->spindle_label = ui_widget_create_label(container, 400, 10, "S:0",
                                                         UI_FONT_SMALL, UI_COLOR_TEXT);

    return container;
}

/*
 * Update status bar
 */
void ui_widget_update_status_bar(lv_obj_t *status_bar) {
    if(!status_bar || !g_status_bar_components) return;

    // Update state
    const char *state_str = ui_state_get_state_string();
    lv_label_set_text(g_status_bar_components->state_label, state_str);

    // Update position (show work or machine coordinates)
    char pos_buf[64];
    float *pos = (ui_state.coord_mode == COORD_MODE_WORK)
                 ? ui_state.work_position
                 : ui_state.machine_position;

    if(ui_state.n_axis >= 3) {
        snprintf(pos_buf, sizeof(pos_buf), "X:%.2f Y:%.2f Z:%.2f",
                 pos[0], pos[1], pos[2]);
    } else {
        snprintf(pos_buf, sizeof(pos_buf), "X:%.2f Y:%.2f",
                 pos[0], pos[1]);
    }
    lv_label_set_text(g_status_bar_components->position_label, pos_buf);

    // Update feed rate
    char feed_buf[16];
    snprintf(feed_buf, sizeof(feed_buf), "F:%.0f", ui_state.feed_rate);
    lv_label_set_text(g_status_bar_components->feed_label, feed_buf);

    // Update spindle speed
    char spindle_buf[16];
    snprintf(spindle_buf, sizeof(spindle_buf), "S:%.0f", ui_state.spindle_speed);
    lv_label_set_text(g_status_bar_components->spindle_label, spindle_buf);
}

/*
 * Create axis position display (LVGL v6)
 */
lv_obj_t* ui_widget_create_axis_display(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                         uint8_t axis) {
    if(axis >= 6) return NULL;  // Support up to 6 axes

    // Create container
    lv_obj_t *container = lv_obj_create(parent, NULL);
    lv_obj_set_size(container, 120, 60);
    lv_obj_set_pos(container, x, y);

    // Axis name label
    char axis_name[2] = {ui_state_get_axis_name(axis), '\0'};
    ui_widget_create_label(container, 10, 5, axis_name, UI_FONT_LARGE, UI_COLOR_PRIMARY);

    // Position value label
    lv_obj_t *pos_label = ui_widget_create_label(container, 10, 30, "0.00",
                                                   UI_FONT_MEDIUM, UI_COLOR_TEXT);

    // Store in global array (LVGL v6)
    g_axis_displays[axis].label = pos_label;
    g_axis_displays[axis].axis = axis;

    return container;
}

/*
 * Update axis position display
 */
void ui_widget_update_axis_display(lv_obj_t *axis_display, float position) {
    // Note: axis_display parameter is not used with global storage approach
    // This function is now only used to update by axis index
    (void)axis_display;
    (void)position;
}

/*
 * Update axis position by axis index (helper for global storage)
 */
void ui_widget_update_axis_by_index(uint8_t axis, float position) {
    if(axis >= 6 || !g_axis_displays[axis].label) return;

    char pos_buf[16];
    snprintf(pos_buf, sizeof(pos_buf), "%.2f", position);
    lv_label_set_text(g_axis_displays[axis].label, pos_buf);
}

/*
 * Create progress bar (LVGL v6)
 */
lv_obj_t* ui_widget_create_progress_bar(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                         lv_coord_t width, lv_coord_t height) {
    lv_obj_t *bar = lv_bar_create(parent, NULL);
    lv_obj_set_size(bar, width, height);
    lv_obj_set_pos(bar, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);  // LVGL v6 requires 3 arguments

    return bar;
}

/*
 * Update progress bar value
 */
void ui_widget_update_progress_bar(lv_obj_t *bar, uint8_t value) {
    if(!bar) return;
    lv_bar_set_value(bar, value, LV_ANIM_ON);  // LVGL v6 requires 3 arguments
}

/*
 * Create message box (LVGL v6 uses lv_mbox_*)
 */
lv_obj_t* ui_widget_create_msgbox(const char *title, const char *message,
                                   const char **buttons, lv_event_cb_t callback) {
    lv_obj_t *mbox = lv_mbox_create(lv_scr_act(), NULL);
    lv_mbox_set_text(mbox, message);

    if(buttons) {
        lv_mbox_add_btns(mbox, buttons);  // LVGL v6 takes only 2 arguments
    }

    if(callback) {
        lv_obj_set_event_cb(mbox, callback);
    }

    lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);

    return mbox;
}

/*
 * Close message box
 */
void ui_widget_close_msgbox(lv_obj_t *msgbox) {
    if(msgbox) {
        lv_mbox_start_auto_close(msgbox, 0);
    }
}

/*
 * Create icon button (LVGL v6)
 */
lv_obj_t* ui_widget_create_icon_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t width, lv_coord_t height,
                                        const char *symbol, lv_event_cb_t callback) {
    lv_obj_t *btn = lv_btn_create(parent, NULL);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, width, height);
    lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style_btn_default_rel);
    lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style_btn_default_pr);

    lv_obj_t *label = lv_label_create(btn, NULL);
    lv_label_set_text(label, symbol);
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

    if(callback) {
        lv_obj_set_event_cb(btn, callback);
    }

    return btn;
}

/*
 * Create container (LVGL v6)
 */
lv_obj_t* ui_widget_create_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                      lv_coord_t width, lv_coord_t height) {
    lv_obj_t *container = lv_obj_create(parent, NULL);
    lv_obj_set_size(container, width, height);
    lv_obj_set_pos(container, x, y);

    // Apply container style
    static lv_style_t container_style;
    lv_style_copy(&container_style, &lv_style_plain);
    container_style.body.main_color = LV_COLOR_MAKE(0x30, 0x30, 0x30);
    container_style.body.grad_color = LV_COLOR_MAKE(0x30, 0x30, 0x30);
    container_style.body.border.width = 2;
    container_style.body.border.color = UI_COLOR_TEXT_DIM;
    container_style.body.radius = 8;
    lv_obj_set_style(container, &container_style);

    return container;
}

/*
 * Confirm button
 */

#define N_CONFIRM_BTNS 8

static ui_confirm_btn_t *confirm_btns[N_CONFIRM_BTNS];
static lv_style_t style_fill_bg, style_fill_indic;

static tft_confirm_mode_t confirm_mode (void)
{
    return tft_settings_confirm_hold() ? Confirm_Hold : Confirm_TapTwice;
}

static void on_confirm_btn (lv_obj_t *obj, lv_event_t event)
{
    ui_confirm_btn_t *cb = NULL;
    uint32_t now = lv_tick_get();

    for(uint_fast8_t idx = 0; idx < N_CONFIRM_BTNS && !cb; idx++) {
        if(confirm_btns[idx] && confirm_btns[idx]->btn == obj)
            cb = confirm_btns[idx];
    }
    if(!cb)
        return;

    bool fire = false;

    if(event == LV_EVENT_PRESSED) {
        if(!cb->direct)     // direct: the tap alone runs it
            tft_logic_confirm_event(&cb->confirm, Confirm_Press, now);
    }
    else if(event == LV_EVENT_RELEASED || event == LV_EVENT_PRESS_LOST)
        tft_logic_confirm_event(&cb->confirm, Confirm_Release, now);
    else if(event == LV_EVENT_CLICKED) {
        if(cb->direct) {
            tft_logic_confirm_cancel(&cb->confirm);
            fire = true;
        } else
            fire = tft_logic_confirm_event(&cb->confirm, Confirm_Click, now);
    } else
        return;

    if(fire && cb->fire)
        cb->fire();

    ui_confirm_btn_update(cb);
}

void ui_confirm_btn_create (ui_confirm_btn_t *cb, lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            lv_coord_t w, lv_coord_t h, const char *text, ui_button_style_t style,
                            void (*fire)(void))
{
    static bool styled = false;

    if(!styled) {
        styled = true;
        lv_style_copy(&style_fill_bg, &lv_style_plain);
        style_fill_bg.body.opa = LV_OPA_TRANSP;
        style_fill_bg.body.border.width = 0;
        style_fill_bg.body.padding.left = style_fill_bg.body.padding.right = 0;
        style_fill_bg.body.padding.top = style_fill_bg.body.padding.bottom = 0;
        lv_style_copy(&style_fill_indic, &style_fill_bg);
        style_fill_indic.body.opa = LV_OPA_COVER;
        style_fill_indic.body.main_color = UI_COLOR_WARNING;
        style_fill_indic.body.grad_color = UI_COLOR_WARNING;
        style_fill_indic.body.radius = 2;
    }

    memset(cb, 0, sizeof(*cb));
    cb->text = text;
    cb->style = style;
    cb->fire = fire;
    cb->shown = -1;
    tft_logic_confirm_init(&cb->confirm, confirm_mode());

    cb->btn = lv_btn_create(parent, NULL);
    lv_obj_set_pos(cb->btn, x, y);
    lv_obj_set_size(cb->btn, w, h);
    lv_btn_set_layout(cb->btn, LV_LAYOUT_OFF);
    ui_widget_set_button_style(cb->btn, style);
    lv_obj_set_event_cb(cb->btn, on_confirm_btn);

    cb->label = lv_label_create(cb->btn, NULL);
    lv_label_set_align(cb->label, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(cb->label, text);
    lv_obj_set_auto_realign(cb->label, true);
    lv_obj_align(cb->label, NULL, LV_ALIGN_CENTER, 0, 0);

    // Hold progress along the bottom edge
    cb->bar = lv_bar_create(cb->btn, NULL);
    lv_obj_set_size(cb->bar, w - 12, 6);
    lv_obj_align(cb->bar, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -5);
    lv_bar_set_range(cb->bar, 0, 100);
    lv_bar_set_style(cb->bar, LV_BAR_STYLE_BG, &style_fill_bg);
    lv_bar_set_style(cb->bar, LV_BAR_STYLE_INDIC, &style_fill_indic);
    lv_obj_set_click(cb->bar, false);
    lv_obj_set_hidden(cb->bar, true);

    for(uint_fast8_t idx = 0; idx < N_CONFIRM_BTNS; idx++) {
        if(!confirm_btns[idx] || confirm_btns[idx] == cb) {
            confirm_btns[idx] = cb;
            break;
        }
    }
}

void ui_confirm_btn_set (ui_confirm_btn_t *cb, const char *text, ui_button_style_t style, bool direct)
{
    if(cb->text != text || cb->style != style)
        cb->shown = -1;
    cb->text = text;
    cb->style = style;
    cb->direct = direct;
}

void ui_confirm_btn_cancel (ui_confirm_btn_t *cb)
{
    tft_logic_confirm_cancel(&cb->confirm);
    ui_confirm_btn_update(cb);
}

void ui_confirm_btn_update (ui_confirm_btn_t *cb)
{
    uint32_t now = lv_tick_get();
    tft_confirm_mode_t mode = confirm_mode();

    if(mode != cb->confirm.mode) {
        tft_logic_confirm_init(&cb->confirm, mode);
        cb->shown = -1;
    }

    if(tft_logic_confirm_tick(&cb->confirm, now) && cb->fire)
        cb->fire();     // held long enough

    // 0 normal, 1 armed (second tap), 2 holding
    int8_t look = cb->confirm.armed ? 1 : cb->confirm.pressing ? 2 : 0;
    uint8_t pct = tft_logic_confirm_progress(&cb->confirm, now);

    if(look != cb->shown) {
        cb->shown = look;
        ui_widget_set_button_style(cb->btn, look == 1 ? BUTTON_STYLE_WARNING : cb->style);
        lv_label_set_text(cb->label, look == 1 ? "Sure?" : cb->text);
        lv_obj_set_hidden(cb->bar, look != 2);
    }

    if(look == 2 && pct != cb->shown_pct) {
        cb->shown_pct = pct;
        lv_bar_set_value(cb->bar, pct, LV_ANIM_OFF);
    }
}
