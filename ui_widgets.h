/*
 * ui_widgets.h - Reusable LVGL widget library
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 * Provides common UI widgets:
 * - Styled buttons
 * - Status bar
 * - Axis position display
 * - Progress bars
 * - Dialogs
 */

#ifndef _UI_WIDGETS_H_
#define _UI_WIDGETS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <lvgl.h>
#include "ui_state.h"
#include "tft_logic.h"

/*
 * Color scheme
 */
#define UI_COLOR_PRIMARY        LV_COLOR_MAKE(0x1E, 0x88, 0xFF)  // Blue: run, jog, navigation
#define UI_COLOR_SUCCESS        LV_COLOR_MAKE(0x22, 0xB1, 0x4C)  // Green: idle, start, touching
#define UI_COLOR_WARNING        LV_COLOR_MAKE(0xFF, 0xA5, 0x00)  // Orange: hold, armed confirm
#define UI_COLOR_DANGER         LV_COLOR_MAKE(0xE5, 0x32, 0x3F)  // Red: alarm, stop, reset
#define UI_COLOR_DANGER_TEXT    LV_COLOR_MAKE(0xFF, 0x8A, 0x92)  // Red text on dark
#define UI_COLOR_BACKGROUND     LV_COLOR_MAKE(0x0A, 0x0C, 0x0E)  // Screen
#define UI_COLOR_SURFACE        LV_COLOR_MAKE(0x1C, 0x20, 0x24)  // Panels, bars
#define UI_COLOR_SURFACE2       LV_COLOR_MAKE(0x2C, 0x32, 0x38)  // Buttons
#define UI_COLOR_BORDER         LV_COLOR_MAKE(0x4A, 0x52, 0x5A)  // Button border, pressed
#define UI_COLOR_TEXT           LV_COLOR_MAKE(0xFF, 0xFF, 0xFF)  // White
#define UI_COLOR_TEXT_DIM       LV_COLOR_MAKE(0xA8, 0xB0, 0xB8)  // Secondary text

/*
 * Screen areas. The status bar, the tabs on the left and the action bar on
 * the right are shared by all screens (ui_chrome.c), screens draw in the
 * content area between them.
 */
#define UI_STATUS_H             36
#define UI_TABS_W               56
#define UI_ACTION_X             412
#define UI_CONTENT_X            60      // with a 4 px gap to the tabs
#define UI_CONTENT_Y            40
#define UI_CONTENT_W            346     // to x 406
#define UI_CONTENT_H            276     // to y 316

/*
 * Font sizes (LVGL v6 built-in fonts)
 */
#define UI_FONT_SMALL           &lv_font_roboto_12
#define UI_FONT_MEDIUM          &lv_font_roboto_16
#define UI_FONT_LARGE           &lv_font_roboto_22
#define UI_FONT_XLARGE          &lv_font_roboto_28

/*
 * Button styles
 */
typedef enum {
    BUTTON_STYLE_DEFAULT = 0,
    BUTTON_STYLE_PRIMARY,
    BUTTON_STYLE_SUCCESS,
    BUTTON_STYLE_WARNING,
    BUTTON_STYLE_DANGER,
    BUTTON_STYLE_OUTLINE_DANGER,    // dark with a red border: Reset, Stop
    BUTTON_STYLE_TAB,               // left tabs
    BUTTON_STYLE_TAB_SELECTED,
    BUTTON_STYLE_PANEL              // flat surface, for cells such as the position
} ui_button_style_t;

/*
 * Initialize widget system (create styles)
 */
void ui_widgets_init(void);

/*
 * Create styled button
 *
 * @param parent    Parent LVGL object
 * @param x         X position
 * @param y         Y position
 * @param width     Button width
 * @param height    Button height
 * @param text      Button text
 * @param style     Button style
 * @param callback  Click event callback
 * @return          Button object
 */
lv_obj_t* ui_widget_create_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                   lv_coord_t width, lv_coord_t height,
                                   const char *text, ui_button_style_t style,
                                   lv_event_cb_t callback);

/*
 * Create label
 *
 * @param parent    Parent LVGL object
 * @param x         X position
 * @param y         Y position
 * @param text      Label text
 * @param font      Font (use UI_FONT_* macros)
 * @param color     Text color
 * @return          Label object
 */
lv_obj_t* ui_widget_create_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                  const char *text, const lv_font_t *font,
                                  lv_color_t color);

/*
 * Create status bar (shows system state, position, feed/spindle)
 *
 * @param parent    Parent LVGL object
 * @return          Status bar container
 */
lv_obj_t* ui_widget_create_status_bar(lv_obj_t *parent);

/*
 * Update status bar (call from screen update())
 *
 * @param status_bar    Status bar object
 */
void ui_widget_update_status_bar(lv_obj_t *status_bar);

/*
 * Create axis position display
 *
 * @param parent    Parent LVGL object
 * @param x         X position
 * @param y         Y position
 * @param axis      Axis index (0=X, 1=Y, 2=Z, etc.)
 * @return          Axis display container
 */
lv_obj_t* ui_widget_create_axis_display(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                         uint8_t axis);

/*
 * Update axis position display
 *
 * @param axis_display  Axis display object
 * @param position      Position value
 */
void ui_widget_update_axis_display(lv_obj_t *axis_display, float position);

/*
 * Create progress bar
 *
 * @param parent    Parent LVGL object
 * @param x         X position
 * @param y         Y position
 * @param width     Bar width
 * @param height    Bar height
 * @return          Progress bar object
 */
lv_obj_t* ui_widget_create_progress_bar(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                         lv_coord_t width, lv_coord_t height);

/*
 * Update progress bar value
 *
 * @param bar       Progress bar object
 * @param value     Value (0-100)
 */
void ui_widget_update_progress_bar(lv_obj_t *bar, uint8_t value);

/*
 * Create message box (modal dialog)
 *
 * @param title     Dialog title
 * @param message   Dialog message
 * @param buttons   Button labels (NULL-terminated array, e.g., {"OK", "Cancel", NULL})
 * @param callback  Button click callback
 * @return          Message box object
 */
lv_obj_t* ui_widget_create_msgbox(const char *title, const char *message,
                                   const char **buttons, lv_event_cb_t callback);

/*
 * Close message box
 *
 * @param msgbox    Message box object
 */
void ui_widget_close_msgbox(lv_obj_t *msgbox);

/*
 * Create icon button (button with image)
 *
 * @param parent    Parent LVGL object
 * @param x         X position
 * @param y         Y position
 * @param width     Button width
 * @param height    Button height
 * @param symbol    LVGL symbol (e.g., LV_SYMBOL_HOME)
 * @param callback  Click event callback
 * @return          Button object
 */
lv_obj_t* ui_widget_create_icon_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t width, lv_coord_t height,
                                        const char *symbol, lv_event_cb_t callback);

/*
 * Create container with border
 *
 * @param parent    Parent LVGL object
 * @param x         X position
 * @param y         Y position
 * @param width     Container width
 * @param height    Container height
 * @return          Container object
 */
lv_obj_t* ui_widget_create_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                      lv_coord_t width, lv_coord_t height);

/*
 * Change a button's color style after creation (e.g. to highlight a selection)
 */
void ui_widget_set_button_style(lv_obj_t *btn, ui_button_style_t style);

/*
 * Dark full-screen background style for screen containers
 */
lv_style_t *ui_widget_screen_style(void);

/*
 * Label with an existing (shared, static) style
 */
lv_obj_t *ui_widget_create_styled_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                        lv_style_t *style, const char *text);

// Flat panel (surface color) for grouping, and a dark scrim over the content
// area that blocks touches to what is under a popup
lv_obj_t *ui_widget_create_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
lv_obj_t *ui_widget_create_scrim(lv_obj_t *parent);

// Shared text styles: font and color
lv_style_t *ui_widget_text_style(const lv_font_t *font, lv_color_t color);

/*
 * Set label text only if it differs from shown (the last text set), which
 * avoids redrawing unchanged labels. shown must hold size chars.
 */
void ui_widget_set_text(lv_obj_t *label, char *shown, size_t size, const char *text);

/*
 * Machine state label helpers shared by screens: text such as "Idle" or
 * "ALARM 9", and a large font style colored by state
 */
void ui_widget_format_state(char *buf, size_t size);

// What to do in a tool change: "Change to T2, then Resume", or a warning
// when $341=3 would measure at a toolsetter position that was never set
void ui_widget_format_tool_change(char *buf, size_t size);
lv_style_t *ui_widget_state_style(sys_state_t state);

/*
 * Confirm button, for spindle on, Stop and Reset: runs fire() after a
 * second tap within 3 s (it turns orange, "Sure?"), or after a 1 s hold
 * while a bar fills, as set with $458. With direct set, one tap runs it
 * (turning the spindle off). Call ui_confirm_btn_update() from the screen's
 * update, it also follows changes of $458.
 */
typedef struct {
    lv_obj_t *btn, *label, *bar;
    tft_confirm_t confirm;
    const char *text;           // shown when not armed
    ui_button_style_t style;
    bool direct;
    void (*fire)(void);
    int8_t shown;               // drawn look, -1 to redraw
    uint8_t shown_pct;
} ui_confirm_btn_t;

void ui_confirm_btn_create(ui_confirm_btn_t *cb, lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h, const char *text, ui_button_style_t style,
                           void (*fire)(void));

// Change the look when not armed, and whether one tap runs it
void ui_confirm_btn_set(ui_confirm_btn_t *cb, const char *text, ui_button_style_t style, bool direct);

void ui_confirm_btn_update(ui_confirm_btn_t *cb);

// Disarm, for example when the screen is left
void ui_confirm_btn_cancel(ui_confirm_btn_t *cb);

#ifdef __cplusplus
}
#endif

#endif // _UI_WIDGETS_H_
