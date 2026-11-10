/*
 * screen_calibrate.h - Touch calibration
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_CALIBRATE_H_
#define _SCREEN_CALIBRATE_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_CALIBRATE with the screen manager. Call from the UI task.
void screen_calibrate_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_CALIBRATE_H_
