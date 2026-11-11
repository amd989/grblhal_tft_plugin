/*
 * tft_logic.c - Pure helpers behind the screens and commands
 *
 * Part of grblHAL TFT Plugin
 *
 * See tft_logic.h. Tested on a PC by test/test_logic.c.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "tft_logic.h"

/*
 * SD card listing
 */

bool tft_logic_is_gcode (const char *name)
{
    static const char *const types[] = { "nc", "ncc", "ngc", "cnc", "gcode", "txt", "text", "tap" };
    const char *ext = strrchr(name, '.');

    if(ext) for(size_t idx = 0; idx < sizeof(types) / sizeof(types[0]); idx++) {
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

bool tft_logic_listing_add (tft_sd_entry_t *entry, uint8_t *count, uint8_t max, const char *name, bool dir, uint32_t size)
{
    uint_fast8_t idx = *count;

    if(*count >= max)
        return false;

    // Insertion sort, the list is short
    while(idx > 0 && !before(&entry[idx - 1], dir, name)) {
        entry[idx] = entry[idx - 1];
        idx--;
    }

    strncpy(entry[idx].name, name, sizeof(entry[idx].name) - 1);
    entry[idx].name[sizeof(entry[idx].name) - 1] = '\0';
    entry[idx].dir = dir;
    entry[idx].size = size;
    (*count)++;

    return true;
}

/*
 * Macros
 */

bool tft_logic_macro_label (const char *macro, char *buf, size_t size)
{
    if(!macro || !*macro || !size)
        return false;

    const char *bar = strchr(macro, '|');
    size_t len = bar ? (size_t)(bar - macro) : strlen(macro);

    if(len >= size)
        len = size - 1;
    memcpy(buf, macro, len);
    buf[len] = '\0';

    return true;
}

const char *tft_logic_macro_lines (const char *macro)
{
    const char *bar = macro ? strchr(macro, '|') : NULL;

    return bar && bar[1] ? bar + 1 : NULL;
}

size_t tft_logic_split_lines (const char *lines, tft_step_t *out, size_t max)
{
    size_t count = 0;

    while(lines && *lines && count < max) {
        const char *end = strchr(lines, '|');
        size_t len = end ? (size_t)(end - lines) : strlen(lines);
        char line[TFT_CMD_MAX_LEN];

        if(len >= sizeof(line))
            len = sizeof(line) - 1;
        memcpy(line, lines, len);
        line[len] = '\0';

        char *p = line;
        while(*p == ' ')
            p++;
        if(*p) {
            strcpy(out[count].line, p);
            out[count].flags = count ? CMD_CHAINED : 0;
            count++;
        }

        lines = end ? end + 1 : NULL;
    }

    return count;
}

/*
 * Job progress
 */

void tft_logic_format_time (char *buf, size_t size, uint32_t ms)
{
    uint32_t s = ms / 1000;

    if(s >= 3600)
        snprintf(buf, size, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    else
        snprintf(buf, size, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

bool tft_logic_eta (uint32_t elapsed_ms, uint32_t size, uint32_t pos_done, uint32_t *left_ms)
{
    if(!size || pos_done >= size || elapsed_ms < TFT_ETA_MIN_MS ||
        (uint64_t)pos_done * 1000 < (uint64_t)size * TFT_ETA_MIN_PERMILLE)
        return false;

    *left_ms = (uint32_t)((uint64_t)elapsed_ms * (size - pos_done) / pos_done);

    return true;
}

uint32_t tft_logic_line_done (uint32_t lines_read, uint32_t queued, uint32_t exec_line)
{
    // Nothing buffered: all lines read so far have run
    if(queued == 0)
        return lines_read;

    return exec_line ? exec_line : (lines_read > queued ? lines_read - queued : 0);
}

uint32_t tft_logic_offset_line (uint32_t line, int32_t offset)
{
    return (int32_t)line + offset > 0 ? (uint32_t)((int32_t)line + offset) : 0;
}

uint32_t tft_logic_pos_done (uint32_t pos, uint32_t line_done, uint32_t lines_read)
{
    return lines_read ? (uint32_t)((uint64_t)pos * line_done / lines_read) : 0;
}

bool tft_logic_job_finished (uint32_t size, uint32_t pos, uint32_t line_done, uint32_t line)
{
    return size && pos >= size && line_done >= line;
}

uint32_t tft_logic_resume_default (const tft_sd_job_t *job, const char *name)
{
    return !job->active && !job->finished && job->line_done && !strcmp(job->source, name) ? job->line_done : 1;
}

/*
 * Text
 */

void tft_logic_format_tool_change (char *buf, size_t size, int32_t tool, bool setter_missing)
{
    if(setter_missing)
        snprintf(buf, size, "T%d: toolsetter position not set!", (int)tool);
    else
        snprintf(buf, size, "Change to T%d, then Resume", (int)tool);
}

uint8_t tft_logic_brightness (int value)
{
    return value < 10 ? 10 : value > 100 ? 100 : (uint8_t)value;
}

bool tft_logic_host_active (bool network_stream, bool report_seen, uint32_t last_report, uint32_t now)
{
    return network_stream || (report_seen && now - last_report < TFT_HOST_TIMEOUT_MS);
}

/*
 * Command lines
 */

static size_t add (tft_step_t *out, size_t count, size_t max, uint8_t flags, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

static size_t add (tft_step_t *out, size_t count, size_t max, uint8_t flags, const char *fmt, ...)
{
    va_list args;

    if(count >= max)
        return count;

    va_start(args, fmt);
    vsnprintf(out[count].line, sizeof(out[count].line), fmt, args);
    va_end(args);
    out[count].flags = flags;

    return count + 1;
}

size_t tft_logic_probe_z (tft_step_t *out, size_t max, float work_z, const tft_probe_cfg_t *cfg)
{
    size_t n = 0;

    n = add(out, n, max, CMD_PROBE, "G21 G90 G38.2 Z%.3f F%d", work_z - cfg->distance, cfg->feed);
    n = add(out, n, max, CMD_CHAINED|CMD_NEEDS_PROBE, "G10 L20 P0 Z%.3f", cfg->plate);
    n = add(out, n, max, CMD_CHAINED|CMD_NEEDS_PROBE, "G0 Z%.3f", cfg->plate + 2.0f);
    n = add(out, n, max, CMD_CHAINED|CMD_PROBE, "G38.2 Z%.3f F%d", cfg->plate - 3.0f, cfg->feed_slow);
    n = add(out, n, max, CMD_CHAINED|CMD_NEEDS_PROBE, "G10 L20 P0 Z%.3f", cfg->plate);
    n = add(out, n, max, CMD_CHAINED|CMD_NEEDS_PROBE, "G0 Z%.3f", cfg->plate + cfg->retract);

    return n;
}

size_t tft_logic_frame (tft_step_t *out, size_t max, float min_x, float min_y, float max_x, float max_y,
                        int feed, float laser_s)
{
    const float corner[5][2] = {
        { min_x, min_y }, { max_x, min_y }, { max_x, max_y }, { min_x, max_y }, { min_x, min_y }
    };
    size_t n = 0;

    if(laser_s > 0.0f) {
        // Laser off to the first corner, then around at low power. M4 scales
        // the power with speed, so the stops between the moves leave no dots.
        n = add(out, n, max, 0, "G21 G90 G0 X%.3f Y%.3f", corner[0][0], corner[0][1]);
        n = add(out, n, max, CMD_CHAINED, "G1 X%.3f Y%.3f F%d M4 S%.1f", corner[1][0], corner[1][1], feed, laser_s);
        for(size_t idx = 2; idx < 5; idx++)
            n = add(out, n, max, CMD_CHAINED, "G1 X%.3f Y%.3f", corner[idx][0], corner[idx][1]);
        n = add(out, n, max, CMD_CHAINED, "M5 G0");
        return n;
    }

    for(size_t idx = 0; idx < 5; idx++)
        n = add(out, n, max, idx ? CMD_CHAINED : 0, "$J=G90 G21 X%.3f Y%.3f F%d", corner[idx][0], corner[idx][1], feed);

    return n;
}

float tft_logic_laser_s (float spindle_max, float pct)
{
    return spindle_max * pct / 100.0f;
}

bool tft_logic_tls_line (cmd_kind_t kind, const tft_tls_t *t, char *buf, size_t size)
{
    switch(kind) {

        case Cmd_TlsSafeZ:
            snprintf(buf, size, "G21 G53 G0 Z%.3f", t->z_home);
            break;

        case Cmd_TlsXY:
            snprintf(buf, size, "G21 G53 G0 X%.3f Y%.3f", t->g59_3[0], t->g59_3[1]);
            break;

        case Cmd_TlsZ:
            snprintf(buf, size, "G21 G53 G0 Z%.3f", t->g59_3[2]);
            break;

        case Cmd_TlsProbeFast:
            snprintf(buf, size, "G21 G90 G38.2 Z%.3f F%d", t->g59_3[2] - t->distance - t->z_offset, t->feed);
            break;

        case Cmd_TlsRetract:
            snprintf(buf, size, "G21 G53 G0 Z%.3f", t->probe_z + 2.0f);
            break;

        case Cmd_TlsProbeSlow:
            snprintf(buf, size, "G21 G90 G38.2 Z%.3f F%d", t->probe_z - 3.0f - t->z_offset, t->feed_slow);
            break;

        case Cmd_TlsOffset:
            if(t->tlo_ref_set)
                snprintf(buf, size, "G21 G43.1 Z%.3f", t->tlo_delta);
            else
                snprintf(buf, size, "$TLR");
            break;

        case Cmd_TlsSetHere:
            snprintf(buf, size, "G21 G10 L2 P9 X%.3f Y%.3f Z%.3f", t->mpos[0], t->mpos[1], t->mpos[2]);
            break;

        default:
            return false;
    }

    return true;
}

/*
 * Jogging
 */

static const float step_mm[TFT_STEP_COUNT] = { 0.1f, 1.0f, 10.0f, 100.0f, 0.0f };
static const char *const step_text[TFT_STEP_COUNT] = { "0.1", "1", "10", "100", "Cont." };

uint8_t tft_logic_step_next (uint8_t idx, int dir)
{
    if(idx >= TFT_STEP_COUNT)
        return 1;

    return dir < 0 ? (idx ? idx - 1 : TFT_STEP_COUNT - 1) : (idx + 1) % TFT_STEP_COUNT;
}

float tft_logic_step_mm (uint8_t idx)
{
    return idx < TFT_STEP_COUNT ? step_mm[idx] : 0.0f;
}

const char *tft_logic_step_text (uint8_t idx)
{
    return idx < TFT_STEP_COUNT ? step_text[idx] : "";
}

bool tft_logic_jog_line (char *buf, size_t size, float dx, float dy, float dz, int feed)
{
    const float d[3] = { dx, dy, dz };
    int n = snprintf(buf, size, "$J=G91 G21");
    bool moves = false;

    for(uint_fast8_t idx = 0; idx < 3; idx++) {
        if(d[idx] != 0.0f && n > 0 && (size_t)n < size) {
            n += snprintf(buf + n, size - n, " %c%.3f", 'X' + idx, d[idx]);
            moves = true;
        }
    }
    if(n > 0 && (size_t)n < size)
        snprintf(buf + n, size - n, " F%d", feed);

    return moves;
}

bool tft_logic_zero_line (char *buf, size_t size, uint8_t mask)
{
    int n = snprintf(buf, size, "G10 L20 P0");

    for(uint_fast8_t idx = 0; idx < 3; idx++) {
        if((mask & (1 << idx)) && n > 0 && (size_t)n < size)
            n += snprintf(buf + n, size - n, " %c0", 'X' + idx);
    }

    return (mask & 0x07) != 0;
}

/*
 * Confirming
 */

void tft_logic_confirm_init (tft_confirm_t *c, tft_confirm_mode_t mode)
{
    memset(c, 0, sizeof(*c));
    c->mode = mode;
}

void tft_logic_confirm_cancel (tft_confirm_t *c)
{
    c->armed = c->pressing = false;
}

bool tft_logic_confirm_event (tft_confirm_t *c, tft_confirm_event_t event, uint32_t now)
{
    if(c->mode == Confirm_Hold) {
        if(event == Confirm_Press) {
            c->pressing = true;
            c->since = now;
        } else if(event == Confirm_Release)
            c->pressing = false;
        return false;   // fires from the tick, once held long enough
    }

    if(event != Confirm_Click)
        return false;

    if(c->armed && now - c->since <= TFT_CONFIRM_MS) {
        c->armed = false;
        return true;
    }

    c->armed = true;
    c->since = now;

    return false;
}

bool tft_logic_confirm_tick (tft_confirm_t *c, uint32_t now)
{
    if(c->mode == Confirm_Hold) {
        if(c->pressing && now - c->since >= TFT_HOLD_MS) {
            c->pressing = false;    // once per press
            return true;
        }
    } else if(c->armed && now - c->since > TFT_CONFIRM_MS)
        c->armed = false;

    return false;
}

uint8_t tft_logic_confirm_progress (const tft_confirm_t *c, uint32_t now)
{
    if(c->mode != Confirm_Hold || !c->pressing)
        return 0;

    uint32_t held = now - c->since;

    return held >= TFT_HOLD_MS ? 100 : (uint8_t)(held * 100 / TFT_HOLD_MS);
}
