/*
 * tft_touch.h - XPT2046 touch reading and calibration
 *
 * Part of grblHAL TFT Plugin
 *
 * Replaces TFT_eSPI's getTouch(), which drops touches outside the calibrated
 * range instead of clamping them to the screen edge.
 *
 * Runs in the UI task only, except tft_touch_request_calibration().
 */

#ifndef _TFT_TOUCH_H_
#define _TFT_TOUCH_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Raw ADC readings (0-4095) at the screen edges. The raw axes may be swapped
// and run in either direction, so x0 > x1 is fine.
typedef struct {
    int16_t x0, x1;     // raw value at screen x = 0 and x = width
    int16_t y0, y1;     // raw value at screen y = 0 and y = height
    uint8_t swap;       // screen x follows the raw y axis
} tft_touch_cal_t;

// Pressure needed for a touch. Lower for calibration, corners are less sensitive.
#define TOUCH_Z_THRESHOLD       600
#define TOUCH_Z_THRESHOLD_CAL   300

// Load the saved calibration, or the compiled default
void tft_touch_init(void);

// Debounced raw reading, false if not touched
bool tft_touch_read_raw(uint16_t *x, uint16_t *y, uint16_t threshold);

// Screen coordinates clamped to the display, false if not touched.
// Always false while calibrating, so LVGL sees no touches.
bool tft_touch_read(uint16_t *x, uint16_t *y);

// Map a raw reading to the screen with the given calibration, unclamped
void tft_touch_convert(const tft_touch_cal_t *cal, uint16_t rx, uint16_t ry, int32_t *x, int32_t *y);

const tft_touch_cal_t *tft_touch_get_calibration(void);

// Apply a calibration and save it to NVS
bool tft_touch_set_calibration(const tft_touch_cal_t *cal);

// Calibration screen active: LVGL gets no touches
void tft_touch_set_calibrating(bool on);

// Ask the UI task to start calibration. Safe from any task.
void tft_touch_request_calibration(void);

// True once per request
bool tft_touch_calibration_requested(void);

#ifdef __cplusplus
}
#endif

#endif // _TFT_TOUCH_H_
