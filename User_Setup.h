// TFT_eSPI User Setup for MKS DLC32 v2.0/v2.1
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
// Hardware SPI pins for MKS DLC32
// ##################################################################################

#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   25  // Chip select
#define TFT_DC   33  // Data/Command (RS)
#define TFT_RST  27  // Reset

#define TOUCH_CS 26  // Touch controller chip select (XPT2046)

// ##################################################################################
// SPI bus frequency
// ##################################################################################

#define SPI_FREQUENCY       40000000    // 40 MHz for display
#define SPI_READ_FREQUENCY  20000000    // 20 MHz for reads
#define SPI_TOUCH_FREQUENCY  2000000    // 2 MHz for touch (XPT2046 requirement)

// ##################################################################################
// Optional features
// ##################################################################################

#define SMOOTH_FONT  // Enable smooth font rendering

#define TFT_WIDTH  480
#define TFT_HEIGHT 320

#define TOUCH_THRESHOLD 600 // Touch detection threshold

#endif // USER_SETUP_H
