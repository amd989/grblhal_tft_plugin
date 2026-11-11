// TFT_eSPI User Setup for the MKS TS35 (ST7796 + XPT2046)
// This file overrides the default User_Setup.h in the TFT_eSPI submodule
// Created for grblHAL TFT plugin

#ifndef USER_SETUP_H
#define USER_SETUP_H

#define USER_SETUP_INFO "MKS_DLC32_grblHAL"

// Tell TFT_eSPI that user setup is already loaded
#define USER_SETUP_LOADED

// ##################################################################################
// Display driver
// ##################################################################################

#define ST7796_DRIVER      // MKS TS35 uses ST7796 controller (480x320)

// ##################################################################################
// Fonts
// ##################################################################################

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

// ##################################################################################
// Pins and SPI clocks, from tft_config.h (DLC32 defaults, board presets, or
// overrides in my_machine.h)
// ##################################################################################

#include "tft_config.h"

#define TFT_MISO TFT_MISO_PIN
#define TFT_MOSI TFT_MOSI_PIN
#define TFT_SCLK TFT_SCLK_PIN
#define TFT_CS   TFT_CS_PIN     // Chip select
#define TFT_DC   TFT_DC_PIN     // Data/Command (RS)
#define TFT_RST  TFT_RST_PIN    // Reset

#define TOUCH_CS TOUCH_CS_PIN   // Touch controller chip select (XPT2046)

#define SPI_FREQUENCY       TFT_SPI_FREQUENCY       // display writes
#define SPI_READ_FREQUENCY  TFT_SPI_READ_FREQUENCY  // display reads
#define SPI_TOUCH_FREQUENCY TOUCH_SPI_FREQUENCY     // XPT2046 limit

// On a bus shared with the SD card, TFT_eSPI wraps every display and touch
// access in SPI.beginTransaction()/endTransaction(), which lock the bus.
#if TFT_SPI_SHARED
#define SPI_HAS_TRANSACTION
#endif

// ##################################################################################
// Optional features
// ##################################################################################

#define SMOOTH_FONT  // Enable smooth font rendering

// Native (portrait) panel size. TFT_eSPI swaps these for landscape rotations.
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

#define TOUCH_THRESHOLD 600 // Touch detection threshold

#endif // USER_SETUP_H
