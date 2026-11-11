/*
 * test_logic.c - Tests for tft_logic.c: files, macros, job progress, commands
 *
 * Part of grblHAL TFT Plugin
 */

#include "harness.h"
#include "../tft_logic.h"

/*
 * SD card listing
 */

TEST(gcode_extensions)
{
    CHECK(tft_logic_is_gcode("part.nc"));
    CHECK(tft_logic_is_gcode("PART.NC"));
    CHECK(tft_logic_is_gcode("a.gcode"));
    CHECK(tft_logic_is_gcode("a.tap"));
    CHECK(tft_logic_is_gcode("a.ngc"));
    CHECK(tft_logic_is_gcode("notes.txt"));
    CHECK(!tft_logic_is_gcode("model.stl"));
    CHECK(!tft_logic_is_gcode("noext"));
    CHECK(!tft_logic_is_gcode("job.nc.bak"));
}

TEST(listing_folders_first_then_by_name)
{
    tft_sd_entry_t e[8];
    uint8_t n = 0;

    tft_logic_listing_add(e, &n, 8, "zeta.nc", false, 10);
    tft_logic_listing_add(e, &n, 8, "Beta", true, 0);
    tft_logic_listing_add(e, &n, 8, "alpha.nc", false, 20);
    tft_logic_listing_add(e, &n, 8, "alpha", true, 0);
    tft_logic_listing_add(e, &n, 8, "Mid.NC", false, 30);

    CHECK_INT(n, 5);
    CHECK_STR(e[0].name, "alpha"); CHECK(e[0].dir);
    CHECK_STR(e[1].name, "Beta");  CHECK(e[1].dir);
    CHECK_STR(e[2].name, "alpha.nc"); CHECK_INT(e[2].size, 20);
    CHECK_STR(e[3].name, "Mid.NC");
    CHECK_STR(e[4].name, "zeta.nc");
}

TEST(listing_full)
{
    tft_sd_entry_t e[2];
    uint8_t n = 0;

    CHECK(tft_logic_listing_add(e, &n, 2, "a.nc", false, 1));
    CHECK(tft_logic_listing_add(e, &n, 2, "b.nc", false, 1));
    CHECK(!tft_logic_listing_add(e, &n, 2, "c.nc", false, 1));
    CHECK_INT(n, 2);
}

/*
 * Macros
 */

TEST(macro_label)
{
    char buf[24];

    CHECK(tft_logic_macro_label("Park|G53 G0 Z-1|G28", buf, sizeof(buf)));
    CHECK_STR(buf, "Park");
    CHECK(tft_logic_macro_label("Just a label", buf, sizeof(buf)));
    CHECK_STR(buf, "Just a label");
    CHECK(!tft_logic_macro_label("", buf, sizeof(buf)));
    CHECK(tft_logic_macro_label("A very long label indeed here|G0", buf, 8));
    CHECK_STR(buf, "A very ");
}

TEST(macro_lines)
{
    CHECK_STR(tft_logic_macro_lines("Park|G53 G0 Z-1|G28"), "G53 G0 Z-1|G28");
    CHECK(tft_logic_macro_lines("Label|") == NULL);
    CHECK(tft_logic_macro_lines("Label") == NULL);
    CHECK(tft_logic_macro_lines("") == NULL);
}

TEST(split_lines_chains_after_first)
{
    tft_step_t s[8];
    size_t n = tft_logic_split_lines("G53 G0 Z-1| G28 ||  M5", s, 8);

    CHECK_INT(n, 3);
    CHECK_STR(s[0].line, "G53 G0 Z-1"); CHECK_INT(s[0].flags, 0);
    CHECK_STR(s[1].line, "G28 ");       CHECK_INT(s[1].flags, CMD_CHAINED);
    CHECK_STR(s[2].line, "M5");         CHECK_INT(s[2].flags, CMD_CHAINED);
}

TEST(split_lines_limits)
{
    tft_step_t s[8];
    char longline[300];

    CHECK_INT(tft_logic_split_lines("a|b|c|d", s, 2), 2);
    CHECK_INT(tft_logic_split_lines("", s, 8), 0);
    CHECK_INT(tft_logic_split_lines(NULL, s, 8), 0);

    memset(longline, 'G', sizeof(longline) - 1);
    longline[sizeof(longline) - 1] = '\0';
    CHECK_INT(tft_logic_split_lines(longline, s, 8), 1);
    CHECK_INT(strlen(s[0].line), TFT_CMD_MAX_LEN - 1);
}

/*
 * Job progress
 */

TEST(format_time)
{
    char buf[16];

    tft_logic_format_time(buf, sizeof(buf), 0);        CHECK_STR(buf, "0:00");
    tft_logic_format_time(buf, sizeof(buf), 61999);    CHECK_STR(buf, "1:01");
    tft_logic_format_time(buf, sizeof(buf), 3599000);  CHECK_STR(buf, "59:59");
    tft_logic_format_time(buf, sizeof(buf), 3600000);  CHECK_STR(buf, "1:00:00");
    tft_logic_format_time(buf, sizeof(buf), 3725000);  CHECK_STR(buf, "1:02:05");
}

TEST(eta)
{
    uint32_t left = 0;

    CHECK(tft_logic_eta(60000, 1000, 250, &left));
    CHECK_INT(left, 180000);
    CHECK(!tft_logic_eta(14999, 1000, 250, &left));     // too early
    CHECK(!tft_logic_eta(60000, 1000, 9, &left));       // under 1% done
    CHECK(tft_logic_eta(60000, 1000, 10, &left));       // 1% is enough
    CHECK(!tft_logic_eta(60000, 1000, 1000, &left));    // done
    CHECK(!tft_logic_eta(60000, 0, 0, &left));          // no size
}

TEST(eta_large_files)
{
    uint32_t left = 0;

    // 4 GB file and 10 hours in, no overflow
    CHECK(tft_logic_eta(36000000, 4000000000u, 1000000000u, &left));
    CHECK_INT(left, 108000000);
}

TEST(line_done)
{
    CHECK_INT(tft_logic_line_done(120, 0, 80), 120);    // nothing buffered
    CHECK_INT(tft_logic_line_done(120, 30, 95), 95);    // tagged block
    CHECK_INT(tft_logic_line_done(120, 30, 0), 90);     // estimate
    CHECK_INT(tft_logic_line_done(10, 30, 0), 0);
}

TEST(offset_line)
{
    CHECK_INT(tft_logic_offset_line(10, -3), 7);
    CHECK_INT(tft_logic_offset_line(2, -5), 0);
    CHECK_INT(tft_logic_offset_line(5, 9), 14);
    CHECK_INT(tft_logic_offset_line(0, 0), 0);
}

TEST(pos_done_and_finished)
{
    CHECK_INT(tft_logic_pos_done(1000, 5, 10), 500);
    CHECK_INT(tft_logic_pos_done(1000, 5, 0), 0);
    CHECK_INT(tft_logic_pos_done(4000000000u, 3, 4), 3000000000u);

    CHECK(tft_logic_job_finished(100, 100, 50, 50));
    CHECK(!tft_logic_job_finished(100, 99, 50, 50));    // not read to the end
    CHECK(!tft_logic_job_finished(100, 100, 49, 50));   // moves left: stopped
    CHECK(!tft_logic_job_finished(0, 0, 0, 0));
}

TEST(resume_default_line)
{
    tft_sd_job_t job = { .active = false, .finished = false, .line_done = 42 };

    strcpy(job.source, "part.nc");
    CHECK_INT(tft_logic_resume_default(&job, "part.nc"), 42);
    CHECK_INT(tft_logic_resume_default(&job, "other.nc"), 1);
    job.finished = true;
    CHECK_INT(tft_logic_resume_default(&job, "part.nc"), 1);
    job.finished = false;
    job.active = true;
    CHECK_INT(tft_logic_resume_default(&job, "part.nc"), 1);
    job.active = false;
    job.line_done = 0;
    CHECK_INT(tft_logic_resume_default(&job, "part.nc"), 1);
}

/*
 * Text and settings
 */

TEST(tool_change_text)
{
    char buf[48];

    tft_logic_format_tool_change(buf, sizeof(buf), 2, false);
    CHECK_STR(buf, "Change to T2, then Resume");
    tft_logic_format_tool_change(buf, sizeof(buf), 7, true);
    CHECK_STR(buf, "T7: toolsetter position not set!");
}

TEST(brightness_limits)
{
    CHECK_INT(tft_logic_brightness(-3), 10);
    CHECK_INT(tft_logic_brightness(0), 10);
    CHECK_INT(tft_logic_brightness(10), 10);
    CHECK_INT(tft_logic_brightness(55), 55);
    CHECK_INT(tft_logic_brightness(100), 100);
    CHECK_INT(tft_logic_brightness(255), 100);
}

/*
 * Command lines
 */

static const tft_probe_cfg_t probe_cfg = {
    .plate = 14.7f, .distance = 50.0f, .feed = 150, .feed_slow = 25, .retract = 5.0f
};

TEST(probe_z_steps)
{
    tft_step_t s[8];

    CHECK_INT(tft_logic_probe_z(s, 8, 20.0f, &probe_cfg), 6);
    CHECK_STR(s[0].line, "G21 G90 G38.2 Z-30.000 F150"); CHECK_INT(s[0].flags, CMD_PROBE);
    CHECK_STR(s[1].line, "G10 L20 P0 Z14.700");          CHECK_INT(s[1].flags, CMD_CHAINED|CMD_NEEDS_PROBE);
    CHECK_STR(s[2].line, "G0 Z16.700");                  CHECK_INT(s[2].flags, CMD_CHAINED|CMD_NEEDS_PROBE);
    CHECK_STR(s[3].line, "G38.2 Z11.700 F25");           CHECK_INT(s[3].flags, CMD_CHAINED|CMD_PROBE);
    CHECK_STR(s[4].line, "G10 L20 P0 Z14.700");          CHECK_INT(s[4].flags, CMD_CHAINED|CMD_NEEDS_PROBE);
    CHECK_STR(s[5].line, "G0 Z19.700");                  CHECK_INT(s[5].flags, CMD_CHAINED|CMD_NEEDS_PROBE);
}

TEST(probe_z_respects_max)
{
    tft_step_t s[3];

    CHECK_INT(tft_logic_probe_z(s, 3, 0.0f, &probe_cfg), 3);
}

TEST(frame_as_jogs)
{
    tft_step_t s[8];

    CHECK_INT(tft_logic_frame(s, 8, -1.5f, 2.0f, 30.0f, 40.25f, 1000, 0.0f), 5);
    CHECK_STR(s[0].line, "$J=G90 G21 X-1.500 Y2.000 F1000"); CHECK_INT(s[0].flags, 0);
    CHECK_STR(s[1].line, "$J=G90 G21 X30.000 Y2.000 F1000"); CHECK_INT(s[1].flags, CMD_CHAINED);
    CHECK_STR(s[2].line, "$J=G90 G21 X30.000 Y40.250 F1000");
    CHECK_STR(s[3].line, "$J=G90 G21 X-1.500 Y40.250 F1000");
    CHECK_STR(s[4].line, "$J=G90 G21 X-1.500 Y2.000 F1000"); CHECK_INT(s[4].flags, CMD_CHAINED);
}

TEST(frame_with_laser)
{
    tft_step_t s[8];

    CHECK_INT(tft_logic_frame(s, 8, 0.0f, 0.0f, 10.0f, 5.0f, 1000, 10.0f), 6);
    CHECK_STR(s[0].line, "G21 G90 G0 X0.000 Y0.000");         CHECK_INT(s[0].flags, 0);
    CHECK_STR(s[1].line, "G1 X10.000 Y0.000 F1000 M4 S10.0"); CHECK_INT(s[1].flags, CMD_CHAINED);
    CHECK_STR(s[2].line, "G1 X10.000 Y5.000");
    CHECK_STR(s[3].line, "G1 X0.000 Y5.000");
    CHECK_STR(s[4].line, "G1 X0.000 Y0.000");
    CHECK_STR(s[5].line, "M5 G0");                            CHECK_INT(s[5].flags, CMD_CHAINED);
}

TEST(laser_power)
{
    CHECK_FLOAT(tft_logic_laser_s(1000.0f, 1.0f), 10.0);
    CHECK_FLOAT(tft_logic_laser_s(255.0f, 100.0f), 255.0);
    CHECK_FLOAT(tft_logic_laser_s(1000.0f, 0.0f), 0.0);
}

static const tft_tls_t tls = {
    .z_home = -1.0f,
    .g59_3 = { 10.0f, 250.0f, -40.0f },
    .probe_z = -62.5f,
    .z_offset = -80.0f,
    .tlo_ref_set = true,
    .tlo_delta = 0.312f,
    .mpos = { 1.0f, 2.0f, 3.0f },
    .distance = 50.0f,
    .feed = 150,
    .feed_slow = 25
};

static const char *tls_line (cmd_kind_t kind, const tft_tls_t *t)
{
    static char buf[TFT_CMD_MAX_LEN];

    CHECK(tft_logic_tls_line(kind, t, buf, sizeof(buf)));

    return buf;
}

TEST(toolsetter_lines)
{
    CHECK_STR(tls_line(Cmd_TlsSafeZ, &tls),     "G21 G53 G0 Z-1.000");
    CHECK_STR(tls_line(Cmd_TlsXY, &tls),        "G21 G53 G0 X10.000 Y250.000");
    CHECK_STR(tls_line(Cmd_TlsZ, &tls),         "G21 G53 G0 Z-40.000");
    // Probe targets in work Z: machine Z minus the work offset
    CHECK_STR(tls_line(Cmd_TlsProbeFast, &tls), "G21 G90 G38.2 Z-10.000 F150");
    CHECK_STR(tls_line(Cmd_TlsRetract, &tls),   "G21 G53 G0 Z-60.500");
    CHECK_STR(tls_line(Cmd_TlsProbeSlow, &tls), "G21 G90 G38.2 Z14.500 F25");
    CHECK_STR(tls_line(Cmd_TlsOffset, &tls),    "G21 G43.1 Z0.312");
    CHECK_STR(tls_line(Cmd_TlsSetHere, &tls),   "G21 G10 L2 P9 X1.000 Y2.000 Z3.000");
}

TEST(toolsetter_first_tool_sets_reference)
{
    tft_tls_t t = tls;

    t.tlo_ref_set = false;
    CHECK_STR(tls_line(Cmd_TlsOffset, &t), "$TLR");
}

TEST(toolsetter_plain_line_is_not_built)
{
    char buf[TFT_CMD_MAX_LEN];

    CHECK(!tft_logic_tls_line(Cmd_Line, &tls, buf, sizeof(buf)));
}

void test_logic_suite (void)
{
    static const test_case_t tests[] = {
        TEST_CASE(gcode_extensions),
        TEST_CASE(listing_folders_first_then_by_name),
        TEST_CASE(listing_full),
        TEST_CASE(macro_label),
        TEST_CASE(macro_lines),
        TEST_CASE(split_lines_chains_after_first),
        TEST_CASE(split_lines_limits),
        TEST_CASE(format_time),
        TEST_CASE(eta),
        TEST_CASE(eta_large_files),
        TEST_CASE(line_done),
        TEST_CASE(offset_line),
        TEST_CASE(pos_done_and_finished),
        TEST_CASE(resume_default_line),
        TEST_CASE(tool_change_text),
        TEST_CASE(brightness_limits),
        TEST_CASE(probe_z_steps),
        TEST_CASE(probe_z_respects_max),
        TEST_CASE(frame_as_jogs),
        TEST_CASE(frame_with_laser),
        TEST_CASE(laser_power),
        TEST_CASE(toolsetter_lines),
        TEST_CASE(toolsetter_first_tool_sets_reference),
        TEST_CASE(toolsetter_plain_line_is_not_built),
    };

    RUN_SUITE("logic", tests);
}
