/*
 * ui_chrome.h - Status bar, tabs and action bar shared by the screens
 *
 * Part of grblHAL TFT Plugin
 *
 * Drawn once on LVGL's top layer, above every screen:
 *   top     state, then the reason for an alarm, hold or tool change, or
 *           the position, then spindle and network badges
 *   left    tabs: Jog, Files, Probe, Macros, Tools
 *   right   Hold/Resume, Home, Unlock, Reset (confirmed)
 * The Job screen keeps only the status bar, so a job cannot be left from
 * the screen. Splash and touch calibration show none of it.
 * Runs in the UI task only.
 */

#ifndef _UI_CHROME_H_
#define _UI_CHROME_H_

#include "ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// The home screen, Jog
#define SCREEN_HOME SCREEN_CONTROL

// Create, hidden until the first screen is shown
void ui_chrome_init(void);

// Called by the screen manager when a screen is shown
void ui_chrome_set_screen(ui_screen_t screen);

// Refresh from the machine state, call from the UI loop
void ui_chrome_update(void);

#ifdef __cplusplus
}
#endif

#endif // _UI_CHROME_H_
