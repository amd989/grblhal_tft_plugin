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
    char name[50];
    uint32_t size;
    uint32_t pos;                   // bytes read
    uint32_t line;
    uint32_t elapsed_ms;            // since the job started, or total once ended
} tft_sd_job_t;

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

/*
 * grblHAL loop (tft_interface.c)
 */
void tft_sd_poll(void);

#ifdef __cplusplus
}
#endif

#endif // _TFT_SD_H_
