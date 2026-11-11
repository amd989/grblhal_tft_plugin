/*
 * tft_settings.c - Screen settings, stored as grblHAL settings
 *
 * Part of grblHAL TFT Plugin
 *
 *   $456  screen brightness, percent (TFT_BRIGHTNESS_MIN-100)
 *   $457  touch beep, 0 or 1
 *   $458  confirm risky actions (spindle on, Stop, Reset): 0 tap twice,
 *         1 hold for a second
 *
 * Set from the Settings screen or a console. They have their own NVS area,
 * apart from the macros, so adding them kept the saved macros.
 */

#include "driver.h"

#if TFT_ENABLE

#include <string.h>

#include "grbl/hal.h"
#include "grbl/settings.h"
#include "grbl/nvs_buffer.h"

#include "tft_config.h"
#include "tft_settings.h"
#include "tft_logic.h"

#define SETTING_BRIGHTNESS  Setting_UserDefined_6
#define SETTING_BEEP        Setting_UserDefined_7
#define SETTING_CONFIRM     Setting_UserDefined_8
#define NO_REQUEST          0xFF

typedef struct {
    uint8_t brightness;
    uint8_t beep;
} screen_settings_t;

static screen_settings_t screen;
static nvs_address_t nvs_address;

// $458 came later, in its own NVS area so the others kept their values
static uint8_t confirm_hold;
static nvs_address_t nvs_confirm;

// Set by the UI task, stored by grblHAL's loop
static volatile uint8_t req_brightness = NO_REQUEST, req_beep = NO_REQUEST, req_confirm = NO_REQUEST;

static const setting_group_detail_t screen_groups[] = {
    { Group_Root, Group_UserSettings, "TFT screen" }
};

static const setting_detail_t screen_settings[] = {
    { SETTING_BRIGHTNESS, Group_UserSettings, "TFT brightness", "percent", Format_Int8, "##0", "10", "100", Setting_NonCore, &screen.brightness, NULL, NULL },
    { SETTING_BEEP, Group_UserSettings, "TFT touch beep", NULL, Format_Bool, NULL, NULL, NULL, Setting_NonCore, &screen.beep, NULL, NULL },
    { SETTING_CONFIRM, Group_UserSettings, "TFT confirm by", NULL, Format_RadioButtons, "Tapping twice,Holding 1 s", NULL, NULL, Setting_NonCore, &confirm_hold, NULL, NULL },
};

static const setting_descr_t screen_descr[] = {
    { SETTING_BRIGHTNESS, "Touchscreen backlight brightness in percent, 10 to 100." },
    { SETTING_BEEP, "Click the beeper on each touch of the touchscreen." },
    { SETTING_CONFIRM, "How the touchscreen confirms spindle on, Stop and Reset: tapping twice within 3 s, or holding for 1 s." },
};

static void screen_save (void)
{
    hal.nvs.memcpy_to_nvs(nvs_address, (uint8_t *)&screen, sizeof(screen), true);
    if(nvs_confirm)
        hal.nvs.memcpy_to_nvs(nvs_confirm, &confirm_hold, sizeof(confirm_hold), true);
}

static void screen_restore (void)
{
    screen.brightness = TFT_BRIGHTNESS;
    screen.beep = TFT_BEEP_ENABLE ? 1 : 0;
    confirm_hold = TFT_CONFIRM_HOLD;

    screen_save();
}

static void screen_load (void)
{
    if(hal.nvs.memcpy_from_nvs((uint8_t *)&screen, nvs_address, sizeof(screen), true) != NVS_TransferResult_OK)
        screen_restore();
    else if(nvs_confirm && hal.nvs.memcpy_from_nvs(&confirm_hold, nvs_confirm, sizeof(confirm_hold), true) != NVS_TransferResult_OK) {
        confirm_hold = TFT_CONFIRM_HOLD;    // first boot with $458
        hal.nvs.memcpy_to_nvs(nvs_confirm, &confirm_hold, sizeof(confirm_hold), true);
    }

    if(screen.brightness < TFT_BRIGHTNESS_MIN || screen.brightness > 100)
        screen.brightness = TFT_BRIGHTNESS;
    screen.beep = screen.beep ? 1 : 0;
    confirm_hold = confirm_hold ? 1 : 0;
}

void tft_settings_init (void)
{
    static setting_details_t details = {
        .groups = screen_groups,
        .n_groups = sizeof(screen_groups) / sizeof(setting_group_detail_t),
        .settings = screen_settings,
        .n_settings = sizeof(screen_settings) / sizeof(setting_detail_t),
        .descriptions = screen_descr,
        .n_descriptions = sizeof(screen_descr) / sizeof(setting_descr_t),
        .save = screen_save,
        .load = screen_load,
        .restore = screen_restore
    };

    // Defaults in RAM, used as is if there is no room in NVS
    screen.brightness = TFT_BRIGHTNESS;
    screen.beep = TFT_BEEP_ENABLE ? 1 : 0;
    confirm_hold = TFT_CONFIRM_HOLD;

    if((nvs_address = nvs_alloc(sizeof(screen)))) {
        nvs_confirm = nvs_alloc(sizeof(confirm_hold));
        settings_register(&details);
    }
    else
        hal.stream.write("[MSG:TFT settings: no room in NVS, using defaults]" ASCII_EOL);
}

uint8_t tft_settings_brightness (void)
{
    uint8_t value = req_brightness;

    return value != NO_REQUEST ? value : screen.brightness;
}

bool tft_settings_beep (void)
{
    uint8_t value = req_beep;

    return (value != NO_REQUEST ? value : screen.beep) != 0;
}

bool tft_settings_confirm_hold (void)
{
    uint8_t value = req_confirm;

    return (value != NO_REQUEST ? value : confirm_hold) != 0;
}

void tft_settings_set_confirm_hold (bool on)
{
    req_confirm = on ? 1 : 0;
}

void tft_settings_set_brightness (uint8_t percent)
{
    req_brightness = tft_logic_brightness(percent);
}

void tft_settings_set_beep (bool on)
{
    req_beep = on ? 1 : 0;
}

void tft_settings_poll (void)
{
    uint8_t brightness = req_brightness, beep = req_beep, confirm = req_confirm;

    if(brightness == NO_REQUEST && beep == NO_REQUEST && confirm == NO_REQUEST)
        return;

    if(brightness != NO_REQUEST)
        screen.brightness = brightness;
    if(beep != NO_REQUEST)
        screen.beep = beep;
    if(confirm != NO_REQUEST)
        confirm_hold = confirm;

    // Clear only what was stored, a newer request waits for the next poll
    if(req_brightness == brightness)
        req_brightness = NO_REQUEST;
    if(req_beep == beep)
        req_beep = NO_REQUEST;
    if(req_confirm == confirm)
        req_confirm = NO_REQUEST;

    if(nvs_address)
        screen_save();
}

#endif // TFT_ENABLE
