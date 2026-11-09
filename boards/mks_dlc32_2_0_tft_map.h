/*
  mks_dlc32_2_0_tft_map.h - MKS DLC32 v2.x board map for use with the TFT plugin

  Driver code for ESP32

  Part of grblHAL TFT Plugin

  Based on mks_dlc32_2_0_map.h from the grblHAL ESP32 driver:
  Copyright (c) 2023-2024 Terje Io
  Copyright (c) 2022 Lucio Tarantino

  grblHAL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  grblHAL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with grblHAL. If not, see <http://www.gnu.org/licenses/>.

  The stock map assigns the EXP_1 LCD header to grblHAL (aux outputs on
  GPIO 25/26/5, spindle enable on GPIO 27, aux input on GPIO 33), which
  keeps the MKS TS24/TS35 display in reset. This map leaves those pins to
  the TFT plugin:

    GPIO 18/19/23  LCD SPI SCK/MISO/MOSI
    GPIO 25        LCD CS
    GPIO 26        Touch CS
    GPIO 27        LCD reset
    GPIO 33        LCD data/command (RS)
    GPIO 5         LCD backlight (LCD_EN)

  Use it with BOARD_MY_MACHINE: create main/boards/my_machine_map.h with
    #include "tft/boards/mks_dlc32_2_0_tft_map.h"
*/

#if VFD_SPINDLE
#error "Board MKS DLC32 + TFT does not have support for VFD spindle."
#endif

#if DRIVER_SPINDLE1_ENABLE
#error "Board MKS DLC32 + TFT does not have support for a second driver spindle."
#endif

#ifdef ADD_SERIAL1
#error "Board MKS DLC32 + TFT: UART1 pins (GPIO 18/19) are used by the LCD."
#endif

#include "use_i2s_out.h"

#define BOARD_NAME "MKS DLC32 2.x + TFT"
#define BOARD_URL "https://github.com/makerbase-mks/MKS-DLC32"

#define AUX_CONTROLS_OUT

#if SDCARD_ENABLE

// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define SPI_MISO_PIN        GPIO_NUM_12
#define SPI_MOSI_PIN        GPIO_NUM_13
#define SPI_SCK_PIN         GPIO_NUM_14
#define SD_CS_PIN           GPIO_NUM_15
#define SD_DETECT_PIN       GPIO_NUM_39

#endif // SDCARD_ENABLE

#define I2S_OUT_BCK         GPIO_NUM_16
#define I2S_OUT_WS          GPIO_NUM_17
#define I2S_OUT_DATA        GPIO_NUM_21

#define X_STEP_PIN          I2SO(1)
#define X_DIRECTION_PIN     I2SO(2)
#define X_LIMIT_PIN         GPIO_NUM_36

#define Y_STEP_PIN          I2SO(5)
#define Y_DIRECTION_PIN     I2SO(6)
#define Y_LIMIT_PIN         GPIO_NUM_35

#define Z_STEP_PIN          I2SO(3)
#define Z_DIRECTION_PIN     I2SO(4)
#define Z_LIMIT_PIN         GPIO_NUM_34

#define STEPPERS_ENABLE_PIN I2SO(0)

// Define ganged axis or A axis step pulse and step direction output pins.
#if N_ABC_MOTORS >= 1
#error "Board MKS DLC32 + TFT does not have support for ABC Motors"
#endif

#define AUXOUTPUT0_PIN          GPIO_NUM_32 // Laser/spindle PWM

// Define driver spindle pins.
// The board has no spindle enable or direction output. grblHAL's PWM spindle
// still needs them, so they go to I2S shift register bits 8/9: the board has a
// single 8-bit 74HC595 and the I2S stream is sent MSB first, so these bits are
// shifted out and never latched.
#if DRIVER_SPINDLE_ENABLE & SPINDLE_PWM
#define SPINDLE_PWM_PIN         AUXOUTPUT0_PIN
#endif
#if DRIVER_SPINDLE_ENABLE & SPINDLE_ENA
#define SPINDLE_ENABLE_PIN      I2SO(8)
#endif
#if DRIVER_SPINDLE_ENABLE & SPINDLE_DIR
#define SPINDLE_DIRECTION_PIN   I2SO(9)
#endif

// Define flood and mist coolant enable output pins.
// Flood was on GPIO 5 (LCD backlight) in the stock map, so it is not available.
#if COOLANT_ENABLE & COOLANT_MIST
#define COOLANT_MIST_PIN    I2SO(7)     // Beeper
#endif

#define AUXINPUT0_PIN       GPIO_NUM_22
#if !I2C_ENABLE
#define AUXINPUT1_PIN       GPIO_NUM_4 // Cycle start  // J2,4 (I2C_SCL)
#endif

// Define user-control controls (cycle start, reset, feed hold) input pins.
#if (CONTROL_ENABLE & CONTROL_CYCLE_START) && defined(AUXINPUT1_PIN)
#define CYCLE_START_PIN         AUXINPUT1_PIN
#endif

// Define probe switch input pin.
#if PROBE_ENABLE
#define PROBE_PIN           AUXINPUT0_PIN
#endif

#if I2C_ENABLE
// Define I2C port/pins
#define I2C_PORT            I2C_NUM_1
#define I2C_SDA             GPIO_NUM_0
#define I2C_SCL             GPIO_NUM_4
#define I2C_CLOCK           100000
#endif
