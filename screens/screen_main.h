/*
 * screen_main.h - Main screen: machine state, position readout and basic controls
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_MAIN_H_
#define _SCREEN_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_MAIN with the screen manager. Call from the UI task.
void screen_main_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_MAIN_H_
