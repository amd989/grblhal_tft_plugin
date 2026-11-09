/*
 * ui_state.c - UI state management implementation
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 */

#include "ui_state.h"
#include <string.h>
#include <stdio.h>

// Global UI state (singleton)
ui_state_data_t ui_state = {0};

/*
 * Initialize UI state
 */
void ui_state_init(void) {
    memset(&ui_state, 0, sizeof(ui_state_data_t));

    ui_state.current_screen = SCREEN_SPLASH;
    ui_state.previous_screen = SCREEN_NONE;
    ui_state.system_state = STATE_IDLE;
    ui_state.coord_mode = COORD_MODE_WORK;
    ui_state.feed_override = 100;
    ui_state.spindle_override = 100;
    ui_state.n_axis = N_AXIS;
    ui_state.language = 0;  // English default
    ui_state.touch_calibrated = true;  // Assume calibrated (handled by TFT_eSPI)
    ui_state.screen_dirty = true;
}

/*
 * Update system state from grblHAL event
 */
void ui_state_update_system_state(sys_state_t state) {
    if(ui_state.system_state != state) {
        ui_state.system_state = state;
        ui_state.screen_dirty = true;
    }
}

/*
 * Update position from grblHAL realtime report
 */
void ui_state_update_position(float *work_pos, float *machine_pos) {
    if(work_pos) {
        memcpy(ui_state.work_position, work_pos, sizeof(float) * N_AXIS);
    }
    if(machine_pos) {
        memcpy(ui_state.machine_position, machine_pos, sizeof(float) * N_AXIS);
    }
    ui_state.screen_dirty = true;
}

/*
 * Update feed rate and spindle speed
 */
void ui_state_update_feed_spindle(float feed, float spindle) {
    ui_state.feed_rate = feed;
    ui_state.spindle_speed = spindle;
    ui_state.screen_dirty = true;
}

/*
 * Update override percentages
 */
void ui_state_update_overrides(uint8_t feed_ovr, uint8_t spindle_ovr) {
    ui_state.feed_override = feed_ovr;
    ui_state.spindle_override = spindle_ovr;
    ui_state.screen_dirty = true;
}

/*
 * Set alarm code
 */
void ui_state_set_alarm(uint16_t alarm) {
    ui_state.alarm_code = alarm;
    ui_state.screen_dirty = true;
}

/*
 * Set error code
 */
void ui_state_set_error(uint16_t error) {
    ui_state.error_code = error;
    ui_state.screen_dirty = true;
}

/*
 * Set current screen
 */
void ui_state_set_screen(ui_screen_t screen) {
    if(ui_state.current_screen != screen) {
        ui_state.previous_screen = ui_state.current_screen;
        ui_state.current_screen = screen;
        ui_state.screen_dirty = true;
    }
}

/*
 * Get current screen
 */
ui_screen_t ui_state_get_screen(void) {
    return ui_state.current_screen;
}

/*
 * Get previous screen
 */
ui_screen_t ui_state_get_previous_screen(void) {
    return ui_state.previous_screen;
}

/*
 * Toggle coordinate display mode
 */
void ui_state_toggle_coord_mode(void) {
    ui_state.coord_mode = (ui_state.coord_mode == COORD_MODE_WORK)
                          ? COORD_MODE_MACHINE
                          : COORD_MODE_WORK;
    ui_state.screen_dirty = true;
}

/*
 * Get coordinate display mode
 */
coord_mode_t ui_state_get_coord_mode(void) {
    return ui_state.coord_mode;
}

/*
 * Set axis homing status
 */
void ui_state_set_homed(uint8_t axis, bool homed) {
    if(axis < N_AXIS) {
        ui_state.homed[axis] = homed;
        ui_state.screen_dirty = true;
    }
}

/*
 * Check if axis is homed
 */
bool ui_state_is_homed(uint8_t axis) {
    return (axis < N_AXIS) ? ui_state.homed[axis] : false;
}

/*
 * Check if all axes are homed
 */
bool ui_state_all_homed(void) {
    for(uint8_t i = 0; i < ui_state.n_axis; i++) {
        if(!ui_state.homed[i]) {
            return false;
        }
    }
    return true;
}

/*
 * Set current file
 */
void ui_state_set_file(const char *filename, uint32_t size) {
    if(filename) {
        strncpy(ui_state.current_file, filename, sizeof(ui_state.current_file) - 1);
        ui_state.current_file[sizeof(ui_state.current_file) - 1] = '\0';
        ui_state.file_size = size;
        ui_state.file_position = 0;
        ui_state.job_progress = 0;
        ui_state.screen_dirty = true;
    }
}

/*
 * Update file position
 */
void ui_state_update_file_position(uint32_t position) {
    ui_state.file_position = position;

    // Calculate progress percentage
    if(ui_state.file_size > 0) {
        uint8_t new_progress = (uint8_t)((position * 100) / ui_state.file_size);
        if(new_progress != ui_state.job_progress) {
            ui_state.job_progress = new_progress;
            ui_state.screen_dirty = true;
        }
    }
}

/*
 * Clear file status
 */
void ui_state_clear_file(void) {
    ui_state.current_file[0] = '\0';
    ui_state.file_size = 0;
    ui_state.file_position = 0;
    ui_state.job_progress = 0;
    ui_state.screen_dirty = true;
}

/*
 * Get job progress percentage
 */
uint8_t ui_state_get_job_progress(void) {
    return ui_state.job_progress;
}

/*
 * Mark screen as dirty (needs redraw)
 */
void ui_state_mark_dirty(void) {
    ui_state.screen_dirty = true;
}

/*
 * Clear dirty flag
 */
void ui_state_clear_dirty(void) {
    ui_state.screen_dirty = false;
}

/*
 * Check if screen is dirty
 */
bool ui_state_is_dirty(void) {
    return ui_state.screen_dirty;
}

/*
 * Get axis name character
 */
char ui_state_get_axis_name(uint8_t axis) {
    const char axis_names[] = "XYZABC";
    return (axis < N_AXIS) ? axis_names[axis] : '?';
}

/*
 * Get system state string
 */
const char* ui_state_get_state_string(void) {
    switch(ui_state.system_state) {
        case STATE_IDLE:
            return "Idle";
        case STATE_CYCLE:
            return "Run";
        case STATE_HOLD:
            return "Hold";
        case STATE_JOG:
            return "Jog";
        case STATE_HOMING:
            return "Home";
        case STATE_ALARM:
            return "ALARM";
        case STATE_CHECK_MODE:
            return "Check";
        case STATE_TOOL_CHANGE:
            return "Tool";
        case STATE_SAFETY_DOOR:
            return "Door";
        case STATE_SLEEP:
            return "Sleep";
        case STATE_ESTOP:
            return "E-Stop";
        default:
            return "Unknown";
    }
}
