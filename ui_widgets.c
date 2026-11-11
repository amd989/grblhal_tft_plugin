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
#include <stdio.h>
#include <string.h>

// Button styles (LVGL v6)
static lv_style_t style_btn_default_rel;
static lv_style_t style_btn_default_pr;
static lv_style_t style_btn_primary_rel;
static lv_style_t style_btn_primary_pr;
static lv_style_t style_btn_success_rel;
static lv_style_t style_btn_warning_rel;
static lv_style_t style_btn_danger_rel;

// Screen background
static lv_style_t style_screen;

// Machine state label styles
static lv_style_t style_state_idle;
static lv_style_t style_state_run;
static lv_style_t style_state_warn;
static lv_style_t style_state_alarm;

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

/*
 * Initialize widget system (LVGL v6)
 */
void ui_widgets_init(void) {
    // Default button style - released
    lv_style_copy(&style_btn_default_rel, &lv_style_btn_rel);
    style_btn_default_rel.body.main_color = LV_COLOR_MAKE(0x40, 0x40, 0x40);
    style_btn_default_rel.body.grad_color = LV_COLOR_MAKE(0x40, 0x40, 0x40);
    style_btn_default_rel.body.border.width = 2;
    style_btn_default_rel.body.border.color = LV_COLOR_MAKE(0x80, 0x80, 0x80);
    style_btn_default_rel.body.radius = 8;
    style_btn_default_rel.text.color = UI_COLOR_TEXT;

    // Default button style - pressed
    lv_style_copy(&style_btn_default_pr, &lv_style_btn_pr);
    style_btn_default_pr.body.main_color = LV_COLOR_MAKE(0x60, 0x60, 0x60);
    style_btn_default_pr.body.grad_color = LV_COLOR_MAKE(0x60, 0x60, 0x60);
    style_btn_default_pr.body.border.width = 2;
    style_btn_default_pr.body.border.color = LV_COLOR_MAKE(0x80, 0x80, 0x80);
    style_btn_default_pr.body.radius = 8;
    style_btn_default_pr.text.color = UI_COLOR_TEXT;

    // Primary button style (blue)
    lv_style_copy(&style_btn_primary_rel, &style_btn_default_rel);
    style_btn_primary_rel.body.main_color = UI_COLOR_PRIMARY;
    style_btn_primary_rel.body.grad_color = UI_COLOR_PRIMARY;
    style_btn_primary_rel.body.border.color = UI_COLOR_PRIMARY;

    lv_style_copy(&style_btn_primary_pr, &style_btn_default_pr);
    style_btn_primary_pr.body.main_color = LV_COLOR_MAKE(0x00, 0x5B, 0xDF);
    style_btn_primary_pr.body.grad_color = LV_COLOR_MAKE(0x00, 0x5B, 0xDF);

    // Success button style (green)
    lv_style_copy(&style_btn_success_rel, &style_btn_default_rel);
    style_btn_success_rel.body.main_color = UI_COLOR_SUCCESS;
    style_btn_success_rel.body.grad_color = UI_COLOR_SUCCESS;
    style_btn_success_rel.body.border.color = UI_COLOR_SUCCESS;

    // Warning button style (orange)
    lv_style_copy(&style_btn_warning_rel, &style_btn_default_rel);
    style_btn_warning_rel.body.main_color = UI_COLOR_WARNING;
    style_btn_warning_rel.body.grad_color = UI_COLOR_WARNING;
    style_btn_warning_rel.body.border.color = UI_COLOR_WARNING;

    // Danger button style (red)
    lv_style_copy(&style_btn_danger_rel, &style_btn_default_rel);
    style_btn_danger_rel.body.main_color = UI_COLOR_DANGER;
    style_btn_danger_rel.body.grad_color = UI_COLOR_DANGER;
    style_btn_danger_rel.body.border.color = UI_COLOR_DANGER;

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
    switch(style) {
        case BUTTON_STYLE_PRIMARY:
            lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style_btn_primary_rel);
            lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style_btn_primary_pr);
            break;
        case BUTTON_STYLE_SUCCESS:
            lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style_btn_success_rel);
            lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style_btn_default_pr);
            break;
        case BUTTON_STYLE_WARNING:
            lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style_btn_warning_rel);
            lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style_btn_default_pr);
            break;
        case BUTTON_STYLE_DANGER:
            lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style_btn_danger_rel);
            lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style_btn_default_pr);
            break;
        default:
            lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style_btn_default_rel);
            lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style_btn_default_pr);
            break;
    }
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
