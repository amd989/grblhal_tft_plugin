/*
 * tft_interface.c - Communication interface between TFT UI and grblHAL
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 */

#include "driver.h"

#if TFT_ENABLE

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "grbl/hal.h"
#include "grbl/state_machine.h"
#include "grbl/system.h"
#include "grbl/settings.h"

#include "tft_interface.h"

/*
 * Command Injection
 */

void tft_send_command(const char *cmd) {
    hal.stream.write(cmd);
}

void tft_send_command_fmt(const char *fmt, ...) {
    char buffer[128];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    hal.stream.write(buffer);
}

/*
 * Motion Commands
 */

void tft_jog_axis(uint8_t axis, float distance, float speed) {
    if(axis >= N_AXIS)
        return;

    char axis_letter = 'X' + axis;
    tft_send_command_fmt("$J=G91 %c%.3f F%.0f\n", axis_letter, distance, speed);
}

void tft_jog_cancel(void) {
    hal.stream.enqueue_rt_command(CMD_JOG_CANCEL);
}

void tft_home_all(void) {
    tft_send_command("$H\n");
}

void tft_home_axis(uint8_t axis) {
    if(axis >= N_AXIS)
        return;

    char axis_letter = 'X' + axis;
    tft_send_command_fmt("$H%c\n", axis_letter);
}

/*
 * Work Coordinate Commands
 */

void tft_zero_axis(uint8_t axis) {
    if(axis >= N_AXIS)
        return;

    char axis_letter = 'X' + axis;
    tft_send_command_fmt("G10 L20 P0 %c0\n", axis_letter);
}

void tft_zero_all(void) {
    // Build command for all active axes
    char cmd[64] = "G10 L20 P0";
    char *p = cmd + strlen(cmd);

    for(uint8_t axis = 0; axis < N_AXIS; axis++) {
        p += sprintf(p, " %c0", 'X' + axis);
    }
    *p++ = '\n';
    *p = '\0';

    tft_send_command(cmd);
}

void tft_set_axis_value(uint8_t axis, float value) {
    if(axis >= N_AXIS)
        return;

    char axis_letter = 'X' + axis;
    tft_send_command_fmt("G10 L20 P0 %c%.3f\n", axis_letter, value);
}

/*
 * Real-time Commands
 */

void tft_feed_hold(void) {
    hal.stream.enqueue_rt_command(CMD_FEED_HOLD);
}

void tft_cycle_start(void) {
    hal.stream.enqueue_rt_command(CMD_CYCLE_START);
}

void tft_stop(void) {
    hal.stream.enqueue_rt_command(CMD_STOP);
}

void tft_reset(void) {
    hal.stream.enqueue_rt_command(CMD_RESET);
}

/*
 * State Queries
 */

sys_state_t tft_get_state(void) {
    return state_get();
}

alarm_code_t tft_get_alarm(void) {
    return sys.alarm;
}

uint8_t tft_get_error(void) {
    // grblHAL doesn't store last error code in sys structure
    // Return 0 for now - errors are reported via messages
    return 0;
}

bool tft_is_homing_enabled(void) {
    return settings.homing.flags.enabled;
}

bool tft_is_limits_enabled(void) {
    return settings.limits.flags.hard_enabled;
}

/*
 * SD Card Commands
 */

bool tft_sd_start_job(const char *filename) {
    if(!filename || !*filename)
        return false;

    tft_send_command_fmt("$SD/Run=%s\n", filename);
    return true;
}

uint8_t tft_sd_get_progress(void) {
    // This would need integration with SD card plugin
    // For now, return 0
    // TODO: Add SD card progress tracking
    return 0;
}

bool tft_sd_is_mounted(void) {
    // This would need integration with SD card plugin
    // For now, return false
    // TODO: Add SD card detection
    return false;
}

/*
 * Settings Commands
 */

float tft_get_setting(setting_id_t id) {
    const setting_detail_t *setting = setting_get_details(id, NULL);
    if(!setting)
        return 0.0f;

    // Get value based on type
    switch(setting->datatype) {
        case Format_Int8:
        case Format_Int16:
        case Format_Integer:
        case Format_Bool:
        case Format_Bitfield:
        case Format_XBitfield:
        case Format_AxisMask:
        case Format_RadioButtons:
            return (float)setting_get_int_value(setting, 0);

        case Format_Decimal:
            return setting_get_float_value(setting, 0);

        default:
            return 0.0f;
    }
}

bool tft_set_setting(setting_id_t id, float value) {
    // Format setting command
    tft_send_command_fmt("$%d=%.3f\n", (uint32_t)id, value);
    return true;
}

void tft_reset_settings(void) {
    tft_send_command("$RST=$\n");
}

#endif // TFT_ENABLE
