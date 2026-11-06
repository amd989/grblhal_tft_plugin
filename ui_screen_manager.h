/*
 * ui_screen_manager.h - Screen transition and management system
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 * Manages screen lifecycle:
 * - Screen registration
 * - Screen transitions (show/hide)
 * - Screen update coordination
 */

#ifndef _UI_SCREEN_MANAGER_H_
#define _UI_SCREEN_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>
#include "ui_state.h"

// Forward declaration
typedef struct ui_screen_handler ui_screen_handler_t;

/*
 * Screen lifecycle callbacks
 */
typedef struct {
    void (*create)(lv_obj_t *parent);   // Create screen UI elements
    void (*show)(void);                 // Called when screen becomes active
    void (*hide)(void);                 // Called when screen becomes inactive
    void (*update)(void);               // Called periodically to update display
    void (*destroy)(void);              // Cleanup (rarely used)
} ui_screen_callbacks_t;

/*
 * Screen handler structure
 */
struct ui_screen_handler {
    ui_screen_t screen_id;              // Screen identifier
    lv_obj_t *container;                // LVGL container object
    ui_screen_callbacks_t callbacks;    // Lifecycle callbacks
    bool created;                       // UI elements created flag
    bool visible;                       // Currently visible flag
};

/*
 * Initialize screen manager
 */
void ui_screen_manager_init(void);

/*
 * Register a screen handler
 */
bool ui_screen_manager_register(ui_screen_t screen_id, ui_screen_callbacks_t *callbacks);

/*
 * Show screen (transitions from current to new screen)
 */
void ui_screen_manager_show(ui_screen_t screen_id);

/*
 * Hide current screen
 */
void ui_screen_manager_hide_current(void);

/*
 * Update current screen (called from UI task loop)
 */
void ui_screen_manager_update(void);

/*
 * Get current screen container
 */
lv_obj_t* ui_screen_manager_get_container(ui_screen_t screen_id);

/*
 * Check if screen is currently visible
 */
bool ui_screen_manager_is_visible(ui_screen_t screen_id);

/*
 * Go back to previous screen
 */
void ui_screen_manager_go_back(void);

/*
 * Get root container for all screens
 */
lv_obj_t* ui_screen_manager_get_root(void);

#ifdef __cplusplus
}
#endif

#endif // _UI_SCREEN_MANAGER_H_
