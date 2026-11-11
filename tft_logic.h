/*
 * tft_logic.h - Pure helpers behind the screens and commands
 *
 * Part of grblHAL TFT Plugin
 *
 * No grblHAL, LVGL or FreeRTOS calls: callers pass in the machine state, so
 * this also builds on a PC for the tests in test/.
 */

#ifndef _TFT_LOGIC_H_
#define _TFT_LOGIC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "tft_sd.h"

#ifdef __cplusplus
extern "C" {
#endif

// Longest command line, including $F=<path>
#define TFT_CMD_MAX_LEN 128

/*
 * Command steps. A chain is a command followed by chained ones, e.g. the
 * steps of a probe cycle, see tft_interface.c.
 */
#define CMD_CHAINED     0x01    // wait for idle after the previous command
#define CMD_PROBE       0x02    // clear the probe result before sending
#define CMD_NEEDS_PROBE 0x04    // drop the chain unless the last probe succeeded
#define CMD_TOOLSETTER  0x08    // probe with the toolsetter input, not the touch plate

typedef struct {
    char line[TFT_CMD_MAX_LEN];
    uint8_t flags;
} tft_step_t;

// Steps of a tool measurement. Their lines need machine state (G59.3, the
// last probe position, the work offset), so they are built when sent.
typedef enum {
    Cmd_Line = 0,       // send line as is
    Cmd_TlsSafeZ,       // up to the Z home position
    Cmd_TlsXY,          // over the toolsetter (G59.3 X Y)
    Cmd_TlsZ,           // down to the G59.3 Z start height
    Cmd_TlsProbeFast,   // probe down the probe distance from there
    Cmd_TlsRetract,     // 2 mm above the fast probe hit
    Cmd_TlsProbeSlow,   // slow probe down to 3 mm below the fast hit
    Cmd_TlsOffset,      // first tool: $TLR, later tools: G43.1 Z<offset>
    Cmd_TlsSetHere      // save the current machine position as G59.3
} cmd_kind_t;

/*
 * SD card listing
 */

// The extensions grblHAL's $F lists
bool tft_logic_is_gcode(const char *name);

// Insert in order: folders first, then by name, ignoring case. False if the
// list is full.
bool tft_logic_listing_add(tft_sd_entry_t *entry, uint8_t *count, uint8_t max, const char *name, bool dir, uint32_t size);

/*
 * Macros: "Label|line|line..."
 */

// The label, false if the macro is empty
bool tft_logic_macro_label(const char *macro, char *buf, size_t size);

// The lines after the label, NULL if there are none
const char *tft_logic_macro_lines(const char *macro);

// Split "line|line|..." into steps, skipping empty ones and leading spaces.
// The first step starts a chain, the others wait for it. Returns the count,
// at most max.
size_t tft_logic_split_lines(const char *lines, tft_step_t *out, size_t max);

/*
 * Job progress
 */

// "m:ss", or "h:mm:ss" from one hour
void tft_logic_format_time(char *buf, size_t size, uint32_t ms);

#define TFT_ETA_MIN_MS          15000   // no estimate in the first 15 s
#define TFT_ETA_MIN_PERMILLE    10      // nor before 1% is done

// Time left from the average rate so far, false while too early to tell
bool tft_logic_eta(uint32_t elapsed_ms, uint32_t size, uint32_t pos_done, uint32_t *left_ms);

// The line in progress: the executing block's tagged line, or estimated
// from the lines read and the moves still buffered
uint32_t tft_logic_line_done(uint32_t lines_read, uint32_t queued, uint32_t exec_line);

// A line of the resume file as a line of its source file, 0 if before it
uint32_t tft_logic_offset_line(uint32_t line, int32_t offset);

// Bytes done, scaled by the line in progress
uint32_t tft_logic_pos_done(uint32_t pos, uint32_t line_done, uint32_t lines_read);

// A job ended after running to the end, not stopped
bool tft_logic_job_finished(uint32_t size, uint32_t pos, uint32_t line_done, uint32_t line);

// Line the Resume panel suggests for a file: where the last job of that
// file stopped, or 1
uint32_t tft_logic_resume_default(const tft_sd_job_t *job, const char *name);

/*
 * Text
 */

// Tool change banner. setter_missing: $341=3 but G59.3 is not set.
void tft_logic_format_tool_change(char *buf, size_t size, int32_t tool, bool setter_missing);

// Brightness percent within 10-100 ($456)
uint8_t tft_logic_brightness(int value);

/*
 * Command lines
 */

typedef struct {
    float plate;            // touch plate thickness
    float distance;         // probe at most this far down
    int feed, feed_slow;    // mm/min
    float retract;          // end this far above the plate
} tft_probe_cfg_t;

// Probe Z with a touch plate from work Z: fast probe, set Z0, back up 2 mm,
// slow probe, set Z0 again, retract. Returns the step count.
size_t tft_logic_probe_z(tft_step_t *out, size_t max, float work_z, const tft_probe_cfg_t *cfg);

// Frame the rectangle. With laser_s > 0: G0 to the first corner, around at
// that power with M4, then M5. Otherwise as jogs. Returns the step count.
size_t tft_logic_frame(tft_step_t *out, size_t max, float min_x, float min_y, float max_x, float max_y,
                       int feed, float laser_s);

// S word for a laser power in percent of $30
float tft_logic_laser_s(float spindle_max, float pct);

typedef struct {
    float z_home;           // machine Z of the home position
    float g59_3[3];         // toolsetter position, machine coordinates
    float probe_z;          // machine Z of the last probe hit
    float z_offset;         // work Z = machine Z - z_offset (TLO included)
    bool tlo_ref_set;       // $TLR done since homing
    float tlo_delta;        // last probe hit minus the reference, mm
    float mpos[3];          // current machine position
    float distance;         // probe distance
    int feed, feed_slow;
} tft_tls_t;

// The line of a tool measurement step (Cmd_Tls...), false if unknown
bool tft_logic_tls_line(cmd_kind_t kind, const tft_tls_t *t, char *buf, size_t size);

/*
 * Jogging
 */

// Step sizes the step button cycles through, the last one is continuous:
// jog while held
#define TFT_STEP_COUNT  5
#define TFT_STEP_CONT   (TFT_STEP_COUNT - 1)

// Step after idx, or before it with dir < 0, wrapping around
uint8_t tft_logic_step_next(uint8_t idx, int dir);

// Step size in mm, 0 for continuous
float tft_logic_step_mm(uint8_t idx);

// Step value for the button: "0.1", "1", "10", "100" or "Cont."
const char *tft_logic_step_text(uint8_t idx);

// Relative jog by dx/dy/dz mm, leaving out the axes that do not move:
// "$J=G91 G21 X1.000 Y-1.000 F1000". False if nothing moves.
bool tft_logic_jog_line(char *buf, size_t size, float dx, float dy, float dz, int feed);

// Set work zero for the axes in mask (bit 0 X, 1 Y, 2 Z): "G10 L20 P0 X0 Y0".
// False for an empty mask.
bool tft_logic_zero_line(char *buf, size_t size, uint8_t mask);

/*
 * Confirming a risky action (spindle on, Stop, Reset): either tap twice
 * within TFT_CONFIRM_MS, or hold for TFT_HOLD_MS ($458).
 */
#define TFT_CONFIRM_MS  3000
#define TFT_HOLD_MS     1000

typedef enum {
    Confirm_TapTwice = 0,
    Confirm_Hold
} tft_confirm_mode_t;

typedef enum {
    Confirm_Press,      // finger down
    Confirm_Release,    // finger up, or lost
    Confirm_Click       // a completed tap
} tft_confirm_event_t;

typedef struct {
    tft_confirm_mode_t mode;
    bool armed;         // tap twice: waiting for the second tap
    bool pressing;      // hold: finger down, not fired yet
    uint32_t since;     // ms when armed or pressed
} tft_confirm_t;

void tft_logic_confirm_init(tft_confirm_t *c, tft_confirm_mode_t mode);

// Feed a touch event, true when the action should run now
bool tft_logic_confirm_event(tft_confirm_t *c, tft_confirm_event_t event, uint32_t now);

// Call periodically: true when a hold completes. Also disarms an unanswered
// first tap after TFT_CONFIRM_MS.
bool tft_logic_confirm_tick(tft_confirm_t *c, uint32_t now);

// Hold progress 0-100 for the fill bar, 0 when not holding
uint8_t tft_logic_confirm_progress(const tft_confirm_t *c, uint32_t now);

// Cancel: not armed, not holding
void tft_logic_confirm_cancel(tft_confirm_t *c);

#ifdef __cplusplus
}
#endif

#endif // _TFT_LOGIC_H_
