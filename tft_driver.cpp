/*
 * tft_driver.c - TFT display hardware driver implementation
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 * Ported from MKS_TS35.cpp in grbl_esp32 firmware
 */

// Include our custom User_Setup.h before TFT_eSPI
#include "User_Setup.h"
#include <TFT_eSPI.h>
#include "tft_config.h"
#include "tft_driver.h"

#if TFT_ENABLE

// TFT_eSPI instance (global, used by lvgl_init.cpp)
TFT_eSPI tft = TFT_eSPI();

// Backlight state
static struct {
    uint8_t level;              // Current brightness (0-255)
    uint8_t breathing_value;    // Breathing effect counter
    bool breathing_up;          // Breathing direction
} backlight_state = {0};

/*
 * Initialize TFT display hardware
 */
void tft_driver_init(void) {
    // Initialize backlight PWM
    ledcSetup(TFT_BACKLIGHT_CHANNEL, TFT_BACKLIGHT_FREQ, TFT_BACKLIGHT_BITS);
    ledcAttachPin(TFT_PIN_BL, TFT_BACKLIGHT_CHANNEL);

    // Turn off backlight initially
    tft_backlight_off();

    // Initialize TFT_eSPI
    tft.begin();
    tft.setRotation(TFT_DISPLAY_ROTATION);

#if TFT_USE_DMA
    // Enable DMA for faster transfers
    tft.initDMA();
#endif

    // Give display time to settle
    delay(100);

#if TFT_BEEP_ENABLE
    // Initialize beeper
    tft_beeper_init();
#endif
}

/*
 * Backlight Control Functions
 */

void tft_set_backlight(uint8_t level) {
    backlight_state.level = level;

    // Map 0-255 to PWM duty cycle
    uint32_t duty = (uint32_t)level * TFT_BACKLIGHT_MAX / 255;

#ifdef USE_BOARD_V2_0
    // V2.0 boards: backlight is active LOW
    duty = TFT_BACKLIGHT_MAX - duty;
#endif

    ledcWrite(TFT_BACKLIGHT_CHANNEL, duty);
}

void tft_backlight_on(uint8_t level) {
    tft_set_backlight(level);
}

void tft_backlight_off(void) {
    tft_set_backlight(0);
}

bool tft_backlight_breathing(void) {
    // Breathing effect: fade in and out
    if(backlight_state.breathing_up) {
        backlight_state.breathing_value += TFT_BREATHING_STEP;
        if(backlight_state.breathing_value >= TFT_BREATHING_MAX) {
            backlight_state.breathing_value = TFT_BREATHING_MAX;
            backlight_state.breathing_up = false;
        }
    } else {
        if(backlight_state.breathing_value > TFT_BREATHING_STEP) {
            backlight_state.breathing_value -= TFT_BREATHING_STEP;
        } else {
            backlight_state.breathing_value = TFT_BREATHING_MIN;
            backlight_state.breathing_up = true;
            // One full cycle complete
            return true;
        }
    }

    tft_set_backlight(backlight_state.breathing_value);
    return false;
}

/*
 * Beeper Control Functions
 */

#if TFT_BEEP_ENABLE

void tft_beeper_init(void) {
    // Beeper is on I2S GPIO expansion
    // Initialization handled by driver.c I2S setup
    // Just ensure pin mode is set
    pinMode(TFT_BEEPER_PIN, OUTPUT);
    tft_beeper_off();
}

void tft_beeper_on(void) {
    digitalWrite(TFT_BEEPER_PIN, HIGH);
}

void tft_beeper_off(void) {
    digitalWrite(TFT_BEEPER_PIN, LOW);
}

void tft_beep_touch(void) {
    // Quick beep for touch feedback
    // This is called from touch callback
    // Actual timing controlled by LVGL/touch driver
    tft_beeper_on();
}

#endif // TFT_BEEP_ENABLE

/*
 * Display Information
 */

uint16_t tft_get_width(void) {
    return tft.width();
}

uint16_t tft_get_height(void) {
    return tft.height();
}

uint8_t tft_get_rotation(void) {
    return tft.getRotation();
}

#endif // TFT_ENABLE
