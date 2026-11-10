/*
 * tft_sd.h - SD card file listing and job status for the TFT UI
 *
 * Part of grblHAL TFT Plugin
 *
 * Needs grblHAL's SD card plugin (SDCARD_ENABLE in my_machine.h). Without it
 * listings report the card as missing and no job is ever active.
 *
 * FatFs is not thread safe, so the card is only read from grblHAL's loop
 * (tft_sd_poll). The UI task requests a listing and waits for listing.seq
 * to change.
 */

#ifndef _TFT_SD_H_
#define _TFT_SD_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFT_SD_MAX_ENTRIES  64
#define TFT_SD_NAME_LEN     41      // grblHAL cannot run files with longer names
#define TFT_SD_PATH_LEN     80      // directory path

typedef struct {
    char name[TFT_SD_NAME_LEN];
    bool dir;
    uint32_t size;
} tft_sd_entry_t;

typedef struct {
    volatile uint32_t seq;          // incremented when a listing is complete
    bool mounted;                   // false: no card, or it could not be read
    bool truncated;                 // more than TFT_SD_MAX_ENTRIES entries
    uint8_t count;
    char path[TFT_SD_PATH_LEN];     // "/" or "/dir/sub"
    tft_sd_entry_t entry[TFT_SD_MAX_ENTRIES];   // folders first, then files, by name
} tft_sd_listing_t;

typedef struct {
    bool active;                    // a file is being streamed from the card
    uint32_t seq;                   // incremented when a job starts
    char name[50];                  // file name, "name from line N" for a resumed job
    char source[TFT_SD_NAME_LEN];   // file name of the job, also for a resumed one
    uint32_t size;
    uint32_t pos;                   // bytes read
    uint32_t line;                  // lines read, numbered as in the source file
    // grblHAL reads up to a full motion buffer (100 moves) ahead. line_done
    // is the line of the move in progress (from tagged planner blocks), in
    // an arc it can be the line before. Resume from line suggests it.
    uint32_t line_done;
    uint32_t pos_done;              // bytes read, scaled by line_done / line
    bool finished;                  // ended after running to the end, not stopped
    uint32_t elapsed_ms;            // since the job started, or total once ended
} tft_sd_job_t;

typedef struct {
    volatile uint32_t seq;          // incremented when done, ok or not
    bool ok;                        // started the job
    uint8_t progress;               // percent of the file done while preparing
    char error[48];                 // why not, when !ok
} tft_sd_resume_t;

typedef struct {
    volatile uint32_t seq;          // incremented when a scan is complete
    bool ok;                        // X/Y moves found
    uint8_t progress;               // percent read while scanning
    float min_x, min_y, max_x, max_y;   // work coordinates, mm
} tft_sd_bounds_t;

/*
 * UI task
 */

// Request a listing of path ("/" for the root). Mounts the card if needed.
void tft_sd_list(const char *path);

// Last completed listing
const tft_sd_listing_t *tft_sd_listing(void);

// Run a file with $F=path. False if the path is too long.
bool tft_sd_run(const char *path);

// Current or last job
const tft_sd_job_t *tft_sd_job(void);

// Find the X/Y extents of a file's moves. Relative moves are taken from
// x, y, the current work position. Read a bit at a time in grblHAL's loop.
void tft_sd_scan(const char *path, float x, float y);

// Last completed scan, progress of the current one
const tft_sd_bounds_t *tft_sd_bounds(void);

// Run path from line (1-based, numbered like the job screen). The modal
// state (units, distance mode, work offset, plane, feed, spindle, coolant)
// and position before that line are read from the file. A copy is written
// to TFT_RESUME_FILE: up to the Z home position, over the start point,
// spindle on, down to the cut, then the file from that line. Needs Z homed.
void tft_sd_resume(const char *path, uint32_t line);

// Result of the last resume, progress of the current one
const tft_sd_resume_t *tft_sd_resume_status(void);

/*
 * grblHAL loop (tft_interface.c)
 */
void tft_sd_poll(void);

#ifdef __cplusplus
}
#endif

#endif // _TFT_SD_H_
