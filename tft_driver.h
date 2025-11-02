/*
 * tft_driver.h - TFT display hardware driver for grblHAL
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 * This file provides low-level hardware control for the ST7796 TFT display
 * via the TFT_eSPI library, including backlight PWM and beeper control.
 */

#ifndef _TFT_DRIVER_H_
#define _TFT_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize TFT display hardware
 * - Initializes TFT_eSPI library
 * - Configures SPI bus (40MHz display, 2MHz touch)
 * - Sets display rotation
 * - Enables DMA if configured
 * - Sets up backlight PWM
 * - Initializes beeper (if enabled)
 */
void tft_driver_init(void);

/*
 * Backlight Control
 */

// Set backlight brightness (0-255)
// 0 = off, 255 = full brightness
void tft_set_backlight(uint8_t level);

// Turn backlight on at specified level
void tft_backlight_on(uint8_t level);

// Turn backlight off
void tft_backlight_off(void);

// Breathing effect for startup (call repeatedly)
// Returns true when breathing cycle is complete
bool tft_backlight_breathing(void);

/*
 * Beeper Control
 */

#if TFT_BEEP_ENABLE

// Initialize beeper
void tft_beeper_init(void);

// Turn beeper on
void tft_beeper_on(void);

// Turn beeper off
void tft_beeper_off(void);

// Quick beep for touch feedback (non-blocking)
void tft_beep_touch(void);

#endif // TFT_BEEP_ENABLE

/*
 * Display Information
 */

// Get display width in pixels
uint16_t tft_get_width(void);

// Get display height in pixels
uint16_t tft_get_height(void);

// Get display rotation (0-3)
uint8_t tft_get_rotation(void);

#ifdef __cplusplus
}
#endif

#endif // _TFT_DRIVER_H_
