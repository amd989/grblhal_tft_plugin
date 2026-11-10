/*
 * ui_state.h - UI state management for grblHAL TFT plugin
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 * Manages global UI state including:
 * - Current screen
 * - System state (Idle/Run/Hold/Alarm)
 * - Machine position (work and machine coordinates)
 * - Feed rate and spindle speed
 * - File/job status
 */

#ifndef _UI_STATE_H_
#define _UI_STATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "grbl/system.h"

// Screen enumeration
typedef enum {
    SCREEN_NONE = 0,
    SCREEN_SPLASH,          // Boot splash screen
    SCREEN_MAIN,            // Main/Ready screen (position display)
    SCREEN_CONTROL,         // Manual control (jog/home/zero)
    SCREEN_FILES,           // SD card file browser
    SCREEN_JOB,             // Job running screen
    SCREEN_TOOL,            // Tool/settings menu
    SCREEN_WIFI,            // WiFi configuration
    SCREEN_POWER,           // Laser power adjustment
    SCREEN_FRAME,           // Border tracing
    SCREEN_LANGUAGE,        // Language selection
    SCREEN_FILE_INFO,       // File information dialog
    SCREEN_COUNT            // Total number of screens
} ui_screen_t;

// Coordinate display mode
typedef enum {
    COORD_MODE_WORK = 0,    // Work coordinates (G54-G59)
    COORD_MODE_MACHINE      // Machine coordinates
} coord_mode_t;

// UI state structure
typedef struct {
    // Current screen
    ui_screen_t current_screen;
    ui_screen_t previous_screen;

    // System state (from grblHAL)
    sys_state_t system_state;       // Idle, Run, Hold, Alarm, etc.
    uint16_t alarm_code;            // Active alarm code (if in Alarm state)
    uint16_t error_code;            // Last error code

    // Machine position
    float work_position[N_AXIS];    // Work coordinate system position
    float machine_position[N_AXIS]; // Machine coordinate system position
    coord_mode_t coord_mode;        // Which coordinates to display

    // Feed and speed
    float feed_rate;                // Current feed rate (mm/min or in/min)
    float spindle_speed;            // Current spindle/laser speed (RPM or %)
    uint8_t feed_override;          // Feed override percentage (10-200)
    uint8_t spindle_override;       // Spindle override percentage (10-200)

    // Machine configuration
    uint8_t n_axis;                 // Number of axes (3-6)
    bool homed[N_AXIS];             // Homing status per axis
    bool limits_enabled;            // Limit switches enabled

    // File/Job status
    char current_file[64];          // Currently loaded/running file
    uint32_t file_size;             // File size in bytes
    uint32_t file_position;         // Current position in file
    uint8_t job_progress;           // Job completion percentage (0-100)

    // UI flags
    bool screen_dirty;              // Screen needs redraw
    bool touch_calibrated;          // Touch calibration complete
    uint8_t language;               // Current language (0=EN, 1=CN, 2=ES)

} ui_state_data_t;

// Global UI state (singleton)
extern ui_state_data_t ui_state;

/*
 * Initialize UI state
 */
void ui_state_init(void);

/*
 * Update system state from grblHAL event
 */
void ui_state_update_system_state(sys_state_t state);

/*
 * Update position from grblHAL realtime report
 */
void ui_state_update_position(float *work_pos, float *machine_pos);

/*
 * Update feed rate and spindle speed
 */
void ui_state_update_feed_spindle(float feed, float spindle);

/*
 * Update override percentages
 */
void ui_state_update_overrides(uint8_t feed_ovr, uint8_t spindle_ovr);

/*
 * Set alarm/error codes
 */
void ui_state_set_alarm(uint16_t alarm);
void ui_state_set_error(uint16_t error);

/*
 * Screen management
 */
void ui_state_set_screen(ui_screen_t screen);
ui_screen_t ui_state_get_screen(void);
ui_screen_t ui_state_get_previous_screen(void);

/*
 * Coordinate mode toggle
 */
void ui_state_toggle_coord_mode(void);
coord_mode_t ui_state_get_coord_mode(void);

/*
 * Homing status
 */
void ui_state_set_homed(uint8_t axis, bool homed);
bool ui_state_is_homed(uint8_t axis);
bool ui_state_all_homed(void);

/*
 * Job/File status
 */
void ui_state_set_file(const char *filename, uint32_t size);
void ui_state_update_file_position(uint32_t position);
void ui_state_clear_file(void);
uint8_t ui_state_get_job_progress(void);

/*
 * Screen dirty flag
 */
void ui_state_mark_dirty(void);
void ui_state_clear_dirty(void);
bool ui_state_is_dirty(void);

/*
 * Get axis name (X/Y/Z/A/B/C)
 */
char ui_state_get_axis_name(uint8_t axis);

/*
 * Get system state string ("Idle", "Run", "Alarm", etc.)
 */
const char* ui_state_get_state_string(void);

/*
 * Get a short description of the active alarm ("Hard limit", ...), or "" if none
 */
const char* ui_state_get_alarm_string(void);

#ifdef __cplusplus
}
#endif

#endif // _UI_STATE_H_
