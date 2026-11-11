/*
 * tft_settings.h - Screen settings: brightness ($456), touch beep ($457),
 *                  confirm by tapping twice or holding ($458)
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _TFT_SETTINGS_H_
#define _TFT_SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFT_BRIGHTNESS_MIN  10      // percent, so the screen never goes dark

// Register the settings, from plugin init
void tft_settings_init(void);

// Brightness in percent, touch beep and confirm by holding ($458), including
// a change not saved yet
uint8_t tft_settings_brightness(void);
bool tft_settings_beep(void);
bool tft_settings_confirm_hold(void);

// UI task: change a setting. grblHAL's loop stores it (tft_settings_poll),
// grblHAL writes it to flash once the machine is idle.
void tft_settings_set_brightness(uint8_t percent);
void tft_settings_set_beep(bool on);
void tft_settings_set_confirm_hold(bool on);

// grblHAL loop (tft_interface.c)
void tft_settings_poll(void);

#ifdef __cplusplus
}
#endif

#endif // _TFT_SETTINGS_H_
