/*
 * screen_job.h - Running job: progress, overrides, hold and stop
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _SCREEN_JOB_H_
#define _SCREEN_JOB_H_

#ifdef __cplusplus
extern "C" {
#endif

// Register SCREEN_JOB with the screen manager. Call from the UI task.
void screen_job_register(void);

#ifdef __cplusplus
}
#endif

#endif // _SCREEN_JOB_H_
