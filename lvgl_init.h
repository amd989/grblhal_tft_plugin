/*
 * lvgl_init.h - LVGL graphics library initialization for grblHAL
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 * This file provides LVGL v6.x initialization and driver callbacks
 * for the TFT display and touch input.
 */

#ifndef _LVGL_INIT_H_
#define _LVGL_INIT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize LVGL graphics library
 * - Calls lv_init()
 * - Allocates display buffer
 * - Registers display driver with flush callback
 * - Registers touch input driver with read callback
 * - Must be called after tft_driver_init()
 */
void lvgl_init(void);

/*
 * LVGL task handler - must be called periodically
 * Typically called every 5ms from UI task
 */
void lvgl_task_handler(void);

#ifdef __cplusplus
}
#endif

#endif // _LVGL_INIT_H_
