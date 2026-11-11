/*
 * test_gcode.c - Tests for tft_gcode.c: numbers, lines, Frame, resume
 *
 * Part of grblHAL TFT Plugin
 */

#include <stdlib.h>

#include "harness.h"
#include "../tft_gcode.h"

/*
 * Helpers
 */

// Run the reader over text and the bounds over its lines
static tft_gcode_bounds_t bounds_of (const char *text, float x, float y)
{
    tft_gcode_reader_t r;
    tft_gcode_bounds_t b;
    char *line;

    tft_gcode_reader_init(&r);
    tft_gcode_bounds_init(&b, x, y);
    for(const char *p = text; *p; p++) {
        if((line = tft_gcode_reader_char(&r, *p)))
            tft_gcode_bounds_line(&b, line);
    }
    if((line = tft_gcode_reader_end(&r)))
        tft_gcode_bounds_line(&b, line);

    return b;
}

static tft_gcode_modal_t modal_of (const char *text)
{
    tft_gcode_reader_t r;
    tft_gcode_modal_t m;
    char *line;

    tft_gcode_reader_init(&r);
    tft_gcode_modal_init(&m);
    for(const char *p = text; *p; p++) {
        if((line = tft_gcode_reader_char(&r, *p)))
            tft_gcode_modal_line(&m, line);
    }

    return m;
}

typedef struct {
    char text[4096];
    size_t len;
    int lines;
    int fail_at;    // fail this write (1-based), 0: never
} out_t;

static bool write_line (void *ctx, const char *line)
{
    out_t *o = ctx;
    size_t n = strlen(line);

    if(++o->lines == o->fail_at)
        return false;
    if(o->len + n + 1 >= sizeof(o->text))
        return false;
    memcpy(o->text + o->len, line, n);
    o->len += n;
    o->text[o->len++] = '\n';
    o->text[o->len] = '\0';

    return true;
}

static char *read_file (const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    char *buf;

    if(!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(*size + 1);
    if(buf && fread(buf, 1, *size, f) == *size)
        buf[*size] = '\0';
    fclose(f);

    return buf;
}

static const tft_gcode_restore_t cnc_opts = {
    .target = 14, .laser = false, .z_homed = true, .z_home = -1.0f,
    .dwell_s = 3, .clearance = 2.0f, .default_plunge = 150.0f
};

/*
 * Numbers
 */

TEST(number_reads_signs_and_decimals)
{
    char text[] = "12.5-3+.5", *s = text;
    float v;

    CHECK(tft_gcode_number(&s, &v)); CHECK_FLOAT(v, 12.5);
    CHECK(tft_gcode_number(&s, &v)); CHECK_FLOAT(v, -3);
    CHECK(tft_gcode_number(&s, &v)); CHECK_FLOAT(v, 0.5);
    CHECK(*s == '\0');
}

TEST(number_without_digits_is_not_read)
{
    char text[] = "-.X", *s = text;
    float v = 7.0f;

    CHECK(!tft_gcode_number(&s, &v));
    CHECK(s == text);       // not advanced
    CHECK_FLOAT(v, 7.0);    // not changed
}

TEST(number_is_not_hex)
{
    char text[] = "0X10", *s = text;
    float v;

    CHECK(tft_gcode_number(&s, &v));
    CHECK_FLOAT(v, 0.0);
    CHECK_STR(s, "X10");
}

TEST(number_stops_at_second_point)
{
    char text[] = "1.2.3", *s = text;
    float v;

    CHECK(tft_gcode_number(&s, &v));
    CHECK_FLOAT(v, 1.2);
    CHECK_STR(s, ".3");
}

/*
 * Line reader
 */

TEST(reader_cleans_lines)
{
    tft_gcode_reader_t r;
    const char *text = "g1 x10\t(move) y2 ; done\n";
    char *line = NULL;

    tft_gcode_reader_init(&r);
    for(const char *p = text; *p; p++) {
        char *l = tft_gcode_reader_char(&r, *p);
        if(*p != '\n')
            CHECK(l == NULL);
        else
            line = l;
    }
    CHECK_STR(line, "G1X10Y2");
}

TEST(reader_skips_empty_lines_and_crlf)
{
    tft_gcode_reader_t r;
    const char *text = "G0\r\n\n(only a comment)\n; and this\nM5";
    int lines = 0;

    tft_gcode_reader_init(&r);
    for(const char *p = text; *p; p++) {
        if(tft_gcode_reader_char(&r, *p))
            lines++;
    }
    CHECK_INT(lines, 1);
    CHECK_STR(tft_gcode_reader_end(&r), "M5");
    CHECK(tft_gcode_reader_end(&r) == NULL);
}

TEST(reader_comment_ends_at_line_end)
{
    tft_gcode_reader_t r;
    const char *text = "(unclosed\nG1 X1\n";
    char *line = NULL;

    tft_gcode_reader_init(&r);
    for(const char *p = text; *p; p++) {
        char *l = tft_gcode_reader_char(&r, *p);
        if(l)
            line = l;
    }
    CHECK_STR(line, "G1X1");
}

TEST(reader_cuts_long_lines)
{
    tft_gcode_reader_t r;
    char *line = NULL;

    tft_gcode_reader_init(&r);
    for(int idx = 0; idx < 300; idx++)
        tft_gcode_reader_char(&r, 'A');
    line = tft_gcode_reader_char(&r, '\n');
    CHECK(line != NULL);
    CHECK_INT(strlen(line), TFT_GCODE_LINE_LEN - 1);
}

/*
 * Frame extents
 */

TEST(bounds_absolute_moves)
{
    tft_gcode_bounds_t b = bounds_of("G21 G90\nG0 X10 Y5\nG1 X-20\nG1 Y40\n", 0, 0);

    CHECK(b.found);
    CHECK_FLOAT(b.min_x, -20); CHECK_FLOAT(b.max_x, 10);
    CHECK_FLOAT(b.min_y, 5);   CHECK_FLOAT(b.max_y, 40);
}

TEST(bounds_relative_from_start)
{
    tft_gcode_bounds_t b = bounds_of("G91\nG0 X10\nG1 Y-5\nG1 X-30\n", 100, 50);

    CHECK_FLOAT(b.min_x, 80);  CHECK_FLOAT(b.max_x, 110);
    CHECK_FLOAT(b.min_y, 45);  CHECK_FLOAT(b.max_y, 50);
}

TEST(bounds_back_to_absolute)
{
    tft_gcode_bounds_t b = bounds_of("G91 G0 X5 Y5\nG90\nG1 X-2 Y1\n", 0, 0);

    CHECK_FLOAT(b.min_x, -2); CHECK_FLOAT(b.max_x, 5);
    CHECK_FLOAT(b.min_y, 1);  CHECK_FLOAT(b.max_y, 5);
}

TEST(bounds_inches)
{
    tft_gcode_bounds_t b = bounds_of("G20 G90\nG0 X1 Y2\n", 0, 0);

    CHECK_FLOAT(b.max_x, 25.4);
    CHECK_FLOAT(b.max_y, 50.8);
}

TEST(bounds_skip_non_work_moves)
{
    tft_gcode_bounds_t b = bounds_of("G53 G0 X-500 Y-500\nG28 X0\nG30\nG92 X900\nG10 L20 P1 X300\n"
                                     "G4 P5\nG38.2 Z-10 X99\nG0 X1 Y1\n", 0, 0);

    CHECK(b.found);
    CHECK_FLOAT(b.min_x, 1); CHECK_FLOAT(b.max_x, 1);
    CHECK_FLOAT(b.min_y, 1); CHECK_FLOAT(b.max_y, 1);
}

TEST(bounds_none_without_xy_moves)
{
    tft_gcode_bounds_t b = bounds_of("(nothing)\nG21\nM3 S1000\nG1 Z-1 F100\n", 0, 0);

    CHECK(!b.found);
}

TEST(bounds_arc_passes_quadrant)
{
    // Clockwise half circle over the top: from (-10,0) to (10,0), center (0,0)
    tft_gcode_bounds_t b = bounds_of("G0 X-10 Y0\nG2 X10 Y0 I10 J0\n", 0, 0);

    CHECK_FLOAT(b.min_x, -10); CHECK_FLOAT(b.max_x, 10);
    CHECK_FLOAT(b.min_y, 0);   CHECK_FLOAT(b.max_y, 10);
}

TEST(bounds_arc_counterclockwise_goes_under)
{
    // Same points counterclockwise: under the center
    tft_gcode_bounds_t b = bounds_of("G0 X-10 Y0\nG3 X10 Y0 I10 J0\n", 0, 0);

    CHECK_FLOAT(b.min_y, -10); CHECK_FLOAT(b.max_y, 0);
}

TEST(bounds_full_circle)
{
    tft_gcode_bounds_t b = bounds_of("G0 X5 Y0\nG2 X5 Y0 I-5 J0\n", 0, 0);

    CHECK_FLOAT(b.min_x, -5); CHECK_FLOAT(b.max_x, 5);
    CHECK_FLOAT(b.min_y, -5); CHECK_FLOAT(b.max_y, 5);
}

TEST(bounds_arc_outside_xy_plane_uses_end_point)
{
    tft_gcode_bounds_t b = bounds_of("G18\nG0 X-10 Y0\nG2 X10 Y0 I10 J0\n", 0, 0);

    CHECK_FLOAT(b.max_y, 0);
}

TEST(bounds_g90_1_keeps_distance_mode)
{
    tft_gcode_bounds_t b = bounds_of("G91.1\nG0 X10 Y10\nG0 X20 Y20\n", 0, 0);

    CHECK_FLOAT(b.max_x, 20);   // still absolute
}

/*
 * Modal state
 */

TEST(modal_defaults)
{
    tft_gcode_modal_t m;

    tft_gcode_modal_init(&m);
    CHECK(m.absolute); CHECK(!m.inches);
    CHECK_INT(m.plane, 17); CHECK_INT(m.wcs, 540); CHECK_INT(m.spindle, 5);
    CHECK(!m.has_x && !m.has_y && !m.has_z);
}

TEST(modal_tracks_modes_and_position)
{
    tft_gcode_modal_t m = modal_of("G20 G18 G55\nM3 S12000\nM8\nG1 X1 Y2 Z-0.5 F20\n");

    CHECK(m.inches); CHECK_INT(m.plane, 18); CHECK_INT(m.wcs, 550);
    CHECK_INT(m.spindle, 3); CHECK_FLOAT(m.speed, 12000); CHECK(m.flood); CHECK(!m.mist);
    CHECK_INT(m.motion, 1); CHECK_FLOAT(m.feed, 20);
    CHECK_FLOAT(m.x, 25.4); CHECK_FLOAT(m.y, 50.8); CHECK_FLOAT(m.z, -12.7);
}

TEST(modal_relative_moves_add_up)
{
    tft_gcode_modal_t m = modal_of("G0 X10 Y10\nG91\nG1 X5\nG1 X5 Y-2\n");

    CHECK(!m.absolute);
    CHECK_FLOAT(m.x, 20); CHECK_FLOAT(m.y, 8);
    CHECK(!m.has_z);
}

TEST(modal_extra_work_offsets)
{
    CHECK_INT(modal_of("G59.1\n").wcs, 591);
    CHECK_INT(modal_of("G59.3\n").wcs, 593);
    CHECK_INT(modal_of("G59\n").wcs, 590);
}

TEST(modal_coolant_off)
{
    tft_gcode_modal_t m = modal_of("M7\nM8\nM9\n");

    CHECK(!m.mist); CHECK(!m.flood);
}

TEST(modal_plunge_from_z_only_feed)
{
    tft_gcode_modal_t m = modal_of("G1 Z-2 F300\nG1 X30 F600\n");

    CHECK_FLOAT(m.plunge, 300);
    CHECK_FLOAT(m.feed, 600);
}

TEST(modal_probe_leaves_position_unknown)
{
    tft_gcode_modal_t m = modal_of("G0 X1 Y1 Z1\nG38.2 Z-50 F100\n");

    CHECK_INT(m.motion, 255);   // probing leaves the motion mode unknown
    CHECK_FLOAT(m.z, 1);        // and is not taken as the position
}

TEST(modal_skips_machine_moves)
{
    tft_gcode_modal_t m = modal_of("G0 X1\nG53 G0 Z-5\nG92 X0\nG28\nG30 Z3\nG10 L20 P0 X7\n");

    CHECK_FLOAT(m.x, 1);
    CHECK(!m.has_z);
    CHECK_INT(m.motion, 0);     // G0 on a G53 line still sets the motion mode
}

TEST(modal_canned_cycle_stops_tracking)
{
    tft_gcode_modal_t m = modal_of("G0 X1\nG81 X5 Z-1 R1\nG80\nX9\n");

    CHECK_INT(m.motion, 255);
    CHECK_FLOAT(m.x, 1);    // moves in a canned cycle are not followed
}

/*
 * Finding the resume line
 */

TEST(resume_finds_line_after_lf)
{
    tft_gcode_resume_t r;
    const char *text = "A\nB\nC\n";

    tft_gcode_resume_init(&r, 3);
    CHECK_INT(tft_gcode_resume_feed(&r, text, strlen(text)), 4);
    CHECK(r.found);
}

TEST(resume_counts_crlf_as_one_line_end)
{
    tft_gcode_resume_t r;
    const char *text = "A\r\nB\r\nC\r\n";

    // Found on the \n of the CR LF pair: the copy starts with a line end,
    // which grblHAL counts with the one before, so C is still line 3.
    tft_gcode_resume_init(&r, 3);
    CHECK_INT(tft_gcode_resume_feed(&r, text, strlen(text)), 5);
}

TEST(resume_counts_blank_lines_as_one)
{
    tft_gcode_resume_t r;
    const char *text = "A\n\n\nB\nC\n";

    tft_gcode_resume_init(&r, 3);
    CHECK_INT(tft_gcode_resume_feed(&r, text, strlen(text)), 6);
}

TEST(resume_line_count_when_past_end)
{
    tft_gcode_resume_t r;

    tft_gcode_resume_init(&r, 9);
    CHECK_INT(tft_gcode_resume_feed(&r, "A\nB", 3), -1);
    CHECK_INT(tft_gcode_resume_lines(&r), 2);

    tft_gcode_resume_init(&r, 9);
    CHECK_INT(tft_gcode_resume_feed(&r, "A\nB\n", 4), -1);
    CHECK_INT(tft_gcode_resume_lines(&r), 2);

    tft_gcode_resume_init(&r, 9);
    tft_gcode_resume_feed(&r, "A\r\nB\r\n\r\n", 8);
    CHECK_INT(tft_gcode_resume_lines(&r), 2);

    tft_gcode_resume_init(&r, 9);
    tft_gcode_resume_feed(&r, "A", 1);
    CHECK_INT(tft_gcode_resume_lines(&r), 1);

    tft_gcode_resume_init(&r, 9);
    tft_gcode_resume_feed(&r, "", 0);
    CHECK_INT(tft_gcode_resume_lines(&r), 0);
}

TEST(resume_last_line_is_found)
{
    tft_gcode_resume_t r;

    tft_gcode_resume_init(&r, 2);
    CHECK_INT(tft_gcode_resume_feed(&r, "A\nB\n", 4), 2);
}

TEST(resume_modal_stops_before_target)
{
    tft_gcode_resume_t r;
    const char *text = "G0 X1\nG0 X2\nG0 X3\n";

    tft_gcode_resume_init(&r, 3);
    tft_gcode_resume_feed(&r, text, strlen(text));
    CHECK_FLOAT(r.modal.x, 2);  // line 3 is not read
}

// The whole resume file the CNC wrote on 2026-11-10, from the example file
static void check_golden (size_t chunk)
{
    size_t src_size, gold_size;
    char *src = read_file("test/fixtures/resume-source.nc", &src_size);
    char *gold = read_file("test/fixtures/resume-line14-z-1.nc", &gold_size);
    tft_gcode_resume_t r;
    out_t out = { .len = 0 };
    long at = -1;
    size_t pos = 0;

    CHECK(src != NULL); CHECK(gold != NULL);
    if(!src || !gold)
        return;

    tft_gcode_resume_init(&r, 14);
    while(pos < src_size && at < 0) {
        size_t n = src_size - pos < chunk ? src_size - pos : chunk;
        at = tft_gcode_resume_feed(&r, src + pos, n);
        if(at >= 0)
            at += (long)pos;
        else
            pos += n;
    }
    CHECK(at >= 0);
    CHECK(tft_gcode_restore(&r.modal, &cnc_opts, write_line, &out));
    if(at >= 0 && out.len + (src_size - (size_t)at) < sizeof(out.text)) {
        memcpy(out.text + out.len, src + at, src_size - (size_t)at);
        out.len += src_size - (size_t)at;
        out.text[out.len] = '\0';
    }
    CHECK_STR(out.text, gold);

    free(src);
    free(gold);
}

TEST(resume_matches_cnc_output) { check_golden(1024); }
TEST(resume_matches_cnc_output_in_small_chunks) { check_golden(7); }
TEST(resume_matches_cnc_output_one_byte_at_a_time) { check_golden(1); }

/*
 * Restore lines
 */

static const char *restore_of (const char *text, const tft_gcode_restore_t *o)
{
    static out_t out;
    tft_gcode_modal_t m = modal_of(text);

    memset(&out, 0, sizeof(out));
    CHECK(tft_gcode_restore(&m, o, write_line, &out));

    return out.text;
}

TEST(restore_cnc_spindle_and_coolant)
{
    CHECK_STR(restore_of("G55\nM3 S18000\nM8\nG0 X10 Y20\nG1 Z-1 F100\nG1 X30 F800\n", &cnc_opts),
        "(TFT resume from line 14)\n"
        "G21 G90 G17 G55\n"
        "G53 G0 Z-1.000\n"
        "G0 X30.000 Y20.000\n"
        "M3 S18000\n"
        "G4 P3\n"
        "M8\n"
        "G0 Z1.000\n"
        "G1 Z-1.000 F100.0\n"
        "G1\n"
        "F800.000\n"
        "(TFT resume: file continues)\n");
}

TEST(restore_laser_skips_dwell_and_plunge)
{
    tft_gcode_restore_t o = cnc_opts;

    o.laser = true;
    o.z_homed = false;
    CHECK_STR(restore_of("G0 X5 Y5 Z10\nM4 S500\nG1 X6 F3000\n", &o),
        "(TFT resume from line 14)\n"
        "G21 G90 G17 G54\n"
        "G0 X6.000 Y5.000\n"
        "M4 S500\n"
        "G0 Z10.000\n"
        "G1\n"
        "F3000.000\n"
        "(TFT resume: file continues)\n");
}

TEST(restore_laser_with_homed_z_goes_up_first)
{
    tft_gcode_restore_t o = cnc_opts;

    o.laser = true;
    o.z_homed = true;
    CHECK(strstr(restore_of("G0 X1\n", &o), "G53 G0 Z-1.000\n") != NULL);
}

TEST(restore_inches_and_relative)
{
    // Positions in mm, the plunge feed scaled too, then back to the file's modes
    CHECK_STR(restore_of("G20\nG0 X1 Y1\nG1 Z-0.1 F10\nG91\nG1 X0.5 F20\n", &cnc_opts),
        "(TFT resume from line 14)\n"
        "G21 G90 G17 G54\n"
        "G53 G0 Z-1.000\n"
        "G0 X38.100 Y25.400\n"
        "G0 Z-0.540\n"
        "G1 Z-2.540 F254.0\n"
        "G20\n"
        "G91\n"
        "G1\n"
        "F20.000\n"
        "(TFT resume: file continues)\n");
}

TEST(restore_plunge_falls_back_to_feed_then_default)
{
    CHECK(strstr(restore_of("G1 X1 Z-1 F500\n", &cnc_opts), "G1 Z-1.000 F500.0\n") != NULL);
    CHECK(strstr(restore_of("G0 X1 Z-1\n", &cnc_opts), "G1 Z-1.000 F150.0\n") != NULL);
}

TEST(restore_speed_without_spindle_on)
{
    const char *text = restore_of("S9000\nM5\nG0 X1\n", &cnc_opts);

    CHECK(strstr(text, "S9000\n") != NULL);
    CHECK(strstr(text, "M5") == NULL);
    CHECK(strstr(text, "G4") == NULL);
}

TEST(restore_work_offset_with_decimal)
{
    CHECK(strstr(restore_of("G59.2\nG0 X1\n", &cnc_opts), "G21 G90 G17 G59.2\n") != NULL);
}

TEST(restore_unknown_motion_keeps_mode_out)
{
    const char *text = restore_of("G0 X1\nG2 X2 Y0 I1 J0\n", &cnc_opts);

    CHECK(strstr(text, "\nG2\n") == NULL);  // only G0/G1 are restored
}

TEST(restore_stops_on_write_error)
{
    tft_gcode_modal_t m = modal_of("G0 X1\n");
    out_t out = { .fail_at = 2 };

    CHECK(!tft_gcode_restore(&m, &cnc_opts, write_line, &out));
    CHECK_INT(out.lines, 2);    // nothing written after the failed line
}

void test_gcode_suite (void)
{
    static const test_case_t tests[] = {
        TEST_CASE(number_reads_signs_and_decimals),
        TEST_CASE(number_without_digits_is_not_read),
        TEST_CASE(number_is_not_hex),
        TEST_CASE(number_stops_at_second_point),
        TEST_CASE(reader_cleans_lines),
        TEST_CASE(reader_skips_empty_lines_and_crlf),
        TEST_CASE(reader_comment_ends_at_line_end),
        TEST_CASE(reader_cuts_long_lines),
        TEST_CASE(bounds_absolute_moves),
        TEST_CASE(bounds_relative_from_start),
        TEST_CASE(bounds_back_to_absolute),
        TEST_CASE(bounds_inches),
        TEST_CASE(bounds_skip_non_work_moves),
        TEST_CASE(bounds_none_without_xy_moves),
        TEST_CASE(bounds_arc_passes_quadrant),
        TEST_CASE(bounds_arc_counterclockwise_goes_under),
        TEST_CASE(bounds_full_circle),
        TEST_CASE(bounds_arc_outside_xy_plane_uses_end_point),
        TEST_CASE(bounds_g90_1_keeps_distance_mode),
        TEST_CASE(modal_defaults),
        TEST_CASE(modal_tracks_modes_and_position),
        TEST_CASE(modal_relative_moves_add_up),
        TEST_CASE(modal_extra_work_offsets),
        TEST_CASE(modal_coolant_off),
        TEST_CASE(modal_plunge_from_z_only_feed),
        TEST_CASE(modal_probe_leaves_position_unknown),
        TEST_CASE(modal_skips_machine_moves),
        TEST_CASE(modal_canned_cycle_stops_tracking),
        TEST_CASE(resume_finds_line_after_lf),
        TEST_CASE(resume_counts_crlf_as_one_line_end),
        TEST_CASE(resume_counts_blank_lines_as_one),
        TEST_CASE(resume_line_count_when_past_end),
        TEST_CASE(resume_last_line_is_found),
        TEST_CASE(resume_modal_stops_before_target),
        TEST_CASE(resume_matches_cnc_output),
        TEST_CASE(resume_matches_cnc_output_in_small_chunks),
        TEST_CASE(resume_matches_cnc_output_one_byte_at_a_time),
        TEST_CASE(restore_cnc_spindle_and_coolant),
        TEST_CASE(restore_laser_skips_dwell_and_plunge),
        TEST_CASE(restore_laser_with_homed_z_goes_up_first),
        TEST_CASE(restore_inches_and_relative),
        TEST_CASE(restore_plunge_falls_back_to_feed_then_default),
        TEST_CASE(restore_speed_without_spindle_on),
        TEST_CASE(restore_work_offset_with_decimal),
        TEST_CASE(restore_unknown_motion_keeps_mode_out),
        TEST_CASE(restore_stops_on_write_error),
    };

    RUN_SUITE("gcode", tests);
}
