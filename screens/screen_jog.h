/*
 * screen_jog.h - Jog screen: step jogging of X/Y/Z, step size, set work zero
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_JOG_H_
#define _SCREEN_JOG_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_CONTROL with the screen manager. Call from the UI task.
void screen_jog_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_JOG_H_
