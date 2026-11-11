/*
 * tft_sd.c - SD card file listing and job status for the TFT UI
 *
 * Part of grblHAL TFT Plugin
 */

#include "driver.h"

#if TFT_ENABLE

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include "grbl/hal.h"
#include "grbl/vfs.h"
#include "grbl/stream_file.h"
#include "grbl/planner.h"
#include "grbl/system.h"
#include "grbl/state_machine.h"
#include "grbl/spindle_control.h"

#include "tft_config.h"

#if (FS_ENABLE & FS_SDCARD)
#include "sdcard/sdcard.h"
#include "sdcard/fs_stream.h"
#define TFT_SD_ENABLED 1
#else
#define TFT_SD_ENABLED 0
#endif

#include "tft_sd.h"
#include "tft_gcode.h"
#include "tft_logic.h"
#include "tft_interface.h"

static tft_sd_listing_t listing;
static tft_sd_job_t job;
static tft_sd_bounds_t bounds;
static tft_sd_resume_t resume;

static char request_path[TFT_SD_PATH_LEN];
static volatile bool list_requested = false;

static char scan_path[TFT_SD_PATH_LEN + TFT_SD_NAME_LEN + 1];
static float scan_start_x, scan_start_y;
static volatile bool scan_requested = false;

static char resume_path[TFT_SD_PATH_LEN + TFT_SD_NAME_LEN + 1];
static uint32_t resume_line;
static volatile bool resume_requested = false;

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

void tft_sd_resume (const char *path, uint32_t line)
{
    strncpy(resume_path, path, sizeof(resume_path) - 1);
    resume_path[sizeof(resume_path) - 1] = '\0';
    resume_line = line ? line : 1;
    resume.progress = 0;
    __sync_synchronize();
    resume_requested = true;
}

const tft_sd_resume_t *tft_sd_resume_status (void)
{
    return &resume;
}

/*
 * grblHAL loop
 */

#if TFT_SD_ENABLED

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

            if((dirent->st_mode.directory || tft_logic_is_gcode(dirent->name)) &&
                !tft_logic_listing_add(listing.entry, &listing.count, TFT_SD_MAX_ENTRIES, dirent->name, dirent->st_mode.directory, (uint32_t)dirent->size))
                listing.truncated = true;
        }

        vfs_closedir(dir);
    }

    __sync_synchronize();
    listing.seq++;
}

/*
 * Frame scan: the X/Y extents of the moves, read a chunk at a time
 */

#define SCAN_CHUNK      512

static struct {
    vfs_file_t *file;
    uint32_t size, read;
    tft_gcode_reader_t reader;
    tft_gcode_bounds_t extents;
} scan;

static void scan_finish (void)
{
    if(scan.file) {
        vfs_close(scan.file);
        scan.file = NULL;
    }

    bounds.min_x = scan.extents.min_x;
    bounds.min_y = scan.extents.min_y;
    bounds.max_x = scan.extents.max_x;
    bounds.max_y = scan.extents.max_y;
    bounds.ok = scan.extents.found;
    bounds.progress = 100;
    __sync_synchronize();
    bounds.seq++;
}

static void scan_step (void)
{
    char buf[SCAN_CHUNK], *line;
    size_t n = vfs_read(buf, 1, sizeof(buf), scan.file);

    scan.read += n;
    bounds.progress = scan.size ? (uint8_t)((uint64_t)scan.read * 100 / scan.size) : 0;

    for(size_t idx = 0; idx < n; idx++) {
        if((line = tft_gcode_reader_char(&scan.reader, buf[idx])))
            tft_gcode_bounds_line(&scan.extents, line);
    }

    if(n < sizeof(buf)) {
        if((line = tft_gcode_reader_end(&scan.reader)))
            tft_gcode_bounds_line(&scan.extents, line);
        scan_finish();
    }
}

static void scan_start (void)
{
    vfs_stat_t st;

    memset(&scan, 0, sizeof(scan));
    tft_gcode_reader_init(&scan.reader);
    tft_gcode_bounds_init(&scan.extents, scan_start_x, scan_start_y);

    if(sdcard_getfs() == NULL || (scan.file = vfs_open(scan_path, "r")) == NULL) {
        scan_finish();
        return;
    }

    scan.size = vfs_stat(scan_path, &st) == 0 ? (uint32_t)st.st_size : 0;
}

/*
 * Resume from line: read the file up to the line, keeping track of the modal
 * state and position (tft_gcode.c), then write the restore lines and copy
 * the rest.
 */

#define RESUME_CHUNK    1024

static struct {
    vfs_file_t *src, *dst;
    uint32_t size, read;
    bool copying;
    bool laser;                 // laser mode: Z homing optional, no spin-up dwell
    uint32_t written_lines;     // lines of the restore part
    tft_gcode_resume_t find;
} rs;

// Pending resumed job, for update_job to name it
static struct {
    bool pending;
    char name[50];
    char source[TFT_SD_NAME_LEN];
    int32_t line_offset;
} resumed;

static int32_t job_line_offset;

static void resume_finish (const char *error)
{
    if(rs.src) {
        vfs_close(rs.src);
        rs.src = NULL;
    }
    if(rs.dst) {
        vfs_close(rs.dst);
        rs.dst = NULL;
    }

    resume.ok = error == NULL;
    strncpy(resume.error, error ? error : "", sizeof(resume.error) - 1);
    resume.error[sizeof(resume.error) - 1] = '\0';
    resume.progress = 100;

    if(resume.ok) {
        const char *leaf = strrchr(resume_path, '/');
        leaf = leaf ? leaf + 1 : resume_path;
        snprintf(resumed.name, sizeof(resumed.name), "%s from line %u", leaf, (unsigned)rs.find.target);
        strncpy(resumed.source, leaf, sizeof(resumed.source) - 1);
        resumed.source[sizeof(resumed.source) - 1] = '\0';
        resumed.line_offset = (int32_t)(rs.find.target - 1) - (int32_t)rs.written_lines;
        resumed.pending = true;
        tft_sd_run(TFT_RESUME_FILE);
        tft_log("resume from line %u", (unsigned)rs.find.target);
    }

    __sync_synchronize();
    resume.seq++;
}

// One restore line to the resume file
static bool resume_write (void *ctx, const char *line)
{
    char buf[100];
    size_t n = strlen(line);

    (void)ctx;
    if(n >= sizeof(buf))
        return false;

    memcpy(buf, line, n);
    buf[n++] = '\n';
    rs.written_lines++;

    return vfs_write(buf, 1, n, rs.dst) == n;
}

static bool resume_write_restore (void)
{
    tft_gcode_restore_t opts = {
        .target = rs.find.target,
        .laser = rs.laser,
        .z_homed = !!(sys.homed.mask & bit(Z_AXIS)),
        .z_home = sys.home_position[Z_AXIS],
        .dwell_s = TFT_RESUME_DWELL,
        .clearance = TFT_RESUME_CLEARANCE,
        .default_plunge = (float)TFT_PROBE_FEED
    };

    return tft_gcode_restore(&rs.find.modal, &opts, resume_write, NULL);
}

static void resume_start (void)
{
    vfs_stat_t st;

    if(rs.src)
        vfs_close(rs.src);
    if(rs.dst)
        vfs_close(rs.dst);

    memset(&rs, 0, sizeof(rs));
    tft_gcode_resume_init(&rs.find, resume_line);
    resumed.pending = false;

    spindle_ptrs_t *spindle = spindle_get(0);
    rs.laser = spindle && spindle->cap.laser;

    // CNC: the restore goes up to the Z home position first
    if(!rs.laser && !(sys.homed.mask & bit(Z_AXIS))) {
        resume_finish("Home the machine first");
        return;
    }

    if(sdcard_getfs() == NULL || (rs.src = vfs_open(resume_path, "r")) == NULL) {
        resume_finish("Cannot open the file");
        return;
    }

    rs.size = vfs_stat(resume_path, &st) == 0 ? (uint32_t)st.st_size : 0;

    if((rs.dst = vfs_open(TFT_RESUME_FILE, "w")) == NULL) {
        resume_finish("Cannot write " TFT_RESUME_FILE);
        return;
    }

    // Line 1 is the whole file
    if(rs.find.target <= 1) {
        rs.copying = true;
        rs.written_lines = 0;
    }
}

static void resume_step (void)
{
    char buf[RESUME_CHUNK];
    size_t n = vfs_read(buf, 1, sizeof(buf), rs.src), from = 0;

    rs.read += n;
    resume.progress = rs.size ? (uint8_t)((uint64_t)rs.read * 99 / rs.size) : 0;

    if(!rs.copying) {
        long at = tft_gcode_resume_feed(&rs.find, buf, n);
        if(at >= 0) {
            if(!resume_write_restore()) {
                resume_finish("Cannot write " TFT_RESUME_FILE);
                return;
            }
            rs.copying = true;
            from = (size_t)at;
        }
    }

    if(rs.copying && n > from) {
        if(vfs_write(buf + from, 1, n - from, rs.dst) != n - from) {
            resume_finish("Cannot write " TFT_RESUME_FILE);
            return;
        }
    }

    if(n < sizeof(buf)) {
        if(!rs.copying) {
            char msg[48];
            snprintf(msg, sizeof(msg), "The file has %u lines", (unsigned)tft_gcode_resume_lines(&rs.find));
            resume_finish(msg);
        } else
            resume_finish(NULL);
    }
}

/*
 * The line in progress: grblHAL calls the plugin poll at the end of every
 * line it reads, before running it, so the newest planner block then comes
 * from the line before, which is the count of lines read. Each new block is
 * tagged with that line (also shown as Ln: in status reports), and the block
 * being executed tells the line in progress. Arcs make many blocks per line,
 * only the last one may get tagged, the untagged ones keep the line before.
 * Counting buffered blocks back from the lines read overshoots, as comments
 * and M codes take no block.
 */
static plan_block_t *tagged_block;
static uint32_t exec_line;

static uint32_t line_in_progress (uint32_t lines_read, uint32_t queued)
{
    plan_block_t *block = plan_get_recent_block();

    if(block && block != tagged_block) {
        block->line_number = (int32_t)lines_read;
        tagged_block = block;
    }

    if((block = plan_get_current_block()) && block->line_number > 0)
        exec_line = (uint32_t)block->line_number;

    return tft_logic_line_done(lines_read, queued, exec_line);
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
            tagged_block = NULL;
            exec_line = 0;
            job_line_offset = 0;
            if(resumed.pending && !strcmp(info->name, TFT_RESUME_FILE + 1)) {
                strcpy(job.name, resumed.name);
                strcpy(job.source, resumed.source);
                job_line_offset = resumed.line_offset;
            } else {
                strncpy(job.name, info->name, sizeof(job.name) - 1);
                job.name[sizeof(job.name) - 1] = '\0';
                strncpy(job.source, info->name, sizeof(job.source) - 1);
                job.source[sizeof(job.source) - 1] = '\0';
            }
            resumed.pending = false;
            tft_log("job started: %s", job.name);
        }
        uint32_t queued = plan_get_buffer_size() - plan_get_block_buffer_available();

        job.size = info->size;
        job.pos = info->pos;
        // line_number starts at 1 and counts up as lines are read, so the lines
        // read so far is one less. N words in the file overwrite it.
        uint32_t lines_read = info->line_number > 1 ? (uint32_t)info->line_number - 1 : 0;
        uint32_t done = line_in_progress(lines_read, queued);

        job.line = tft_logic_offset_line(lines_read, job_line_offset);
        job.line_done = tft_logic_offset_line(done, job_line_offset);
        job.pos_done = tft_logic_pos_done(info->pos, done, lines_read);
        job.elapsed_ms = now - started_at;
    } else if(job.active) {
        job.elapsed_ms = now - started_at;
        // A finished job waits for the motion buffer to empty before it ends,
        // a stopped one ends with moves still buffered
        job.finished = tft_logic_job_finished(job.size, job.pos, job.line_done, job.line);

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

    // Only while idle: the copy must not change under a running job
    if(resume_requested && !stream_is_file() && state_get() == STATE_IDLE) {
        __sync_synchronize();
        resume_requested = false;
        resume_start();
    } else if(rs.src && !stream_is_file())
        resume_step();

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

    if(resume_requested) {
        resume_requested = false;
        resume.ok = false;
        strcpy(resume.error, "No SD card support");
        resume.seq++;
    }
}

#endif

#endif // TFT_ENABLE
