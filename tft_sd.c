/*
 * tft_sd.c - SD card file listing and job status for the TFT UI
 *
 * Part of grblHAL TFT Plugin
 */

#include "driver.h"

#if TFT_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

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
static tft_sd_bounds_t bounds;

static char request_path[TFT_SD_PATH_LEN];
static volatile bool list_requested = false;

static char scan_path[TFT_SD_PATH_LEN + TFT_SD_NAME_LEN + 1];
static float scan_start_x, scan_start_y;
static volatile bool scan_requested = false;

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

void tft_sd_scan (const char *path, float x, float y)
{
    strncpy(scan_path, path, sizeof(scan_path) - 1);
    scan_path[sizeof(scan_path) - 1] = '\0';
    scan_start_x = x;
    scan_start_y = y;
    bounds.progress = 0;
    __sync_synchronize();
    scan_requested = true;
}

const tft_sd_bounds_t *tft_sd_bounds (void)
{
    return &bounds;
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

/*
 * Frame scan: a small G-code reader for the X/Y extents of moves
 */

#define SCAN_CHUNK      512
#define SCAN_LINE_LEN   128

static struct {
    vfs_file_t *file;
    uint32_t size, read;
    char line[SCAN_LINE_LEN];
    uint_fast8_t len;
    bool in_comment, skip_line;
    bool absolute, inches, plane_xy, found;
    uint_fast8_t motion;
    float x, y;
} scan;

static void extend (float x, float y)
{
    if(!scan.found) {
        bounds.min_x = bounds.max_x = x;
        bounds.min_y = bounds.max_y = y;
        scan.found = true;
    } else {
        if(x < bounds.min_x) bounds.min_x = x;
        if(x > bounds.max_x) bounds.max_x = x;
        if(y < bounds.min_y) bounds.min_y = y;
        if(y > bounds.max_y) bounds.max_y = y;
    }
}

// Arc extents: the end point, plus every 90 degree point the arc passes
static void extend_arc (float x0, float y0, float x1, float y1, float cx, float cy, bool cw)
{
    const float two_pi = 6.2831853f;
    float r = hypotf(x0 - cx, y0 - cy);
    float a0 = atan2f(y0 - cy, x0 - cx), a1 = atan2f(y1 - cy, x1 - cx);
    // Angle swept counterclockwise from a0, a full turn if start and end meet
    float sweep = cw ? a0 - a1 : a1 - a0;

    while(sweep <= 0.0001f)
        sweep += two_pi;

    for(uint_fast8_t q = 0; q < 4; q++) {
        float a = q * (two_pi / 4.0f);
        float d = cw ? a0 - a : a - a0;
        while(d < 0.0f)
            d += two_pi;
        while(d >= two_pi)
            d -= two_pi;
        if(d <= sweep)
            extend(cx + r * cosf(a), cy + r * sinf(a));
    }

    extend(x1, y1);
}

// Decimal number as G-code writes it. Not strtof(), which reads "0X10" as hex.
static bool read_number (char **s, float *value)
{
    char *p = *s;
    bool neg = false, digits = false;
    float v = 0.0f, scale = 1.0f;

    if(*p == '-' || *p == '+')
        neg = *p++ == '-';

    while(*p >= '0' && *p <= '9') {
        v = v * 10.0f + (*p++ - '0');
        digits = true;
    }

    if(*p == '.') {
        p++;
        while(*p >= '0' && *p <= '9') {
            scale *= 0.1f;
            v += (*p++ - '0') * scale;
            digits = true;
        }
    }

    if(digits) {
        *value = neg ? -v : v;
        *s = p;
    }

    return digits;
}

static void scan_line (char *s)
{
    bool has_x = false, has_y = false, has_i = false, has_j = false, skip = false;
    float vx = 0.0f, vy = 0.0f, vi = 0.0f, vj = 0.0f;

    while(*s) {
        char letter = *s++;
        float value;

        if(!read_number(&s, &value))
            continue;

        switch(letter) {

            case 'G': {
                int g = (int)value, g10 = (int)lroundf(value * 10.0f);
                if(g10 == 900 || g10 == 910)
                    ;   // G90.1 / G91.1 arc center modes, IJ incremental assumed
                else if(g <= 3)
                    scan.motion = g;
                else if(g == 17 || g == 18 || g == 19)
                    scan.plane_xy = g == 17;
                else if(g == 20 || g == 21)
                    scan.inches = g == 20;
                else if(g == 90 || g == 91)
                    scan.absolute = g == 90;
                else if(g == 10 || g == 28 || g == 30 || g == 38 || g == 53 || g == 92 || g == 4)
                    skip = true;    // coordinates that are not a move in work coordinates
                } break;

            case 'X': has_x = true; vx = value; break;
            case 'Y': has_y = true; vy = value; break;
            case 'I': has_i = true; vi = value; break;
            case 'J': has_j = true; vj = value; break;
            default: break;
        }
    }

    if(skip || !(has_x || has_y) || scan.motion > 3)
        return;

    float k = scan.inches ? 25.4f : 1.0f;
    float x = has_x ? (scan.absolute ? vx * k : scan.x + vx * k) : scan.x;
    float y = has_y ? (scan.absolute ? vy * k : scan.y + vy * k) : scan.y;

    if(scan.motion >= 2 && scan.plane_xy && (has_i || has_j))
        extend_arc(scan.x, scan.y, x, y, scan.x + vi * k, scan.y + vj * k, scan.motion == 2);
    else
        extend(x, y);   // also R arcs, end point only

    scan.x = x;
    scan.y = y;
}

static void scan_finish (void)
{
    if(scan.file) {
        vfs_close(scan.file);
        scan.file = NULL;
    }

    bounds.ok = scan.found;
    bounds.progress = 100;
    __sync_synchronize();
    bounds.seq++;
}

static void scan_step (void)
{
    char buf[SCAN_CHUNK];
    size_t n = vfs_read(buf, 1, sizeof(buf), scan.file);

    scan.read += n;
    bounds.progress = scan.size ? (uint8_t)((uint64_t)scan.read * 100 / scan.size) : 0;

    for(size_t idx = 0; idx < n; idx++) {
        char c = buf[idx];

        if(c == '\n' || c == '\r') {
            scan.line[scan.len] = '\0';
            if(scan.len)
                scan_line(scan.line);
            scan.len = 0;
            scan.in_comment = scan.skip_line = false;
        } else if(scan.skip_line)
            ;
        else if(c == ';')
            scan.skip_line = true;
        else if(c == '(')
            scan.in_comment = true;
        else if(c == ')')
            scan.in_comment = false;
        else if(!scan.in_comment && c != ' ' && c != '\t' && scan.len < SCAN_LINE_LEN - 1)
            scan.line[scan.len++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
    }

    if(n < sizeof(buf)) {
        scan.line[scan.len] = '\0';
        if(scan.len)
            scan_line(scan.line);
        scan_finish();
    }
}

static void scan_start (void)
{
    vfs_stat_t st;

    memset(&scan, 0, sizeof(scan));
    scan.absolute = scan.plane_xy = true;
    scan.motion = 0;
    scan.x = scan_start_x;
    scan.y = scan_start_y;

    if(sdcard_getfs() == NULL || (scan.file = vfs_open(scan_path, "r")) == NULL) {
        scan_finish();
        return;
    }

    scan.size = vfs_stat(scan_path, &st) == 0 ? (uint32_t)st.st_size : 0;
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

    if(scan_requested && !stream_is_file()) {
        __sync_synchronize();
        scan_requested = false;
        if(scan.file) {
            vfs_close(scan.file);
            scan.file = NULL;
        }
        scan_start();
    } else if(scan.file && !stream_is_file())
        scan_step();

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

    if(scan_requested) {
        scan_requested = false;
        bounds.ok = false;
        bounds.seq++;
    }
}

#endif

#endif // TFT_ENABLE
