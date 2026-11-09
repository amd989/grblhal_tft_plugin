/*
 * tft_interface.h - Communication interface between TFT UI and grblHAL
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 */

#ifndef _TFT_INTERFACE_H_
#define _TFT_INTERFACE_H_

#include "grbl/system.h"
#include "grbl/settings.h"
#include "tft_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Setup - creates the command queue and hooks grblHAL's foreground loop.
 * Call once from plugin init.
 */
void tft_interface_init(void);

/*
 * Debug log - safe from any task. Lines are printed as [MSG:TFT ...] from
 * grblHAL's loop. Compiled out unless TFT_DEBUG is 1.
 */
#if TFT_DEBUG
void tft_log(const char *fmt, ...);
#else
#define tft_log(...)
#endif

/*
 * Command Injection - Send G-code commands to grblHAL.
 * Safe to call from the UI task: lines are queued and handed to grblHAL
 * from its own loop.
 */

// Send raw G-code command
void tft_send_command(const char *cmd);

// Send formatted G-code command
void tft_send_command_fmt(const char *fmt, ...);

/*
 * Motion Commands
 */

// Jog a single axis
void tft_jog_axis(uint8_t axis, float distance, float speed);

// Cancel active jog
void tft_jog_cancel(void);

// Home all axes
void tft_home_all(void);

// Home single axis
void tft_home_axis(uint8_t axis);

/*
 * Work Coordinate Commands
 */

// Zero a single axis work coordinate
void tft_zero_axis(uint8_t axis);

// Zero all axes work coordinates
void tft_zero_all(void);

// Set work coordinate to specific value
void tft_set_axis_value(uint8_t axis, float value);

/*
 * Real-time Commands
 */

// Feed hold (pause)
void tft_feed_hold(void);

// Cycle start (resume)
void tft_cycle_start(void);

// Stop
void tft_stop(void);

// Soft reset
void tft_reset(void);

// Clear alarm lock ($X)
void tft_unlock(void);

/*
 * State Queries
 */

// Get current machine state
sys_state_t tft_get_state(void);

// Get alarm code
alarm_code_t tft_get_alarm(void);

// Get error code
uint8_t tft_get_error(void);

// Check if homing is enabled
bool tft_is_homing_enabled(void);

// Check if limits are enabled
bool tft_is_limits_enabled(void);

/*
 * SD Card Commands (if SD card plugin is loaded)
 */

// Start SD card job
bool tft_sd_start_job(const char *filename);

// Get SD card job progress (0-100)
uint8_t tft_sd_get_progress(void);

// Check if SD card is mounted
bool tft_sd_is_mounted(void);

/*
 * Settings Commands
 */

// Get setting value
float tft_get_setting(setting_id_t id);

// Set setting value
bool tft_set_setting(setting_id_t id, float value);

// Reset settings to defaults
void tft_reset_settings(void);

#ifdef __cplusplus
}
#endif

#endif // _TFT_INTERFACE_H_
