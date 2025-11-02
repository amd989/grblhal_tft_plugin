/*
 * tft_config.h - Configuration header for TFT display hardware
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 * This file centralizes all TFT display and LVGL configuration settings.
 */

#ifndef _TFT_CONFIG_H_
#define _TFT_CONFIG_H_

// Import configuration directly from my_machine.h (avoid pulling in driver.h FreeRTOS headers)
#include "my_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display Specifications
#define TFT_DISPLAY_WIDTH       TFT_WIDTH       // 480 pixels
#define TFT_DISPLAY_HEIGHT      TFT_HEIGHT      // 320 pixels
#define TFT_DISPLAY_ROTATION    TFT_ROTATION    // 1 = landscape

// LVGL Buffer Configuration
#define TFT_LVGL_BUFFER_SIZE    LVGL_BUFFER_SIZE    // 4800 pixels (10 rows)
#define TFT_LVGL_REFRESH_MS     LVGL_REFRESH_PERIOD // 5ms task cycle

// Display Driver: ST7796
#define TFT_DRIVER_ST7796       1

// Hardware Pins (from my_machine.h)
#define TFT_PIN_MISO    TFT_MISO_PIN
#define TFT_PIN_MOSI    TFT_MOSI_PIN
#define TFT_PIN_SCLK    TFT_SCLK_PIN
#define TFT_PIN_CS      TFT_CS_PIN
#define TFT_PIN_DC      TFT_DC_PIN
#define TFT_PIN_RST     TFT_RST_PIN
#define TFT_PIN_BL      TFT_BL_PIN

#define TOUCH_PIN_CS    TOUCH_CS_PIN

// SPI Frequencies (from my_machine.h)
#define TFT_SPI_FREQ        TFT_SPI_FREQUENCY
#define TFT_SPI_READ_FREQ   TFT_SPI_READ_FREQUENCY
#define TOUCH_SPI_FREQ      TOUCH_SPI_FREQUENCY

// Backlight PWM (from my_machine.h)
#define TFT_BACKLIGHT_CHANNEL   TFT_BL_PWM_CHANNEL
#define TFT_BACKLIGHT_FREQ      TFT_BL_PWM_FREQ
#define TFT_BACKLIGHT_BITS      TFT_BL_PWM_BITS
#define TFT_BACKLIGHT_MAX       ((1 << TFT_BACKLIGHT_BITS) - 1)  // 1023 for 10-bit

// Backlight polarity (board-specific)
#ifdef USE_BOARD_V2_0
    #define TFT_BACKLIGHT_ON(duty)  ledcWrite(TFT_BACKLIGHT_CHANNEL, TFT_BACKLIGHT_MAX - (duty))
    #define TFT_BACKLIGHT_OFF()     ledcWrite(TFT_BACKLIGHT_CHANNEL, TFT_BACKLIGHT_MAX)
#else
    #define TFT_BACKLIGHT_ON(duty)  ledcWrite(TFT_BACKLIGHT_CHANNEL, (duty))
    #define TFT_BACKLIGHT_OFF()     ledcWrite(TFT_BACKLIGHT_CHANNEL, 0)
#endif

// Beeper Configuration
#if TFT_BEEP_ENABLE
    #define TFT_BEEPER_PIN      TFT_BEEP_PIN
    // Beeper macros will be defined in tft_driver.h
#endif

// Touch Controller: XPT2046
#define TFT_TOUCH_XPT2046       1

// Touch Coordinate Transformation
// (MKS TS35 requires mirrored X and Y axes)
#define TFT_TOUCH_MIRROR_X      1
#define TFT_TOUCH_MIRROR_Y      1

// Color Depth
#define TFT_COLOR_DEPTH         16      // RGB565

// DMA Configuration
#if USE_LCD_DMA
    #define TFT_USE_DMA         1
#else
    #define TFT_USE_DMA         0
#endif

// Task Configuration
#define TFT_TASK_STACK_SIZE     (8192)  // 8 KB stack
#define TFT_TASK_PRIORITY       2       // Moderate priority
#define TFT_TASK_CORE           0       // Core 0 for UI (Core 1 for motion)

// Splash Screen Timing
#define TFT_SPLASH_BACKLIGHT_DELAY_MS   500     // Turn on backlight after 500ms
#define TFT_SPLASH_DURATION_MS          2000    // Show splash for 2 seconds

// Breathing Effect Configuration
#define TFT_BREATHING_STEP      15      // PWM step size
#define TFT_BREATHING_MIN       0       // Minimum brightness
#define TFT_BREATHING_MAX       255     // Maximum brightness

#ifdef __cplusplus
}
#endif

#endif // _TFT_CONFIG_H_
