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

// ---------------------------------------------------------------------------
// User settings. Defaults below match the MKS DLC32 v2.0/v2.1 with the MKS TS35
// display. Override any of them by defining it in my_machine.h.
// ---------------------------------------------------------------------------

// Display
// Landscape size as used by LVGL. TFT_WIDTH/TFT_HEIGHT in User_Setup.h are
// the panel's native portrait size, as TFT_eSPI expects.
#ifndef TFT_DISPLAY_WIDTH
#define TFT_DISPLAY_WIDTH       480
#endif
#ifndef TFT_DISPLAY_HEIGHT
#define TFT_DISPLAY_HEIGHT      320
#endif
#ifndef TFT_ROTATION
#define TFT_ROTATION            1       // 0-3
#endif

// LVGL
#ifndef LVGL_BUFFER_SIZE
#define LVGL_BUFFER_SIZE        (10 * 480)
#endif
#ifndef LVGL_REFRESH_PERIOD
#define LVGL_REFRESH_PERIOD     5       // ms
#endif

// Features
#ifndef TFT_BEEP_ENABLE
#define TFT_BEEP_ENABLE         0       // Needs grblHAL I2S output integration, off for now
#endif
#ifndef TFT_LED_ENABLE
#define TFT_LED_ENABLE          1
#endif
#ifndef TFT_LANGUAGE_DEFAULT
#define TFT_LANGUAGE_DEFAULT    1       // 0=Chinese, 1=English, 2=German
#endif

// Debug log of touches, button clicks and commands, printed as [MSG:TFT ...]
#ifndef TFT_DEBUG
#define TFT_DEBUG               0
#endif

// Board revision: v2.0/v2.1 backlight is active low
#ifndef USE_BOARD_V2_0
#define USE_BOARD_V2_0          1
#endif
#ifndef USE_LCD_DMA
#define USE_LCD_DMA             0       // TFT_eSPI DMA not wired up yet
#endif

// Display SPI pins
#ifndef TFT_MISO_PIN
#define TFT_MISO_PIN            19
#endif
#ifndef TFT_MOSI_PIN
#define TFT_MOSI_PIN            23
#endif
#ifndef TFT_SCLK_PIN
#define TFT_SCLK_PIN            18
#endif
#ifndef TFT_CS_PIN
#define TFT_CS_PIN              25      // Chip select
#endif
#ifndef TFT_DC_PIN
#define TFT_DC_PIN              33      // Data/Command (RS)
#endif
#ifndef TFT_RST_PIN
#define TFT_RST_PIN             27      // Reset
#endif
#ifndef TFT_BL_PIN
#define TFT_BL_PIN              5       // Backlight enable
#endif

// Touch controller (XPT2046)
#ifndef TOUCH_CS_PIN
#define TOUCH_CS_PIN            26
#endif

// SPI frequencies
#ifndef TFT_SPI_FREQUENCY
#define TFT_SPI_FREQUENCY       40000000    // 40 MHz display writes
#endif
#ifndef TFT_SPI_READ_FREQUENCY
#define TFT_SPI_READ_FREQUENCY  20000000    // 20 MHz display reads
#endif
#ifndef TOUCH_SPI_FREQUENCY
#define TOUCH_SPI_FREQUENCY     2000000     // 2 MHz touch
#endif

// Backlight PWM (LEDC)
#ifndef TFT_BL_PWM_CHANNEL
#define TFT_BL_PWM_CHANNEL      14
#endif
#ifndef TFT_BL_PWM_FREQ
#define TFT_BL_PWM_FREQ         1000        // Hz
#endif
#ifndef TFT_BL_PWM_BITS
#define TFT_BL_PWM_BITS         10          // 0-1023
#endif

// Beeper (via I2S GPIO expansion), only used when TFT_BEEP_ENABLE is 1
// #define TFT_BEEP_PIN         I2SO(7)

#ifdef __cplusplus
extern "C" {
#endif

// Display Specifications
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
#if USE_BOARD_V2_0
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

// Touch Coordinate Transformation (TS35 needs none)
#ifndef TFT_TOUCH_MIRROR_X
#define TFT_TOUCH_MIRROR_X      0
#endif
#ifndef TFT_TOUCH_MIRROR_Y
#define TFT_TOUCH_MIRROR_Y      0
#endif

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
