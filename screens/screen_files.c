/*
 * screen_files.c - SD card file browser
 *
 * Part of grblHAL TFT Plugin
 *
 * Layout (480x320):
 *   top     Back (parent folder, or main screen), folder name, Reload
 *   middle  five rows of folders and G-code files, page up/down on the right
 *   overlay confirm before running a file
 *
 * The card is read in grblHAL's loop (tft_sd.c); this screen requests a
 * listing and redraws when it arrives. Starting a job switches to the job
 * screen from the UI loop once grblHAL reports it running.
 * Runs in the UI task only.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdio.h>
#include <string.h>

#include "screen_files.h"
#include "ui_screen_manager.h"
#include "ui_state.h"
#include "ui_widgets.h"
#include "tft_interface.h"
#include "tft_sd.h"

#define ROWS            5
#define ROW_X           12
#define ROW_Y           60
#define ROW_W           380
#define ROW_H           46
#define ROW_PITCH       50
#define START_WAIT_MS   4000    // give up waiting for the job to start

static lv_style_t s_title, s_name, s_dim, s_msg, s_big;

static lv_obj_t *lbl_title, *lbl_msg, *lbl_page, *btn_back, *btn_reload, *btn_up, *btn_down;
static lv_obj_t *btn_row[ROWS], *lbl_row_name[ROWS], *lbl_row_size[ROWS];
static lv_obj_t *confirm, *lbl_confirm_name, *lbl_confirm_info, *lbl_confirm_status, *btn_start, *btn_cancel, *btn_frame;

static char path[TFT_SD_PATH_LEN] = "/";
static char selected[TFT_SD_PATH_LEN + TFT_SD_NAME_LEN + 1];
static uint32_t listing_seq;        // seq of the listing shown
static bool waiting;                // listing requested, not here yet
static uint_fast8_t page;
static uint32_t start_seq, start_at, scan_seq;
static bool starting, framing;
static int16_t scan_shown;

static void styles_init (void)
{
    lv_style_copy(&s_title, &lv_style_plain);
    s_title.text.font = UI_FONT_LARGE;
    s_title.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_name, &lv_style_plain);
    s_name.text.font = UI_FONT_MEDIUM;
    s_name.text.color = UI_COLOR_TEXT;

    lv_style_copy(&s_dim, &s_name);
    s_dim.text.color = UI_COLOR_TEXT_DIM;

    lv_style_copy(&s_msg, &s_dim);
    s_msg.text.font = UI_FONT_LARGE;

    lv_style_copy(&s_big, &s_title);
    s_big.text.font = UI_FONT_XLARGE;
}

static void format_size (char *buf, size_t size, uint32_t bytes)
{
    if(bytes < 1024)
        snprintf(buf, size, "%u B", (unsigned)bytes);
    else if(bytes < 1024 * 1024)
        snprintf(buf, size, "%.1f KB", bytes / 1024.0f);
    else
        snprintf(buf, size, "%.1f MB", bytes / (1024.0f * 1024.0f));
}

static void join (char *buf, size_t size, const char *dir, const char *name)
{
    snprintf(buf, size, "%s%s%s", dir, strcmp(dir, "/") ? "/" : "", name);
}

static void request (void)
{
    tft_sd_list(path);
    waiting = true;
    page = 0;
    lv_label_set_text(lbl_msg, "Reading SD card...");
    lv_obj_set_hidden(lbl_msg, false);
    for(uint_fast8_t idx = 0; idx < ROWS; idx++)
        lv_obj_set_hidden(btn_row[idx], true);
}

static uint_fast8_t pages (const tft_sd_listing_t *list)
{
    return list->count ? (list->count + ROWS - 1) / ROWS : 1;
}

static void show_page (void)
{
    const tft_sd_listing_t *list = tft_sd_listing();
    char buf[24];

    const char *leaf = strrchr(list->path, '/');
    lv_label_set_text(lbl_title, strcmp(list->path, "/") && leaf ? leaf + 1 : "SD card");

    if(!list->mounted)
        lv_label_set_text(lbl_msg, "No SD card");
    else if(list->count == 0)
        lv_label_set_text(lbl_msg, "No G-code files here");
    lv_obj_set_hidden(lbl_msg, list->mounted && list->count);

    for(uint_fast8_t idx = 0; idx < ROWS; idx++) {
        uint_fast16_t n = page * ROWS + idx;
        bool used = n < list->count;

        lv_obj_set_hidden(btn_row[idx], !used);
        if(used) {
            const tft_sd_entry_t *e = &list->entry[n];
            char name[TFT_SD_NAME_LEN + 8];

            snprintf(name, sizeof(name), "%s  %s", e->dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE, e->name);
            lv_label_set_text(lbl_row_name[idx], name);
            if(e->dir)
                buf[0] = '\0';
            else
                format_size(buf, sizeof(buf), e->size);
            lv_label_set_text(lbl_row_size[idx], buf);
            lv_obj_align(lbl_row_size[idx], NULL, LV_ALIGN_IN_RIGHT_MID, -12, 0);
        }
    }

    snprintf(buf, sizeof(buf), "%u/%u%s", (unsigned)page + 1, (unsigned)pages(list), list->truncated ? "+" : "");
    lv_label_set_text(lbl_page, buf);
    lv_obj_align(lbl_page, btn_up, LV_ALIGN_OUT_BOTTOM_MID, 0, 18);
}

static void show_confirm (const tft_sd_entry_t *e)
{
    char size[16], info[96];

    format_size(size, sizeof(size), e->size);
    snprintf(info, sizeof(info), "%s, from the current work zero. Frame traces at the current Z.", size);

    lv_label_set_text(lbl_confirm_name, e->name);
    lv_label_set_text(lbl_confirm_info, info);
    lv_label_set_text(lbl_confirm_status, "");
    lv_obj_set_hidden(btn_start, false);
    starting = framing = false;
    lv_obj_set_hidden(confirm, false);
}

/*
 * Events
 */

static void on_row (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    const tft_sd_listing_t *list = tft_sd_listing();

    for(uint_fast8_t idx = 0; idx < ROWS; idx++) {
        uint_fast16_t n = page * ROWS + idx;
        if(obj == btn_row[idx] && n < list->count) {
            const tft_sd_entry_t *e = &list->entry[n];
            if(e->dir) {
                char sub[TFT_SD_PATH_LEN + TFT_SD_NAME_LEN + 1];
                join(sub, sizeof(sub), list->path, e->name);
                if(strlen(sub) < sizeof(path)) {
                    strcpy(path, sub);
                    request();
                }
            } else {
                join(selected, sizeof(selected), list->path, e->name);
                tft_log("file selected: %s", selected);
                show_confirm(e);
            }
        }
    }
}

static void on_nav (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    const tft_sd_listing_t *list = tft_sd_listing();

    if(obj == btn_back) {
        char *slash = strrchr(path, '/');
        if(!strcmp(path, "/") || !slash)
            ui_screen_manager_show(SCREEN_MAIN);
        else {
            if(slash == path)
                path[1] = '\0';
            else
                *slash = '\0';
            request();
        }
    } else if(obj == btn_reload)
        request();
    else if(obj == btn_up && !waiting && page > 0) {
        page--;
        show_page();
    } else if(obj == btn_down && !waiting && page + 1 < pages(list)) {
        page++;
        show_page();
    }
}

static void on_confirm (lv_obj_t *obj, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED)
        return;

    if(obj == btn_cancel) {
        starting = framing = false;
        lv_obj_set_hidden(confirm, true);
    } else if(obj == btn_frame && !framing && !starting) {
        if(ui_state.system_state != STATE_IDLE)
            lv_label_set_text(lbl_confirm_status, "Machine must be idle");
        else {
            framing = true;
            scan_shown = -1;
            scan_seq = tft_sd_bounds()->seq;
            tft_sd_scan(selected, ui_state.work_position[X_AXIS], ui_state.work_position[Y_AXIS]);
            lv_label_set_text(lbl_confirm_status, "Reading file...");
        }
    } else if(obj == btn_start && !starting) {
        if(ui_state.system_state != STATE_IDLE)
            lv_label_set_text(lbl_confirm_status, "Machine must be idle");
        else if(tft_sd_run(selected)) {
            starting = true;
            start_seq = tft_sd_job()->seq;
            start_at = lv_tick_get();
            lv_obj_set_hidden(btn_start, true);
            lv_label_set_text(lbl_confirm_status, "Starting...");
        } else
            lv_label_set_text(lbl_confirm_status, "Path too long");
    }
}

/*
 * Screen callbacks
 */

static lv_obj_t *create_label (lv_obj_t *parent, lv_style_t *style, const char *text)
{
    return ui_widget_create_styled_label(parent, 0, 0, style, text);
}

static void screen_create (lv_obj_t *parent)
{
    styles_init();
    lv_obj_set_style(parent, ui_widget_screen_style());

    btn_back = ui_widget_create_button(parent, 12, 8, 90, 44, LV_SYMBOL_LEFT " Back", BUTTON_STYLE_DEFAULT, on_nav);
    btn_reload = ui_widget_create_button(parent, 404, 8, 64, 44, LV_SYMBOL_REFRESH, BUTTON_STYLE_DEFAULT, on_nav);

    lbl_title = ui_widget_create_styled_label(parent, 116, 18, &s_title, "SD card");
    lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_title, 280);

    for(uint_fast8_t idx = 0; idx < ROWS; idx++) {
        lv_obj_t *btn = lv_btn_create(parent, NULL);
        lv_obj_set_pos(btn, ROW_X, ROW_Y + idx * ROW_PITCH);
        lv_obj_set_size(btn, ROW_W, ROW_H);
        lv_btn_set_layout(btn, LV_LAYOUT_OFF);
        ui_widget_set_button_style(btn, BUTTON_STYLE_DEFAULT);
        lv_obj_set_event_cb(btn, on_row);

        lbl_row_name[idx] = create_label(btn, &s_name, "");
        lv_label_set_long_mode(lbl_row_name[idx], LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl_row_name[idx], 270);
        lv_obj_align(lbl_row_name[idx], NULL, LV_ALIGN_IN_LEFT_MID, 12, 0);

        lbl_row_size[idx] = create_label(btn, &s_dim, "");
        lv_obj_align(lbl_row_size[idx], NULL, LV_ALIGN_IN_RIGHT_MID, -12, 0);

        btn_row[idx] = btn;
    }

    btn_up = ui_widget_create_button(parent, 404, ROW_Y, 64, 96, LV_SYMBOL_UP, BUTTON_STYLE_DEFAULT, on_nav);
    btn_down = ui_widget_create_button(parent, 404, ROW_Y + 5 * ROW_PITCH - 4 - 96, 64, 96, LV_SYMBOL_DOWN, BUTTON_STYLE_DEFAULT, on_nav);
    lbl_page = create_label(parent, &s_dim, "");

    lbl_msg = create_label(parent, &s_msg, "");
    lv_obj_set_auto_realign(lbl_msg, true);
    lv_obj_align(lbl_msg, NULL, LV_ALIGN_CENTER, -32, 0);

    // Confirm overlay, covers and blocks the list
    confirm = lv_obj_create(parent, NULL);
    lv_obj_set_size(confirm, TFT_DISPLAY_WIDTH, TFT_DISPLAY_HEIGHT);
    lv_obj_set_style(confirm, ui_widget_screen_style());

    lv_obj_t *lbl = ui_widget_create_styled_label(confirm, 16, 16, &s_title, "Run this file?");
    (void)lbl;

    lbl_confirm_name = ui_widget_create_styled_label(confirm, 16, 64, &s_big, "");
    lv_label_set_long_mode(lbl_confirm_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_confirm_name, 448);

    lbl_confirm_info = ui_widget_create_styled_label(confirm, 16, 110, &s_dim, "");
    lv_label_set_long_mode(lbl_confirm_info, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(lbl_confirm_info, 448);
    lbl_confirm_status = ui_widget_create_styled_label(confirm, 16, 170, &s_title, "");

    btn_cancel = ui_widget_create_button(confirm, 12, 248, 140, 60, "Cancel", BUTTON_STYLE_DEFAULT, on_confirm);
    btn_frame = ui_widget_create_button(confirm, 170, 248, 140, 60, "Frame", BUTTON_STYLE_PRIMARY, on_confirm);
    btn_start = ui_widget_create_button(confirm, 328, 248, 140, 60, LV_SYMBOL_PLAY " Start", BUTTON_STYLE_SUCCESS, on_confirm);

    lv_obj_set_hidden(confirm, true);

    listing_seq = tft_sd_listing()->seq;
}

static void screen_show (void)
{
    lv_obj_set_hidden(confirm, true);
    starting = false;
    request();      // also mounts a card inserted since the last visit
}

static void screen_update (void)
{
    const tft_sd_listing_t *list = tft_sd_listing();

    if(waiting && list->seq != listing_seq) {
        listing_seq = list->seq;
        waiting = false;
        // Keep the folder grblHAL actually listed
        strcpy(path, list->path);
        show_page();
    }

    if(framing) {
        const tft_sd_bounds_t *b = tft_sd_bounds();
        char buf[64];

        if(b->seq == scan_seq) {
            if(b->progress != scan_shown) {
                scan_shown = b->progress;
                snprintf(buf, sizeof(buf), "Reading file %u%%", (unsigned)b->progress);
                lv_label_set_text(lbl_confirm_status, buf);
            }
        } else {
            framing = false;
            if(!b->ok)
                lv_label_set_text(lbl_confirm_status, "No X/Y moves found");
            else if(ui_state.system_state != STATE_IDLE)
                lv_label_set_text(lbl_confirm_status, "Machine must be idle");
            else {
                snprintf(buf, sizeof(buf), "X %.1f to %.1f   Y %.1f to %.1f", b->min_x, b->max_x, b->min_y, b->max_y);
                lv_label_set_text(lbl_confirm_status, buf);
                tft_frame(b->min_x, b->min_y, b->max_x, b->max_y);
            }
        }
    }

    if(starting && tft_sd_job()->seq == start_seq && lv_tick_elaps(start_at) > START_WAIT_MS) {
        // The UI loop switches to the job screen when the job starts, so it did not
        starting = false;
        lv_obj_set_hidden(btn_start, false);
        lv_label_set_text(lbl_confirm_status, "Did not start, see the serial log");
    }
}

void screen_files_register (void)
{
    ui_screen_callbacks_t callbacks = {
        .create = screen_create,
        .show = screen_show,
        .hide = NULL,
        .update = screen_update,
        .destroy = NULL
    };

    ui_screen_manager_register(SCREEN_FILES, &callbacks);
}

#endif // TFT_ENABLE
