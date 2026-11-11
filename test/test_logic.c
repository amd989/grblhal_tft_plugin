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

/*
 * Jogging
 */

TEST(step_cycle)
{
    CHECK_INT(tft_logic_step_next(0, 1), 1);
    CHECK_INT(tft_logic_step_next(3, 1), TFT_STEP_CONT);
    CHECK_INT(tft_logic_step_next(TFT_STEP_CONT, 1), 0);    // wraps to 0.1
    CHECK_INT(tft_logic_step_next(0, -1), TFT_STEP_CONT);   // long press goes back
    CHECK_INT(tft_logic_step_next(2, -1), 1);
    CHECK_INT(tft_logic_step_next(99, 1), 1);               // bad index: 1 mm
}

TEST(step_values)
{
    CHECK_FLOAT(tft_logic_step_mm(0), 0.1);
    CHECK_FLOAT(tft_logic_step_mm(3), 100);
    CHECK_FLOAT(tft_logic_step_mm(TFT_STEP_CONT), 0);
    CHECK_STR(tft_logic_step_text(1), "1");
    CHECK_STR(tft_logic_step_text(TFT_STEP_CONT), "Cont.");
    CHECK_STR(tft_logic_step_text(99), "");
}

TEST(jog_lines)
{
    char buf[TFT_CMD_MAX_LEN];

    CHECK(tft_logic_jog_line(buf, sizeof(buf), 1.0f, 0.0f, 0.0f, 1000));
    CHECK_STR(buf, "$J=G91 G21 X1.000 F1000");
    CHECK(tft_logic_jog_line(buf, sizeof(buf), -10.0f, 10.0f, 0.0f, 1000));
    CHECK_STR(buf, "$J=G91 G21 X-10.000 Y10.000 F1000");
    CHECK(tft_logic_jog_line(buf, sizeof(buf), 0.0f, 0.0f, -0.1f, 300));
    CHECK_STR(buf, "$J=G91 G21 Z-0.100 F300");
    CHECK(!tft_logic_jog_line(buf, sizeof(buf), 0.0f, 0.0f, 0.0f, 300));
}

TEST(zero_lines)
{
    char buf[32];

    CHECK(tft_logic_zero_line(buf, sizeof(buf), 0x01)); CHECK_STR(buf, "G10 L20 P0 X0");
    CHECK(tft_logic_zero_line(buf, sizeof(buf), 0x03)); CHECK_STR(buf, "G10 L20 P0 X0 Y0");
    CHECK(tft_logic_zero_line(buf, sizeof(buf), 0x04)); CHECK_STR(buf, "G10 L20 P0 Z0");
    CHECK(tft_logic_zero_line(buf, sizeof(buf), 0x07)); CHECK_STR(buf, "G10 L20 P0 X0 Y0 Z0");
    CHECK(!tft_logic_zero_line(buf, sizeof(buf), 0));
}

/*
 * Confirming
 */

TEST(confirm_tap_twice)
{
    tft_confirm_t c;

    tft_logic_confirm_init(&c, Confirm_TapTwice);
    CHECK(!tft_logic_confirm_event(&c, Confirm_Press, 1000));
    CHECK(!tft_logic_confirm_event(&c, Confirm_Click, 1000));   // arms
    CHECK(c.armed);
    CHECK(!tft_logic_confirm_tick(&c, 2000));
    CHECK(tft_logic_confirm_event(&c, Confirm_Click, 3500));    // second tap fires
    CHECK(!c.armed);
    CHECK(!tft_logic_confirm_event(&c, Confirm_Click, 3600));   // and starts over
}

TEST(confirm_tap_twice_times_out)
{
    tft_confirm_t c;

    tft_logic_confirm_init(&c, Confirm_TapTwice);
    tft_logic_confirm_event(&c, Confirm_Click, 0);
    tft_logic_confirm_tick(&c, TFT_CONFIRM_MS + 1);
    CHECK(!c.armed);
    CHECK(!tft_logic_confirm_event(&c, Confirm_Click, TFT_CONFIRM_MS + 2));  // arms again

    // Without a tick in between, a late second tap still does not fire
    tft_logic_confirm_init(&c, Confirm_TapTwice);
    tft_logic_confirm_event(&c, Confirm_Click, 0);
    CHECK(!tft_logic_confirm_event(&c, Confirm_Click, TFT_CONFIRM_MS + 500));
}

TEST(confirm_hold)
{
    tft_confirm_t c;

    tft_logic_confirm_init(&c, Confirm_Hold);
    CHECK(!tft_logic_confirm_event(&c, Confirm_Click, 0));      // taps do nothing
    CHECK(!tft_logic_confirm_event(&c, Confirm_Press, 100));
    CHECK_INT(tft_logic_confirm_progress(&c, 600), 50);
    CHECK(!tft_logic_confirm_tick(&c, 1099));
    CHECK(tft_logic_confirm_tick(&c, 1100));                    // held 1 s
    CHECK(!tft_logic_confirm_tick(&c, 1500));                   // only once
    CHECK_INT(tft_logic_confirm_progress(&c, 1500), 0);
    CHECK(!tft_logic_confirm_event(&c, Confirm_Release, 1600));
}

TEST(confirm_hold_released_early)
{
    tft_confirm_t c;

    tft_logic_confirm_init(&c, Confirm_Hold);
    tft_logic_confirm_event(&c, Confirm_Press, 0);
    tft_logic_confirm_event(&c, Confirm_Release, 900);
    CHECK(!tft_logic_confirm_tick(&c, 2000));
    CHECK_INT(tft_logic_confirm_progress(&c, 2000), 0);
}

TEST(confirm_cancel_and_tick_wrap)
{
    tft_confirm_t c;

    tft_logic_confirm_init(&c, Confirm_TapTwice);
    tft_logic_confirm_event(&c, Confirm_Click, 0xFFFFFF00u);
    CHECK(tft_logic_confirm_event(&c, Confirm_Click, 0x00000100u));    // across the tick wrap
    tft_logic_confirm_event(&c, Confirm_Click, 10);
    tft_logic_confirm_cancel(&c);
    CHECK(!tft_logic_confirm_event(&c, Confirm_Click, 20));
}

TEST(host_active)
{
    CHECK(!tft_logic_host_active(false, false, 0, 100000));     // never seen
    CHECK(tft_logic_host_active(true, false, 0, 100000));       // WebUI or telnet
    CHECK(tft_logic_host_active(false, true, 10000, 10250));    // polling
    CHECK(tft_logic_host_active(false, true, 10000, 10000 + TFT_HOST_TIMEOUT_MS - 1));
    CHECK(!tft_logic_host_active(false, true, 10000, 10000 + TFT_HOST_TIMEOUT_MS));  // gone quiet
    CHECK(tft_logic_host_active(false, true, 0xFFFFFF00u, 0x00000100u));            // tick wrap
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
        TEST_CASE(host_active),
        TEST_CASE(probe_z_steps),
        TEST_CASE(probe_z_respects_max),
        TEST_CASE(frame_as_jogs),
        TEST_CASE(frame_with_laser),
        TEST_CASE(laser_power),
        TEST_CASE(toolsetter_lines),
        TEST_CASE(toolsetter_first_tool_sets_reference),
        TEST_CASE(toolsetter_plain_line_is_not_built),
        TEST_CASE(step_cycle),
        TEST_CASE(step_values),
        TEST_CASE(jog_lines),
        TEST_CASE(zero_lines),
        TEST_CASE(confirm_tap_twice),
        TEST_CASE(confirm_tap_twice_times_out),
        TEST_CASE(confirm_hold),
        TEST_CASE(confirm_hold_released_early),
        TEST_CASE(confirm_cancel_and_tick_wrap),
    };

    RUN_SUITE("logic", tests);
}
