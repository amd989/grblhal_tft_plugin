/*
 * screen_tools.h - Tools tab: spindle, screen settings, calibration, info
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_TOOLS_H_
#define _SCREEN_TOOLS_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_TOOL with the screen manager. Call from the UI task.
void screen_tools_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_TOOLS_H_
