/*
 * ui_screen_manager.c - Screen transition and management implementation
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 */

#include "tft_config.h"    // first, so my_machine.h comes before grblHAL config.h
#include "ui_screen_manager.h"
#include <string.h>

// Screen registry (static array for all possible screens)
static ui_screen_handler_t screen_handlers[SCREEN_COUNT] = {0};

// Root LVGL object (screen container)
static lv_obj_t *root_screen = NULL;

// Currently active screen
static ui_screen_t current_screen_id = SCREEN_NONE;

/*
 * Initialize screen manager
 */
void ui_screen_manager_init(void) {
    // Clear screen registry
    memset(screen_handlers, 0, sizeof(screen_handlers));

    // Create root screen (fullscreen container)
    root_screen = lv_obj_create(NULL, NULL);
    lv_obj_set_size(root_screen, TFT_DISPLAY_WIDTH, TFT_DISPLAY_HEIGHT);

    current_screen_id = SCREEN_NONE;
}

/*
 * Register a screen handler
 */
bool ui_screen_manager_register(ui_screen_t screen_id, ui_screen_callbacks_t *callbacks) {
    if(screen_id >= SCREEN_COUNT || !callbacks) {
        return false;
    }

    ui_screen_handler_t *handler = &screen_handlers[screen_id];
    handler->screen_id = screen_id;
    handler->callbacks = *callbacks;
    handler->created = false;
    handler->visible = false;
    handler->container = NULL;

    return true;
}

/*
 * Show screen (transitions from current to new screen)
 */
void ui_screen_manager_show(ui_screen_t screen_id) {
    if(screen_id >= SCREEN_COUNT) {
        return;
    }

    ui_screen_handler_t *new_handler = &screen_handlers[screen_id];

    // Check if handler is registered
    if(new_handler->callbacks.create == NULL) {
        return;  // Screen not registered
    }

    // Hide current screen
    if(current_screen_id != SCREEN_NONE && current_screen_id != screen_id) {
        ui_screen_handler_t *old_handler = &screen_handlers[current_screen_id];
        if(old_handler->visible && old_handler->callbacks.hide) {
            old_handler->callbacks.hide();
        }
        if(old_handler->container) {
            lv_obj_set_hidden(old_handler->container, true);
        }
        old_handler->visible = false;
    }

    // Create screen UI if not already created
    if(!new_handler->created) {
        // Create container for this screen
        new_handler->container = lv_obj_create(root_screen, NULL);
        lv_obj_set_size(new_handler->container, TFT_DISPLAY_WIDTH, TFT_DISPLAY_HEIGHT);
        lv_obj_set_pos(new_handler->container, 0, 0);

        // Call create callback
        new_handler->callbacks.create(new_handler->container);
        new_handler->created = true;
    }

    // Show new screen
    if(new_handler->container) {
        lv_obj_set_hidden(new_handler->container, false);
        lv_scr_load(root_screen);  // Load root screen
    }

    // Call show callback
    if(new_handler->callbacks.show) {
        new_handler->callbacks.show();
    }

    new_handler->visible = true;
    current_screen_id = screen_id;

    // Update UI state
    ui_state_set_screen(screen_id);
}

/*
 * Hide current screen
 */
void ui_screen_manager_hide_current(void) {
    if(current_screen_id == SCREEN_NONE) {
        return;
    }

    ui_screen_handler_t *handler = &screen_handlers[current_screen_id];

    if(handler->visible && handler->callbacks.hide) {
        handler->callbacks.hide();
    }

    if(handler->container) {
        lv_obj_set_hidden(handler->container, true);
    }

    handler->visible = false;
    current_screen_id = SCREEN_NONE;
}

/*
 * Update current screen
 */
void ui_screen_manager_update(void) {
    if(current_screen_id == SCREEN_NONE || current_screen_id >= SCREEN_COUNT) {
        return;
    }

    ui_screen_handler_t *handler = &screen_handlers[current_screen_id];

    // Call update callback if screen is visible
    if(handler->visible && handler->callbacks.update) {
        handler->callbacks.update();
    }
}

/*
 * Get screen container
 */
lv_obj_t* ui_screen_manager_get_container(ui_screen_t screen_id) {
    if(screen_id >= SCREEN_COUNT) {
        return NULL;
    }

    return screen_handlers[screen_id].container;
}

/*
 * Check if screen is visible
 */
bool ui_screen_manager_is_visible(ui_screen_t screen_id) {
    if(screen_id >= SCREEN_COUNT) {
        return false;
    }

    return screen_handlers[screen_id].visible;
}

/*
 * Go back to previous screen
 */
void ui_screen_manager_go_back(void) {
    ui_screen_t prev_screen = ui_state_get_previous_screen();

    if(prev_screen != SCREEN_NONE) {
        ui_screen_manager_show(prev_screen);
    }
}

/*
 * Get root container
 */
lv_obj_t* ui_screen_manager_get_root(void) {
    return root_screen;
}
