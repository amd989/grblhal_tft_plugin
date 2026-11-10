/*
 * tft_sd.c - SD card file listing and job status for the TFT UI
 *
 * Part of grblHAL TFT Plugin
 */

#include "driver.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "grbl/hal.h"
#include "grbl/vfs.h"
#include "grbl/stream_file.h"
#include "grbl/planner.h"

#if (FS_ENABLE & FS_SDCARD)
#include "sdcard/sdcard.h"
#include "sdcard/fs_stream.h"
#define TFT_SD_ENABLED 1
#else
#define TFT_SD_ENABLED 0
#endif

#include "tft_sd.h"
#include "tft_interface.h"

static tft_sd_listing_t listing;
static tft_sd_job_t job;

static char request_path[TFT_SD_PATH_LEN];
static volatile bool list_requested = false;

/*
 * UI task
 */

void tft_sd_list (const char *path)
{
    strncpy(request_path, path && *path ? path : "/", sizeof(request_path) - 1);
    request_path[sizeof(request_path) - 1] = '\0';
    __sync_synchronize();
    list_requested = true;
}

const tft_sd_listing_t *tft_sd_listing (void)
{
    return &listing;
}

bool tft_sd_run (const char *path)
{
    char cmd[TFT_CMD_MAX_LEN];

    if(snprintf(cmd, sizeof(cmd), "$F=%s", path) >= (int)sizeof(cmd))
        return false;

    tft_send_command(cmd);

    return true;
}

const tft_sd_job_t *tft_sd_job (void)
{
    return &job;
}

/*
 * grblHAL loop
 */

#if TFT_SD_ENABLED

// Same extensions grblHAL's $F lists
static bool is_gcode (const char *name)
{
    static const char *const types[] = { "nc", "ncc", "ngc", "cnc", "gcode", "txt", "text", "tap" };
    const char *ext = strrchr(name, '.');

    if(ext) for(uint_fast8_t idx = 0; idx < sizeof(types) / sizeof(types[0]); idx++) {
        if(!strcasecmp(ext + 1, types[idx]))
            return true;
    }

    return false;
}

// Folders first, then by name
static bool before (const tft_sd_entry_t *a, bool dir, const char *name)
{
    return a->dir != dir ? a->dir : strcasecmp(a->name, name) < 0;
}

static void add_entry (const char *name, bool dir, uint32_t size)
{
    uint_fast8_t idx = listing.count;

    if(listing.count == TFT_SD_MAX_ENTRIES) {
        listing.truncated = true;
        return;
    }

    // Insertion sort, the list is short
    while(idx > 0 && !before(&listing.entry[idx - 1], dir, name)) {
        listing.entry[idx] = listing.entry[idx - 1];
        idx--;
    }

    strcpy(listing.entry[idx].name, name);
    listing.entry[idx].dir = dir;
    listing.entry[idx].size = size;
    listing.count++;
}

static void read_listing (void)
{
    vfs_dir_t *dir = NULL;
    vfs_dirent_t *dirent;

    strcpy(listing.path, request_path);
    listing.count = 0;
    listing.truncated = false;

    // Mounts the card if it is not mounted yet, inserting one is not detected
    listing.mounted = sdcard_getfs() != NULL && (dir = vfs_opendir(listing.path)) != NULL;

    if(listing.mounted) {

        while((dirent = vfs_readdir(dir)) != NULL && dirent->name[0] != '\0' && !vfs_errno) {

            // Skip hidden files, including macOS "._" files
            if(dirent->name[0] == '.' || strlen(dirent->name) >= TFT_SD_NAME_LEN)
                continue;

            if(dirent->st_mode.directory || is_gcode(dirent->name))
                add_entry(dirent->name, dirent->st_mode.directory, (uint32_t)dirent->size);
        }

        vfs_closedir(dir);
    }

    __sync_synchronize();
    listing.seq++;
}

static void update_job (void)
{
    static uint32_t started_at;
    stream_job_t *info = stream_get_job_info();
    uint32_t now = hal.get_elapsed_ticks();

    if(info) {
        if(!job.active) {
            job.active = true;
            job.finished = false;
            job.seq++;
            started_at = now;
            strncpy(job.name, info->name, sizeof(job.name) - 1);
            job.name[sizeof(job.name) - 1] = '\0';
            tft_log("job started: %s", job.name);
        }
        uint32_t queued = plan_get_buffer_size() - plan_get_block_buffer_available();

        job.size = info->size;
        job.pos = info->pos;
        job.line = info->line;
        job.line_done = info->line > queued ? info->line - queued : 0;
        job.pos_done = info->line ? (uint32_t)((uint64_t)info->pos * job.line_done / info->line) : 0;
        job.elapsed_ms = now - started_at;
    } else if(job.active) {
        job.elapsed_ms = now - started_at;
        // A finished job waits for the motion buffer to empty before it ends,
        // a stopped one ends with moves still buffered
        job.finished = job.size && job.pos >= job.size && job.line_done >= job.line;
        if(job.finished) {
            job.pos_done = job.size;
            job.line_done = job.line;
        }
        __sync_synchronize();   // the UI reads the result once active is false
        job.active = false;
        tft_log("job ended at line %u", (unsigned)job.line);
    }
}

void tft_sd_poll (void)
{
    // Do not touch the card while a job reads from it
    if(list_requested && !stream_is_file()) {
        __sync_synchronize();
        list_requested = false;
        read_listing();
    }

    update_job();
}

#else // no SD card support in grblHAL

void tft_sd_poll (void)
{
    if(list_requested) {
        list_requested = false;
        strcpy(listing.path, request_path);
        listing.mounted = false;
        listing.count = 0;
        listing.seq++;
    }
}

#endif

#endif // TFT_ENABLE
