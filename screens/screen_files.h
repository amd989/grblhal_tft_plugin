/*
 * screen_files.h - SD card file browser
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_FILES_H_
#define _SCREEN_FILES_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_FILES with the screen manager. Call from the UI task.
void screen_files_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_FILES_H_
