/*
 * tft_macros.c - Macro buttons, stored as grblHAL settings $450-$455
 *
 * Part of grblHAL TFT Plugin
 *
 * Each setting is "Label|G-code|G-code...", for example
 *   $451=Park|G53 G0 Z-1|G28
 * The label goes on the button, the lines run one after the other, each
 * once the previous one has finished. An empty setting hides the button.
 * $RST=& (or $RST=*) restores the TFT_MACRO_n defaults from tft_config.h.
 */

#include "driver.h"

#if TFT_ENABLE

#include <string.h>

#include "grbl/hal.h"
#include "grbl/settings.h"
#include "grbl/nvs_buffer.h"

#include "tft_config.h"
#include "tft_macros.h"
#include "tft_interface.h"

#define MACRO_SETTING(n) (setting_id_t)(Setting_UserDefined_0 + (n))

typedef struct {
    char macro[TFT_N_MACROS][TFT_MACRO_LEN + 1];
} macro_settings_t;

static macro_settings_t macros;
static nvs_address_t nvs_address;

static const char *const defaults[TFT_N_MACROS] = {
    TFT_MACRO_1, TFT_MACRO_2, TFT_MACRO_3, TFT_MACRO_4, TFT_MACRO_5, TFT_MACRO_6
};

static status_code_t macro_set (setting_id_t id, char *value)
{
    uint_fast8_t idx = id - Setting_UserDefined_0;

    if(idx >= TFT_N_MACROS || strlen(value) > TFT_MACRO_LEN)
        return Status_InvalidStatement;

    strcpy(macros.macro[idx], value);

    return Status_OK;
}

static char *macro_get (setting_id_t id)
{
    uint_fast8_t idx = id - Setting_UserDefined_0;

    return idx < TFT_N_MACROS ? macros.macro[idx] : "";
}

static const setting_group_detail_t macro_groups[] = {
    { Group_Root, Group_UserSettings, "TFT macros" }
};

#define MACRO_DETAIL(n, name) \
    { MACRO_SETTING(n), Group_UserSettings, name, NULL, Format_String, "x(80)", "0", "80", Setting_NonCoreFn, macro_set, macro_get, NULL }

static const setting_detail_t macro_settings[] = {
    MACRO_DETAIL(0, "TFT macro 1"),
    MACRO_DETAIL(1, "TFT macro 2"),
    MACRO_DETAIL(2, "TFT macro 3"),
    MACRO_DETAIL(3, "TFT macro 4"),
    MACRO_DETAIL(4, "TFT macro 5"),
    MACRO_DETAIL(5, "TFT macro 6"),
};

#define MACRO_DESCR "Touchscreen macro button: label, then G-code lines, separated by |. Example: Park|G53 G0 Z-1|G28. Empty hides the button."

static const setting_descr_t macro_descr[] = {
    { MACRO_SETTING(0), MACRO_DESCR },
    { MACRO_SETTING(1), MACRO_DESCR },
    { MACRO_SETTING(2), MACRO_DESCR },
    { MACRO_SETTING(3), MACRO_DESCR },
    { MACRO_SETTING(4), MACRO_DESCR },
    { MACRO_SETTING(5), MACRO_DESCR },
};

static void macro_save (void)
{
    hal.nvs.memcpy_to_nvs(nvs_address, (uint8_t *)&macros, sizeof(macros), true);
}

static void macro_restore (void)
{
    memset(&macros, 0, sizeof(macros));
    for(uint_fast8_t idx = 0; idx < TFT_N_MACROS; idx++)
        strncpy(macros.macro[idx], defaults[idx], TFT_MACRO_LEN);

    macro_save();
}

static void macro_load (void)
{
    if(hal.nvs.memcpy_from_nvs((uint8_t *)&macros, nvs_address, sizeof(macros), true) != NVS_TransferResult_OK)
        macro_restore();

    for(uint_fast8_t idx = 0; idx < TFT_N_MACROS; idx++)
        macros.macro[idx][TFT_MACRO_LEN] = '\0';
}

void tft_macros_init (void)
{
    static setting_details_t details = {
        .groups = macro_groups,
        .n_groups = sizeof(macro_groups) / sizeof(setting_group_detail_t),
        .settings = macro_settings,
        .n_settings = sizeof(macro_settings) / sizeof(setting_detail_t),
        .descriptions = macro_descr,
        .n_descriptions = sizeof(macro_descr) / sizeof(setting_descr_t),
        .save = macro_save,
        .load = macro_load,
        .restore = macro_restore
    };

    // Defaults in RAM, used as is if there is no room in NVS
    for(uint_fast8_t idx = 0; idx < TFT_N_MACROS; idx++)
        strncpy(macros.macro[idx], defaults[idx], TFT_MACRO_LEN);

    if((nvs_address = nvs_alloc(sizeof(macros))))
        settings_register(&details);
    else
        hal.stream.write("[MSG:TFT macros: no room in NVS, using defaults]" ASCII_EOL);
}

/*
 * UI task. The strings only change when a $45x setting is written, a torn
 * read then garbles one label until the next refresh.
 */

bool tft_macro_label (uint_fast8_t idx, char *buf, size_t size)
{
    if(idx >= TFT_N_MACROS || !macros.macro[idx][0] || !size)
        return false;

    const char *src = macros.macro[idx];
    const char *bar = strchr(src, '|');
    size_t len = bar ? (size_t)(bar - src) : strlen(src);

    if(len >= size)
        len = size - 1;
    memcpy(buf, src, len);
    buf[len] = '\0';

    return true;
}

bool tft_macro_run (uint_fast8_t idx)
{
    char copy[TFT_MACRO_LEN + 1];

    if(idx >= TFT_N_MACROS || !macros.macro[idx][0])
        return false;

    strcpy(copy, macros.macro[idx]);

    const char *lines = strchr(copy, '|');
    if(!lines || !lines[1])
        return false;   // a label only

    tft_send_lines(lines + 1);

    return true;
}

#endif // TFT_ENABLE
