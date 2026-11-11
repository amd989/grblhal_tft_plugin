/*
 * screen_probe.h - Probe tab: touch plate and tool setter
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_PROBE_H_
#define _SCREEN_PROBE_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_PROBE with the screen manager. Call from the UI task.
void screen_probe_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_PROBE_H_
