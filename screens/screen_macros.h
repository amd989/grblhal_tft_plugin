/*
 * screen_macros.h - Macro buttons
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_MACROS_H_
#define _SCREEN_MACROS_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_MACROS with the screen manager. Call from the UI task.
void screen_macros_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_MACROS_H_
