/*
 * lvgl_init.c - LVGL graphics library initialization
 *
 * Part of grblHAL TFT Plugin
 *
 * Copyright (c) 2025
 *
 * Ported from MKS_LVGL.cpp in grbl_esp32 firmware
 */

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "tft_config.h"
#include "tft_driver.h"
#include "lvgl_init.h"

#if TFT_ENABLE

// External TFT_eSPI instance (from tft_driver.c)
extern TFT_eSPI tft;

// LVGL display buffer
static lv_disp_buf_t disp_buf;
static lv_color_t bmp_public_buf[TFT_LVGL_BUFFER_SIZE];

// Forward declarations for callbacks
static void lvgl_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
static bool lvgl_touch_read(lv_indev_drv_t *indev, lv_indev_data_t *data);

/*
 * Initialize LVGL
 */
void lvgl_init(void) {
    // Initialize LVGL library
    lv_init();

    // Initialize display buffer (single buffer mode)
    lv_disp_buf_init(&disp_buf, bmp_public_buf, NULL, TFT_LVGL_BUFFER_SIZE);

    // Register display driver
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_DISPLAY_WIDTH;
    disp_drv.ver_res = TFT_DISPLAY_HEIGHT;
    disp_drv.flush_cb = lvgl_display_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    // Register touch input driver
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev_drv);
}

/*
 * LVGL display flush callback
 * Called by LVGL to update display with dirty region
 */
static void lvgl_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    // Calculate update region dimensions
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Set address window (clipping region) on ST7796
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);

#if TFT_USE_DMA
    // Push pixels using DMA (non-blocking)
    tft.pushColorsDMA(&color_p->full, w * h, true);
#else
    // Push pixels using blocking SPI
    tft.pushColors(&color_p->full, w * h, true);
#endif

    tft.endWrite();

    // Signal LVGL that flush is complete
    lv_disp_flush_ready(disp);
}

/*
 * LVGL touch input callback
 * Called by LVGL to read touch state
 */
static bool lvgl_touch_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    uint16_t touchX = 0, touchY = 0;
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;

    // Read touch from TFT_eSPI (handles XPT2046 protocol)
    boolean touched = tft.getTouch(&touchY, &touchX);

    if(touched) {
        // Boundary clamping
        if(touchX > TFT_DISPLAY_WIDTH) {
            touchX = TFT_DISPLAY_WIDTH;
        }
        if(touchY > TFT_DISPLAY_HEIGHT) {
            touchY = TFT_DISPLAY_HEIGHT;
        }

#if TFT_TOUCH_MIRROR_X
        // Mirror X-axis (required for MKS TS35)
        touchX = TFT_DISPLAY_WIDTH - touchX;
#endif

#if TFT_TOUCH_MIRROR_Y
        // Mirror Y-axis (required for MKS TS35)
        touchY = TFT_DISPLAY_HEIGHT - touchY;
#endif

        // Update last known position
        last_x = touchX;
        last_y = touchY;

        // Update LVGL input state
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_PR;  // Pressed

#if TFT_BEEP_ENABLE
        // Haptic feedback via beeper
        tft_beeper_on();
#endif
    }
    else {
        // Touch released
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_REL;  // Released

#if TFT_BEEP_ENABLE
        // Turn off beeper
        tft_beeper_off();
#endif
    }

    // Return false to indicate no more data to read
    return false;
}

/*
 * LVGL task handler
 */
void lvgl_task_handler(void) {
    lv_task_handler();
}

#endif // TFT_ENABLE
