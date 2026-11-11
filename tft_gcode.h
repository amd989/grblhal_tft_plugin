/*
 * tft_gcode.h - G-code reading for Frame and resume from line
 *
 * Part of grblHAL TFT Plugin
 *
 * Pure logic: no grblHAL, LVGL or file calls, so it also builds on a PC for
 * the tests in test/. tft_sd.c feeds it the file a chunk at a time.
 */

#ifndef _TFT_GCODE_H_
#define _TFT_GCODE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFT_GCODE_LINE_LEN 128

// Decimal number as G-code writes it, sign and digits only. Not strtof(),
// which reads "0X10" as hex. Advances *s past it, false if there is none.
bool tft_gcode_number(char **s, float *value);

/*
 * Line reader: collects one line at a time, upper case, without spaces,
 * tabs, (comments) and ; comments. Over long lines are cut.
 */
typedef struct {
    char text[TFT_GCODE_LINE_LEN];
    uint_fast8_t len;
    bool in_comment, skip_line;
} tft_gcode_reader_t;

void tft_gcode_reader_init(tft_gcode_reader_t *r);

// Feed one character. At a line end returns the line if it is not empty,
// otherwise NULL. The line stays valid until the next call.
char *tft_gcode_reader_char(tft_gcode_reader_t *r, char c);

// At the end of the file: the last line if it had no line end, or NULL
char *tft_gcode_reader_end(tft_gcode_reader_t *r);

/*
 * Frame: X/Y extents of the moves, in work coordinates (mm). Arcs (I/J in
 * the XY plane) include every 90 degree point they pass. G10/G28/G30/G38/
 * G53/G92/G4 lines are skipped, their coordinates are not moves in work
 * coordinates.
 */
typedef struct {
    bool absolute, inches, plane_xy;
    uint_fast8_t motion;
    float x, y;                     // current position
    bool found;                     // a move was seen
    float min_x, min_y, max_x, max_y;
} tft_gcode_bounds_t;

// Start from the current work position x, y (for relative moves)
void tft_gcode_bounds_init(tft_gcode_bounds_t *b, float x, float y);
void tft_gcode_bounds_line(tft_gcode_bounds_t *b, char *line);

/*
 * Resume from line: the modal state and position before a line.
 */
typedef struct {
    bool absolute, inches;
    uint8_t motion;             // 0-3, 255: other (probe, canned cycle, G80)
    uint8_t plane;              // 17, 18, 19
    uint16_t wcs;               // G-code x10: 540-590, 591-593
    bool has_x, has_y, has_z;
    float x, y, z;              // work coordinates, mm
    float feed;                 // file units per minute
    float plunge;               // feed of the last Z only G1, for lowering to the cut
    float speed;
    uint8_t spindle;            // 3, 4, 5
    bool mist, flood;
} tft_gcode_modal_t;

void tft_gcode_modal_init(tft_gcode_modal_t *m);
void tft_gcode_modal_line(tft_gcode_modal_t *m, char *line);

/*
 * Finds where a line starts, counting lines like grblHAL's file streamer:
 * a run of line ends is one. Lines before it update the modal state.
 */
typedef struct {
    tft_gcode_reader_t reader;
    tft_gcode_modal_t modal;
    uint32_t target;            // 1-based line to resume from
    uint32_t line;              // line ends counted so far
    uint_fast8_t eol;           // line end characters in a row, at most 2
    bool found;
} tft_gcode_resume_t;

void tft_gcode_resume_init(tft_gcode_resume_t *r, uint32_t target);

// Feed a chunk of the file. Returns the offset in buf where the target line
// starts, copy the file from there, or -1 if it is not in this chunk. Line 1
// (or 0) is the whole file and needs no feeding.
long tft_gcode_resume_feed(tft_gcode_resume_t *r, const char *buf, size_t n);

// After the whole file was fed without finding the line: its line count
uint32_t tft_gcode_resume_lines(const tft_gcode_resume_t *r);

typedef struct {
    uint32_t target;            // line resumed from, for the header comment
    bool laser;                 // laser mode: no spin-up dwell, Z homing optional
    bool z_homed;               // go up to z_home first (always on a CNC)
    float z_home;               // machine Z of the home position
    unsigned dwell_s;           // spin-up wait after the spindle starts
    float clearance;            // start this far above the cut, then plunge
    float default_plunge;       // mm/min when the file set no feed
} tft_gcode_restore_t;

// Write one line, without line end. False stops the restore.
typedef bool (*tft_gcode_write_ptr)(void *ctx, const char *line);

// The restore lines: mm and absolute while positioning, safe Z, over the
// start point, spindle and coolant, down to the cut, then the file's own
// modes. False if a write failed or a line did not fit.
bool tft_gcode_restore(const tft_gcode_modal_t *m, const tft_gcode_restore_t *o, tft_gcode_write_ptr write, void *ctx);

#ifdef __cplusplus
}
#endif

#endif // _TFT_GCODE_H_
