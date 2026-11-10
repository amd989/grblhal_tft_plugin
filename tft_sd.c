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

/*
 * Resume from line: read the file up to the line, keeping track of the modal
 * state and position, then write the restore lines and copy the rest.
 */

#define RESUME_CHUNK    1024

static struct {
    vfs_file_t *src, *dst;
    uint32_t target, line, size, read;
    uint_fast8_t eol;
    bool copying;
    char text[SCAN_LINE_LEN];
    uint_fast8_t len;
    bool in_comment, skip_line;
    // modal state and position before the target line
    bool absolute, inches;
    uint8_t motion;             // 0-3, 255: other (probe, canned cycle off)
    uint8_t plane;              // 17, 18, 19
    uint16_t wcs;               // G-code x10: 540-590, 591-593
    bool has_x, has_y, has_z;
    float x, y, z;              // work coordinates, mm
    float feed;                 // file units per minute
    float plunge;               // feed of the last Z only G1, for lowering to the cut
    float speed;
    uint8_t spindle;            // 3, 4, 5
    bool mist, flood;
    bool laser;                 // laser mode: Z homing optional, no spin-up dwell
    uint32_t written_lines;     // lines of the restore part
} rs;

// Pending resumed job, for update_job to name it
static struct {
    bool pending;
    char name[50];
    char source[TFT_SD_NAME_LEN];
    int32_t line_offset;
} resumed;

static int32_t job_line_offset;

static void resume_line_parse (char *s)
{
    bool skip = false, has[3] = { false, false, false };
    float v[3] = { 0.0f, 0.0f, 0.0f };

    while(*s) {
        char letter = *s++;
        float value;

        if(!read_number(&s, &value))
            continue;

        switch(letter) {

            case 'G': {
                int g10 = (int)lroundf(value * 10.0f);
                if(g10 == 0 || g10 == 10 || g10 == 20 || g10 == 30)
                    rs.motion = g10 / 10;
                else if(g10 >= 382 && g10 <= 385) {
                    rs.motion = 255;    // probing, the end point is not known
                    skip = true;
                } else if(g10 >= 730 && g10 <= 890)
                    rs.motion = 255;    // canned cycles and G80
                else if(g10 == 170 || g10 == 180 || g10 == 190)
                    rs.plane = g10 / 10;
                else if(g10 == 200 || g10 == 210)
                    rs.inches = g10 == 200;
                else if(g10 == 900 || g10 == 910)
                    rs.absolute = g10 == 900;
                else if((g10 >= 540 && g10 <= 590 && g10 % 10 == 0) || g10 == 591 || g10 == 592 || g10 == 593)
                    rs.wcs = g10;
                else if(g10 == 40 || g10 == 100 || g10 == 280 || g10 == 281 || g10 == 300 || g10 == 301 || g10 == 530 || (g10 >= 920 && g10 <= 923))
                    skip = true;    // coordinates that are not a move in work coordinates
                } break;

            case 'M': {
                int m = (int)value;
                if(m == 3 || m == 4 || m == 5)
                    rs.spindle = m;
                else if(m == 7)
                    rs.mist = true;
                else if(m == 8)
                    rs.flood = true;
                else if(m == 9)
                    rs.mist = rs.flood = false;
                } break;

            case 'F': rs.feed = value; break;
            case 'S': rs.speed = value; break;
            case 'X': has[0] = true; v[0] = value; break;
            case 'Y': has[1] = true; v[1] = value; break;
            case 'Z': has[2] = true; v[2] = value; break;
            default: break;
        }
    }

    if(skip || rs.motion > 3)
        return;

    if(rs.motion == 1 && has[2] && !has[0] && !has[1])
        rs.plunge = rs.feed;

    float k = rs.inches ? 25.4f : 1.0f;
    float *pos[3] = { &rs.x, &rs.y, &rs.z };
    bool *known[3] = { &rs.has_x, &rs.has_y, &rs.has_z };

    for(uint_fast8_t idx = 0; idx < 3; idx++) {
        if(has[idx]) {
            *pos[idx] = rs.absolute ? v[idx] * k : *pos[idx] + v[idx] * k;
            *known[idx] = true;
        }
    }
}

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
        snprintf(resumed.name, sizeof(resumed.name), "%s from line %u", leaf, (unsigned)rs.target);
        strncpy(resumed.source, leaf, sizeof(resumed.source) - 1);
        resumed.source[sizeof(resumed.source) - 1] = '\0';
        resumed.line_offset = (int32_t)(rs.target - 1) - (int32_t)rs.written_lines;
        resumed.pending = true;
        tft_sd_run(TFT_RESUME_FILE);
        tft_log("resume from line %u", (unsigned)rs.target);
    }

    __sync_synchronize();
    resume.seq++;
}

static bool resume_write (const char *fmt, ...)
{
    char buf[96];
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    if(n < 0 || n >= (int)sizeof(buf) - 1)
        return false;

    buf[n++] = '\n';
    rs.written_lines++;

    return vfs_write(buf, 1, n, rs.dst) == (size_t)n;
}

// Restore lines: mm and absolute while positioning, then the file's own modes
static bool resume_write_restore (void)
{
    float k = rs.inches ? 25.4f : 1.0f;
    bool spindle_on = rs.spindle == 3 || rs.spindle == 4;
    bool ok = true;
    char wcs[8];

    if(rs.wcs % 10)
        snprintf(wcs, sizeof(wcs), "G%u.%u", (unsigned)rs.wcs / 10, (unsigned)rs.wcs % 10);
    else
        snprintf(wcs, sizeof(wcs), "G%u", (unsigned)rs.wcs / 10);

    ok = ok && resume_write("(TFT resume from line %u)", (unsigned)rs.target);
    ok = ok && resume_write("G21 G90 G%u %s", (unsigned)rs.plane, wcs);
    if(!rs.laser || (sys.homed.mask & bit(Z_AXIS)))    // a laser may have no Z homing
        ok = ok && resume_write("G53 G0 Z%.3f", sys.home_position[Z_AXIS]);
    if(rs.has_x || rs.has_y) {
        char xy[40] = "";
        if(rs.has_x)
            snprintf(xy, sizeof(xy), " X%.3f", rs.x);
        if(rs.has_y)
            snprintf(xy + strlen(xy), sizeof(xy) - strlen(xy), " Y%.3f", rs.y);
        ok = ok && resume_write("G0%s", xy);
    }
    if(spindle_on) {
        // A laser in G0 stays off until the next cut
        ok = ok && resume_write("M%u S%.0f", (unsigned)rs.spindle, rs.speed);
        if(!rs.laser)
            ok = ok && resume_write("G4 P%u", (unsigned)TFT_RESUME_DWELL);
    } else if(rs.speed > 0.0f)
        ok = ok && resume_write("S%.0f", rs.speed);
    if(rs.mist)
        ok = ok && resume_write("M7");
    if(rs.flood)
        ok = ok && resume_write("M8");
    if(rs.has_z && rs.laser)
        ok = ok && resume_write("G0 Z%.3f", rs.z);     // focus height, laser off in G0
    else if(rs.has_z) {
        float plunge = rs.plunge > 0.0f ? rs.plunge * k : rs.feed > 0.0f ? rs.feed * k : (float)TFT_PROBE_FEED;
        ok = ok && resume_write("G0 Z%.3f", rs.z + TFT_RESUME_CLEARANCE);
        ok = ok && resume_write("G1 Z%.3f F%.1f", rs.z, plunge);
    }
    if(rs.inches)
        ok = ok && resume_write("G20");
    if(!rs.absolute)
        ok = ok && resume_write("G91");
    if(rs.motion <= 1)
        ok = ok && resume_write("G%u", (unsigned)rs.motion);
    if(rs.feed > 0.0f)
        ok = ok && resume_write("F%.3f", rs.feed);
    ok = ok && resume_write("(TFT resume: file continues)");

    return ok;
}

static void resume_start (void)
{
    vfs_stat_t st;

    if(rs.src)
        vfs_close(rs.src);
    if(rs.dst)
        vfs_close(rs.dst);

    memset(&rs, 0, sizeof(rs));
    rs.target = resume_line;
    rs.absolute = true;
    rs.plane = 17;
    rs.wcs = 540;
    rs.spindle = 5;
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
    if(rs.target <= 1) {
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
        for(size_t idx = 0; idx < n && !rs.copying; idx++) {
            char c = buf[idx];

            // Count lines like grblHAL's file streamer: a run of line ends is one
            if(rs.eol == 1 && ++rs.line == rs.target - 1) {
                if(!resume_write_restore()) {
                    resume_finish("Cannot write " TFT_RESUME_FILE);
                    return;
                }
                rs.copying = true;
                from = idx;
                break;
            }
            rs.eol = (c == '\r' || c == '\n') ? (rs.eol < 2 ? rs.eol + 1 : 2) : 0;

            if(c == '\n' || c == '\r') {
                rs.text[rs.len] = '\0';
                if(rs.len)
                    resume_line_parse(rs.text);
                rs.len = 0;
                rs.in_comment = rs.skip_line = false;
            } else if(rs.skip_line)
                ;
            else if(c == ';')
                rs.skip_line = true;
            else if(c == '(')
                rs.in_comment = true;
            else if(c == ')')
                rs.in_comment = false;
            else if(!rs.in_comment && c != ' ' && c != '\t' && rs.len < SCAN_LINE_LEN - 1)
                rs.text[rs.len++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
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
            snprintf(msg, sizeof(msg), "The file has %u lines", (unsigned)rs.line + (rs.eol ? 0 : 1));
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

    // Nothing buffered: all lines read so far have run
    if(queued == 0)
        return lines_read;

    return exec_line ? exec_line : (lines_read > queued ? lines_read - queued : 0);
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
        uint32_t done = line_in_progress(info->line, queued);

        job.line = (int32_t)info->line + job_line_offset > 0 ? info->line + job_line_offset : 0;
        job.line_done = (int32_t)done + job_line_offset > 0 ? done + job_line_offset : 0;
        job.pos_done = info->line ? (uint32_t)((uint64_t)info->pos * done / info->line) : 0;
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
