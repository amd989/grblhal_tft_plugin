/*
 * screen_probe.c - Probe tab: touch plate and tool setter
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (content area):
 *   top     touch plate: what to do, the live probe input, Probe
 *   middle  status line
 *   bottom  tool setter (TOOLSETTER_ENABLE, not in laser mode): its
 *           position and the tool offset, the live input, Set here
 *           (confirmed, it overwrites G59.3) and Measure
 *
 * Probe Z: a fast probe down, then a slow one, sets Z0 on the work surface
 * with the plate thickness taken off, see tft_probe_z().
 * Runs in the UI task only.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>

#include "screen_probe.h"
#include "ui_screen_manager.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_interface.h"

#define UPDATE_MS       100
#define PLATE_Y         UI_CONTENT_Y
#define STATUS_Y        172
#define SETTER_Y        196

static lv_obj_t *lbl_input, *lbl_status;
static char txt_input[32];
static int8_t input_shown = -1;

#if TOOLSETTER_ENABLE
static lv_obj_t *setter_panel, *lbl_setter_input, *lbl_setter_pos, *lbl_setter_ofs, *btn_measure;
static ui_confirm_btn_t set_here;
static char txt_setter_input[24], txt_setter_pos[64], txt_setter_ofs[64];
static int8_t setter_shown = -1, laser_shown = -1;
#endif

static uint32_t last_update;

static void status (const char *text)
{
    lv_label_set_text(lbl_status, text);
}

static void on_probe (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(ui_state.system_state != STATE_IDLE)
        status("Machine must be idle");
    else if(ui_state.probe_triggered)
        status("Probe already touching, check the clip");
    else {
        tft_log("btn probe start");
        tft_probe_z(ui_state.work_position[Z_AXIS]);
        status("Probing...");
    }
}

#if TOOLSETTER_ENABLE

static void do_set_here (void)
{
    if(ui_state.system_state != STATE_IDLE)
        status("Machine must be idle");
    else {
        tft_log("btn toolsetter set");
        tft_toolsetter_set_here();
        status("Toolsetter position saved");
    }
}

static void on_measure (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(ui_state.system_state != STATE_IDLE)
        status("Machine must be idle");
    else if(!ui_state.z_homed)
        status("Home the machine first");
    else if(!ui_state.tls_pos_set)
        status("Set the toolsetter position first");
    else if(ui_state.toolsetter_triggered)
        status("Toolsetter already triggered");
    else {
        tft_log("btn tool measure");
        tft_tool_measure();
        status(ui_state.tlo_ref_set ? "Measuring, sets the tool offset" : "Measuring, sets the reference");
    }
}

#endif

static void screen_create (lv_obj_t *parent)
{
    lv_style_t *title = ui_widget_text_style(UI_FONT_MEDIUM, UI_COLOR_TEXT);
    lv_style_t *small = ui_widget_text_style(UI_FONT_SMALL, UI_COLOR_TEXT_DIM);
    char buf[160];

    lv_obj_set_style(parent, ui_widget_screen_style());

    // Touch plate
    lv_obj_t *panel = ui_widget_create_panel(parent, UI_CONTENT_X, PLATE_Y, UI_CONTENT_W, 128);
    ui_widget_create_styled_label(panel, 10, 8, title, "Touch plate");
    snprintf(buf, sizeof(buf), "Plate under the bit, clip on. Bit less\nthan %.0f mm above. Plate %.2f mm,\nsets Z0 on the work surface.",
             (double)TFT_PROBE_DISTANCE, (double)TFT_PROBE_THICKNESS);
    ui_widget_create_styled_label(panel, 10, 32, small, buf);
    lbl_input = ui_widget_create_styled_label(panel, 10, 96, title, "");
    txt_input[0] = '\0';
    ui_widget_create_button(panel, 232, 66, 104, 52, LV_SYMBOL_DOWNLOAD " Probe", BUTTON_STYLE_SUCCESS, on_probe);

    lbl_status = ui_widget_create_styled_label(parent, UI_CONTENT_X + 4, STATUS_Y, ui_widget_text_style(UI_FONT_MEDIUM, UI_COLOR_WARNING), "");

#if TOOLSETTER_ENABLE
    setter_panel = ui_widget_create_panel(parent, UI_CONTENT_X, SETTER_Y, UI_CONTENT_W, 120);
    ui_widget_create_styled_label(setter_panel, 10, 8, title, "Tool setter");
    lbl_setter_input = ui_widget_create_styled_label(setter_panel, 200, 8, title, "");
    lbl_setter_pos = ui_widget_create_styled_label(setter_panel, 10, 32, small, "");
    lbl_setter_ofs = ui_widget_create_styled_label(setter_panel, 10, 50, small, "");
    txt_setter_input[0] = txt_setter_pos[0] = txt_setter_ofs[0] = '\0';
    ui_confirm_btn_create(&set_here, setter_panel, 10, 70, 160, 44, "Set here", BUTTON_STYLE_DEFAULT, do_set_here);
    btn_measure = ui_widget_create_button(setter_panel, 176, 70, 160, 44, LV_SYMBOL_DOWNLOAD " Measure", BUTTON_STYLE_PRIMARY, on_measure);
#endif

    last_update = 0;
}

static void screen_show (void)
{
    status("");
    input_shown = -1;
#if TOOLSETTER_ENABLE
    setter_shown = laser_shown = -1;
    ui_confirm_btn_cancel(&set_here);
#endif
    last_update = 0;
}

static void screen_update (void)
{
#if TOOLSETTER_ENABLE
    ui_confirm_btn_update(&set_here);
#endif

    if(last_update && lv_tick_elaps(last_update) < UPDATE_MS)
        return;

    last_update = lv_tick_get();
    if(last_update == 0)
        last_update = 1;

    // Live probe input, to check the clip before probing
    int8_t hit = ui_state.probe_triggered;
    if(hit != input_shown) {
        input_shown = hit;
        lv_label_set_style(lbl_input, LV_LABEL_STYLE_MAIN, ui_widget_text_style(UI_FONT_MEDIUM, hit ? UI_COLOR_SUCCESS : UI_COLOR_TEXT_DIM));
        ui_widget_set_text(lbl_input, txt_input, sizeof(txt_input), hit ? "Probe: touching" : "Probe: open");
    }

#if TOOLSETTER_ENABLE
    char buf[64];

    // A toolsetter does not apply to a laser
    if(ui_state.laser_mode != laser_shown) {
        laser_shown = ui_state.laser_mode;
        lv_obj_set_hidden(setter_panel, ui_state.laser_mode);
    }
    if(ui_state.laser_mode)
        return;

    int8_t setter = ui_state.toolsetter_triggered;
    if(setter != setter_shown) {
        setter_shown = setter;
        lv_label_set_style(lbl_setter_input, LV_LABEL_STYLE_MAIN, ui_widget_text_style(UI_FONT_MEDIUM, setter ? UI_COLOR_SUCCESS : UI_COLOR_TEXT_DIM));
        ui_widget_set_text(lbl_setter_input, txt_setter_input, sizeof(txt_setter_input), setter ? "Triggered" : "Open");
    }

    if(ui_state.tls_pos_set)
        snprintf(buf, sizeof(buf), "At X %.1f  Y %.1f  Z %.1f (machine)", ui_state.tls_pos[0], ui_state.tls_pos[1], ui_state.tls_pos[2]);
    else
        strcpy(buf, "Position not set: jog above it, Set here");
    ui_widget_set_text(lbl_setter_pos, txt_setter_pos, sizeof(txt_setter_pos), buf);

    if(ui_state.tlo_ref_set)
        snprintf(buf, sizeof(buf), "Reference set, tool offset Z %.3f", ui_state.tlo_z);
    else
        strcpy(buf, "No reference yet, the first Measure sets it");
    ui_widget_set_text(lbl_setter_ofs, txt_setter_ofs, sizeof(txt_setter_ofs), buf);
#endif
}

void screen_probe_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = NULL,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_PROBE, &callbacks);
}

#endif // TFT_ENABLE
