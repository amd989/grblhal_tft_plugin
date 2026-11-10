/*
 * tft_touch.cpp - XPT2046 touch reading and calibration
 *
 * Part of grblHAL TFT Plugin
 *
 * The calibration is kept in the ESP32 NVS partition (namespace "tft"), apart
 * from grblHAL's settings, so a settings reset does not lose it.
 */

#include "tft_config.h"

#if TFT_ENABLE

#include <stdlib.h>
#include <TFT_eSPI.h>
#include <nvs_flash.h>     // ESP-IDF nvs.h, grblHAL has its own nvs.h
#include "tft_touch.h"

extern TFT_eSPI tft;

#define NVS_NAMESPACE   "tft"
#define NVS_KEY         "touch_cal"
#define CAL_VERSION     1

#define RAW_DEADBAND    20      // max difference between two samples of one touch
#define RAW_SAMPLES     5
#define HOLD_MS         50      // keep a touch alive at low pressure this long

typedef struct {
    uint16_t version;
    tft_touch_cal_t cal;
} tft_touch_cal_record_t;

static tft_touch_cal_t calibration = {
    .x0 = TFT_TOUCH_CAL_X0,
    .x1 = TFT_TOUCH_CAL_X1,
    .y0 = TFT_TOUCH_CAL_Y0,
    .y1 = TFT_TOUCH_CAL_Y1,
    .swap = TFT_TOUCH_CAL_SWAP
};

static volatile bool calibrating = false;
static volatile bool cal_requested = false;

static bool cal_valid (const tft_touch_cal_t *cal)
{
    return abs(cal->x1 - cal->x0) > 500 && abs(cal->y1 - cal->y0) > 500;
}

void tft_touch_init (void)
{
    nvs_handle_t handle;
    tft_touch_cal_record_t rec;
    size_t size = sizeof(rec);

    if(nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return;

    if(nvs_get_blob(handle, NVS_KEY, &rec, &size) == ESP_OK &&
        size == sizeof(rec) && rec.version == CAL_VERSION && cal_valid(&rec.cal))
        calibration = rec.cal;

    nvs_close(handle);
}

bool tft_touch_set_calibration (const tft_touch_cal_t *cal)
{
    nvs_handle_t handle;
    tft_touch_cal_record_t rec = { .version = CAL_VERSION, .cal = *cal };
    bool ok = false;

    if(!cal_valid(cal))
        return false;

    calibration = *cal;

    if(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        ok = nvs_set_blob(handle, NVS_KEY, &rec, sizeof(rec)) == ESP_OK && nvs_commit(handle) == ESP_OK;
        nvs_close(handle);
    }

    return ok;
}

const tft_touch_cal_t *tft_touch_get_calibration (void)
{
    return &calibration;
}

// One position sample, following TFT_eSPI's validTouch(): wait for the
// pressure to settle, then take two readings that must agree.
static bool sample (uint16_t *x, uint16_t *y, uint16_t threshold)
{
    uint16_t z1 = 1, z2 = 0, xa, ya, xb, yb;
    uint_fast8_t tries = 10;

    while(z1 > z2 && tries--) {
        z2 = z1;
        z1 = tft.getTouchRawZ();
        delay(1);
    }

    if(z1 <= threshold)
        return false;

    tft.getTouchRaw(&xa, &ya);
    delay(1);
    if(tft.getTouchRawZ() <= threshold)
        return false;
    tft.getTouchRaw(&xb, &yb);

    if(abs(xa - xb) > RAW_DEADBAND || abs(ya - yb) > RAW_DEADBAND)
        return false;

    *x = (xa + xb) / 2;
    *y = (ya + yb) / 2;

    return true;
}

bool tft_touch_read_raw (uint16_t *x, uint16_t *y, uint16_t threshold)
{
    static uint32_t hold_until = 0;
    uint32_t sx = 0, sy = 0;
    uint_fast8_t valid = 0;
    uint16_t rx, ry;

    // Once pressed, a light touch is enough to keep it pressed
    if((int32_t)(hold_until - millis()) > 0)
        threshold = 20;

    for(uint_fast8_t idx = 0; idx < RAW_SAMPLES; idx++) {
        if(sample(&rx, &ry, threshold)) {
            sx += rx;
            sy += ry;
            valid++;
        }
    }

    if(!valid) {
        hold_until = 0;
        return false;
    }

    hold_until = millis() + HOLD_MS;
    *x = sx / valid;
    *y = sy / valid;

    return true;
}

void tft_touch_convert (const tft_touch_cal_t *cal, uint16_t rx, uint16_t ry, int32_t *x, int32_t *y)
{
    int32_t a = cal->swap ? ry : rx;
    int32_t b = cal->swap ? rx : ry;

    *x = (a - cal->x0) * TFT_DISPLAY_WIDTH / (cal->x1 - cal->x0);
    *y = (b - cal->y0) * TFT_DISPLAY_HEIGHT / (cal->y1 - cal->y0);
}

static uint16_t clamp (int32_t v, int32_t max)
{
    return v < 0 ? 0 : (v >= max ? max - 1 : v);
}

bool tft_touch_read (uint16_t *x, uint16_t *y)
{
    uint16_t rx, ry;
    int32_t sx, sy;

    if(calibrating || !tft_touch_read_raw(&rx, &ry, TOUCH_Z_THRESHOLD))
        return false;

    tft_touch_convert(&calibration, rx, ry, &sx, &sy);
    *x = clamp(sx, TFT_DISPLAY_WIDTH);
    *y = clamp(sy, TFT_DISPLAY_HEIGHT);

    return true;
}

void tft_touch_set_calibrating (bool on)
{
    calibrating = on;
}

void tft_touch_request_calibration (void)
{
    cal_requested = true;
}

bool tft_touch_calibration_requested (void)
{
    bool req = cal_requested;
    cal_requested = false;
    return req;
}

#endif // TFT_ENABLE
