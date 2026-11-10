/*
 * screen_settings.h - Brightness, touch beep, touch calibration and info
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_SETTINGS_H_
#define _SCREEN_SETTINGS_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_SETTINGS with the screen manager. Call from the UI task.
void screen_settings_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_SETTINGS_H_
