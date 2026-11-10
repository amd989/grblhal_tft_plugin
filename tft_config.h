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
#define TFT_BEEP_ENABLE         1       // Click on each touch
#endif
#ifndef TFT_BEEP_MS
#define TFT_BEEP_MS             20      // Click length
#endif
#ifndef TFT_LED_ENABLE
#define TFT_LED_ENABLE          1
#endif
#ifndef TFT_LANGUAGE_DEFAULT
#define TFT_LANGUAGE_DEFAULT    1       // 0=Chinese, 1=English, 2=German
#endif

// Jog feed rates, mm/min
#ifndef TFT_JOG_FEED
#define TFT_JOG_FEED            1000
#endif
#ifndef TFT_JOG_FEED_Z
#define TFT_JOG_FEED_Z          300
#endif

// Z probe with a touch plate (jog screen): plate thickness in mm, how far
// down to search, fast and slow feed (mm/min), and lift above the plate after
#ifndef TFT_PROBE_THICKNESS
#define TFT_PROBE_THICKNESS     0.0f
#endif
#ifndef TFT_PROBE_DISTANCE
#define TFT_PROBE_DISTANCE      50.0f
#endif
#ifndef TFT_PROBE_FEED
#define TFT_PROBE_FEED          150
#endif
#ifndef TFT_PROBE_FEED_SLOW
#define TFT_PROBE_FEED_SLOW     25
#endif
#ifndef TFT_PROBE_RETRACT
#define TFT_PROBE_RETRACT       5.0f
#endif

// Spindle speed for the Spindle button on the jog screen (M3 S...)
#ifndef TFT_SPINDLE_RPM
#define TFT_SPINDLE_RPM         10000
#endif

// Macro buttons: defaults for settings $450-$455, "Label|G-code|G-code...".
// Change them with $450=... from a console, an empty setting hides the button.
#ifndef TFT_MACRO_1
#define TFT_MACRO_1             "Safe Z|G53 G0 Z-1"
#endif
#ifndef TFT_MACRO_2
#define TFT_MACRO_2             "Park|G53 G0 Z-1|G28"
#endif
#ifndef TFT_MACRO_3
#define TFT_MACRO_3             "Change pos|G53 G0 Z-1|G30"
#endif
#ifndef TFT_MACRO_4
#define TFT_MACRO_4             "Save park|G28.1"
#endif
#ifndef TFT_MACRO_5
#define TFT_MACRO_5             "Save change|G30.1"
#endif
#ifndef TFT_MACRO_6
#define TFT_MACRO_6             ""
#endif

// Resume from line: the job is copied from that line to this file on the SD
// card, after a few lines that restore the modal state and position. The
// spindle gets TFT_RESUME_DWELL seconds to spin up, and the bit goes down to
// the cut from TFT_RESUME_CLEARANCE mm above it.
#ifndef TFT_RESUME_FILE
#define TFT_RESUME_FILE         "/tft_resume.nc"
#endif
#ifndef TFT_RESUME_DWELL
#define TFT_RESUME_DWELL        3
#endif
#ifndef TFT_RESUME_CLEARANCE
#define TFT_RESUME_CLEARANCE    2.0f
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

// Beeper output, a shift register bit on the DLC32. The plugin board map
// leaves it free (the stock map used it for mist coolant).
#ifndef TFT_BEEP_PIN
#define TFT_BEEP_PIN            I2SO(7)
#endif

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

// Touch Controller: XPT2046
#define TFT_TOUCH_XPT2046       1

// Touch Coordinate Transformation (TS35 needs none)
#ifndef TFT_TOUCH_MIRROR_X
#define TFT_TOUCH_MIRROR_X      0
#endif
#ifndef TFT_TOUCH_MIRROR_Y
#define TFT_TOUCH_MIRROR_Y      0
#endif

// Touch calibration used until one is saved with $TFTCAL: raw XPT2046 values
// at the screen edges (left, right, top, bottom), and whether the raw axes
// are swapped. Measured on an MKS TS35 with $TFTCAL; other panels may differ.
#ifndef TFT_TOUCH_CAL_X0
#define TFT_TOUCH_CAL_X0        3930
#endif
#ifndef TFT_TOUCH_CAL_X1
#define TFT_TOUCH_CAL_X1        196
#endif
#ifndef TFT_TOUCH_CAL_Y0
#define TFT_TOUCH_CAL_Y0        160
#endif
#ifndef TFT_TOUCH_CAL_Y1
#define TFT_TOUCH_CAL_Y1        3875
#endif
#ifndef TFT_TOUCH_CAL_SWAP
#define TFT_TOUCH_CAL_SWAP      1
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
