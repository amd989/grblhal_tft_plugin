/*
 * tft_gcode.c - G-code reading for Frame and resume from line
 *
 * Part of grblHAL TFT Plugin
 *
 * Pure logic, see tft_gcode.h. Tested on a PC by test/test_gcode.c.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "tft_gcode.h"

bool tft_gcode_number (char **s, float *value)
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

/*
 * Line reader
 */

void tft_gcode_reader_init (tft_gcode_reader_t *r)
{
    memset(r, 0, sizeof(*r));
}

char *tft_gcode_reader_char (tft_gcode_reader_t *r, char c)
{
    if(c == '\n' || c == '\r') {
        bool has_text = r->len != 0;
        r->text[r->len] = '\0';
        r->len = 0;
        r->in_comment = r->skip_line = false;
        return has_text ? r->text : NULL;
    }

    if(r->skip_line)
        ;
    else if(c == ';')
        r->skip_line = true;
    else if(c == '(')
        r->in_comment = true;
    else if(c == ')')
        r->in_comment = false;
    else if(!r->in_comment && c != ' ' && c != '\t' && r->len < TFT_GCODE_LINE_LEN - 1)
        r->text[r->len++] = (c >= 'a' && c <= 'z') ? c - 32 : c;

    return NULL;
}

char *tft_gcode_reader_end (tft_gcode_reader_t *r)
{
    bool has_text = r->len != 0;

    r->text[r->len] = '\0';
    r->len = 0;

    return has_text ? r->text : NULL;
}

/*
 * Frame extents
 */

void tft_gcode_bounds_init (tft_gcode_bounds_t *b, float x, float y)
{
    memset(b, 0, sizeof(*b));
    b->absolute = b->plane_xy = true;
    b->x = x;
    b->y = y;
}

static void extend (tft_gcode_bounds_t *b, float x, float y)
{
    if(!b->found) {
        b->min_x = b->max_x = x;
        b->min_y = b->max_y = y;
        b->found = true;
    } else {
        if(x < b->min_x) b->min_x = x;
        if(x > b->max_x) b->max_x = x;
        if(y < b->min_y) b->min_y = y;
        if(y > b->max_y) b->max_y = y;
    }
}

// Arc extents: the end point, plus every 90 degree point the arc passes
static void extend_arc (tft_gcode_bounds_t *b, float x0, float y0, float x1, float y1, float cx, float cy, bool cw)
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
            extend(b, cx + r * cosf(a), cy + r * sinf(a));
    }

    extend(b, x1, y1);
}

void tft_gcode_bounds_line (tft_gcode_bounds_t *b, char *s)
{
    bool has_x = false, has_y = false, has_i = false, has_j = false, skip = false;
    float vx = 0.0f, vy = 0.0f, vi = 0.0f, vj = 0.0f;

    while(*s) {
        char letter = *s++;
        float value;

        if(!tft_gcode_number(&s, &value))
            continue;

        switch(letter) {

            case 'G': {
                int g = (int)value, g10 = (int)lroundf(value * 10.0f);
                if(g10 == 901 || g10 == 911)
                    ;   // G90.1 / G91.1 arc center modes, IJ incremental assumed
                else if(g <= 3)
                    b->motion = g;
                else if(g == 17 || g == 18 || g == 19)
                    b->plane_xy = g == 17;
                else if(g == 20 || g == 21)
                    b->inches = g == 20;
                else if(g == 90 || g == 91)
                    b->absolute = g == 90;
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

    if(skip || !(has_x || has_y) || b->motion > 3)
        return;

    float k = b->inches ? 25.4f : 1.0f;
    float x = has_x ? (b->absolute ? vx * k : b->x + vx * k) : b->x;
    float y = has_y ? (b->absolute ? vy * k : b->y + vy * k) : b->y;

    if(b->motion >= 2 && b->plane_xy && (has_i || has_j))
        extend_arc(b, b->x, b->y, x, y, b->x + vi * k, b->y + vj * k, b->motion == 2);
    else
        extend(b, x, y);   // also R arcs, end point only

    b->x = x;
    b->y = y;
}

/*
 * Resume from line
 */

void tft_gcode_modal_init (tft_gcode_modal_t *m)
{
    memset(m, 0, sizeof(*m));
    m->absolute = true;
    m->plane = 17;
    m->wcs = 540;
    m->spindle = 5;
}

void tft_gcode_modal_line (tft_gcode_modal_t *m, char *s)
{
    bool skip = false, has[3] = { false, false, false };
    float v[3] = { 0.0f, 0.0f, 0.0f };

    while(*s) {
        char letter = *s++;
        float value;

        if(!tft_gcode_number(&s, &value))
            continue;

        switch(letter) {

            case 'G': {
                int g10 = (int)lroundf(value * 10.0f);
                if(g10 == 0 || g10 == 10 || g10 == 20 || g10 == 30)
                    m->motion = g10 / 10;
                else if(g10 >= 382 && g10 <= 385) {
                    m->motion = 255;    // probing, the end point is not known
                    skip = true;
                } else if(g10 >= 730 && g10 <= 890)
                    m->motion = 255;    // canned cycles and G80
                else if(g10 == 170 || g10 == 180 || g10 == 190)
                    m->plane = g10 / 10;
                else if(g10 == 200 || g10 == 210)
                    m->inches = g10 == 200;
                else if(g10 == 900 || g10 == 910)
                    m->absolute = g10 == 900;
                else if((g10 >= 540 && g10 <= 590 && g10 % 10 == 0) || g10 == 591 || g10 == 592 || g10 == 593)
                    m->wcs = g10;
                else if(g10 == 40 || g10 == 100 || g10 == 280 || g10 == 281 || g10 == 300 || g10 == 301 || g10 == 530 || (g10 >= 920 && g10 <= 923))
                    skip = true;    // coordinates that are not a move in work coordinates
                } break;

            case 'M': {
                int mc = (int)value;
                if(mc == 3 || mc == 4 || mc == 5)
                    m->spindle = mc;
                else if(mc == 7)
                    m->mist = true;
                else if(mc == 8)
                    m->flood = true;
                else if(mc == 9)
                    m->mist = m->flood = false;
                } break;

            case 'F': m->feed = value; break;
            case 'S': m->speed = value; break;
            case 'X': has[0] = true; v[0] = value; break;
            case 'Y': has[1] = true; v[1] = value; break;
            case 'Z': has[2] = true; v[2] = value; break;
            default: break;
        }
    }

    if(skip || m->motion > 3)
        return;

    if(m->motion == 1 && has[2] && !has[0] && !has[1])
        m->plunge = m->feed;

    float k = m->inches ? 25.4f : 1.0f;
    float *pos[3] = { &m->x, &m->y, &m->z };
    bool *known[3] = { &m->has_x, &m->has_y, &m->has_z };

    for(uint_fast8_t idx = 0; idx < 3; idx++) {
        if(has[idx]) {
            *pos[idx] = m->absolute ? v[idx] * k : *pos[idx] + v[idx] * k;
            *known[idx] = true;
        }
    }
}

void tft_gcode_resume_init (tft_gcode_resume_t *r, uint32_t target)
{
    memset(r, 0, sizeof(*r));
    tft_gcode_reader_init(&r->reader);
    tft_gcode_modal_init(&r->modal);
    r->target = target;
}

long tft_gcode_resume_feed (tft_gcode_resume_t *r, const char *buf, size_t n)
{
    if(r->found)
        return 0;

    for(size_t idx = 0; idx < n; idx++) {
        char c = buf[idx];

        // Count lines like grblHAL's file streamer: a run of line ends is one.
        // The target line starts at the first character after a line end.
        if(r->eol == 1 && ++r->line == r->target - 1) {
            r->found = true;
            return (long)idx;
        }
        r->eol = (c == '\r' || c == '\n') ? (r->eol < 2 ? r->eol + 1 : 2) : 0;
        if(r->eol == 1)
            r->ends++;
        r->text = r->text || r->eol == 0;

        char *line = tft_gcode_reader_char(&r->reader, c);
        if(line)
            tft_gcode_modal_line(&r->modal, line);
    }

    return -1;
}

uint32_t tft_gcode_resume_lines (const tft_gcode_resume_t *r)
{
    // Each run of line ends closes a line, text after the last one is one more
    return r->ends + (r->text && r->eol == 0 ? 1 : 0);
}

static bool restore_write (tft_gcode_write_ptr write, void *ctx, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

static bool restore_write (tft_gcode_write_ptr write, void *ctx, const char *fmt, ...)
{
    char buf[96];
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    if(n < 0 || n >= (int)sizeof(buf) - 1)
        return false;

    return write(ctx, buf);
}

bool tft_gcode_restore (const tft_gcode_modal_t *m, const tft_gcode_restore_t *o, tft_gcode_write_ptr write, void *ctx)
{
    float k = m->inches ? 25.4f : 1.0f;
    bool spindle_on = m->spindle == 3 || m->spindle == 4;
    bool ok = true;
    char wcs[8];

    if(m->wcs % 10)
        snprintf(wcs, sizeof(wcs), "G%u.%u", (unsigned)m->wcs / 10, (unsigned)m->wcs % 10);
    else
        snprintf(wcs, sizeof(wcs), "G%u", (unsigned)m->wcs / 10);

    ok = ok && restore_write(write, ctx, "(TFT resume from line %u)", (unsigned)o->target);
    ok = ok && restore_write(write, ctx, "G21 G90 G%u %s", (unsigned)m->plane, wcs);
    if(!o->laser || o->z_homed)     // a laser may have no Z homing
        ok = ok && restore_write(write, ctx, "G53 G0 Z%.3f", o->z_home);
    if(m->has_x || m->has_y) {
        char xy[40] = "";
        if(m->has_x)
            snprintf(xy, sizeof(xy), " X%.3f", m->x);
        if(m->has_y)
            snprintf(xy + strlen(xy), sizeof(xy) - strlen(xy), " Y%.3f", m->y);
        ok = ok && restore_write(write, ctx, "G0%s", xy);
    }
    if(spindle_on) {
        // A laser in G0 stays off until the next cut
        ok = ok && restore_write(write, ctx, "M%u S%.0f", (unsigned)m->spindle, m->speed);
        if(!o->laser)
            ok = ok && restore_write(write, ctx, "G4 P%u", o->dwell_s);
    } else if(m->speed > 0.0f)
        ok = ok && restore_write(write, ctx, "S%.0f", m->speed);
    if(m->mist)
        ok = ok && restore_write(write, ctx, "M7");
    if(m->flood)
        ok = ok && restore_write(write, ctx, "M8");
    if(m->has_z && o->laser)
        ok = ok && restore_write(write, ctx, "G0 Z%.3f", m->z);     // focus height, laser off in G0
    else if(m->has_z) {
        float plunge = m->plunge > 0.0f ? m->plunge * k : m->feed > 0.0f ? m->feed * k : o->default_plunge;
        ok = ok && restore_write(write, ctx, "G0 Z%.3f", m->z + o->clearance);
        ok = ok && restore_write(write, ctx, "G1 Z%.3f F%.1f", m->z, plunge);
    }
    if(m->inches)
        ok = ok && restore_write(write, ctx, "G20");
    if(!m->absolute)
        ok = ok && restore_write(write, ctx, "G91");
    if(m->motion <= 1)
        ok = ok && restore_write(write, ctx, "G%u", (unsigned)m->motion);
    if(m->feed > 0.0f)
        ok = ok && restore_write(write, ctx, "F%.3f", m->feed);
    ok = ok && restore_write(write, ctx, "(TFT resume: file continues)");

    return ok;
}
