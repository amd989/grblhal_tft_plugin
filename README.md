# grblHAL TFT Touchscreen Plugin

A touchscreen user interface for [grblHAL](https://github.com/grblHAL) on the ESP32, built on LVGL and TFT_eSPI. Developed and tested on the MKS DLC32 v2.0/v2.1 with the MKS TS35 display.

**Documentation lives in the [wiki](https://github.com/amd989/grblhal_tft_plugin/wiki).**

## Features

- A layout built around Jog: status bar, five tabs (Jog, Files, Probe, Macros, Tools), and an action bar with Hold, Home, Unlock and Reset, see [Screen Layout](https://github.com/amd989/grblhal_tft_plugin/wiki/Screen-Layout)
- Machine state with the alarm reason, work/machine position
- Jogging with diagonals and continuous jog, set work zero by tapping a position, go to zero, Z probe with a touch plate, tool length measurement on a toolsetter, spindle on/off
- SD card file browser and job screen: progress, elapsed and remaining time, feed and spindle overrides, hold and stop
- Frame: trace the outline of a file's X/Y moves before running it
- Resume a job from a line, after a broken bit or a stop
- Tool changes (M6): the screen asks for the next tool, Resume continues, with `$341=3` after measuring it on the toolsetter
- Six macro buttons, set with `$450`-`$455`
- CNC and laser: the same firmware adapts to laser mode (`$32=1`), see [Laser Mode](https://github.com/amd989/grblhal_tft_plugin/wiki/Laser-Mode)
- Wi-Fi status in the status bar (struck through while down) and a badge while a PC talks to the machine
- Risky actions (spindle on, Stop, Reset) confirmed by tapping twice or holding 1 s (`$458`)
- Tools tab: brightness (`$456`), touch beep (`$457`), touch calibration, network and version info, see [Settings](https://github.com/amd989/grblhal_tft_plugin/wiki/Settings)
- Touch calibration (`$TFTCAL` or the Tools tab), saved across resets, and a click on each touch

## Hardware

- ESP32 running the [grblHAL ESP32 driver](https://github.com/grblHAL/ESP32)
- ST7796 (or compatible) TFT display with an XPT2046 (or compatible) touch controller
- Recommended: MKS DLC32 v2.0/v2.1 with the MKS TS35

## Quick start

From the root of the grblHAL ESP32 project (next to `platformio.ini`):

1. Add the plugin:

   ```bash
   cd main
   git submodule add https://github.com/amd989/grblhal_tft_plugin.git tft
   git submodule update --init --recursive
   ```

2. Create `3rdParty.cmake` in the project root:

   ```cmake
   include(${CMAKE_CURRENT_LIST_DIR}/main/tft/tft_plugin.cmake)
   ```

3. Create `main/boards/my_machine_map.h` (the stock DLC32 map uses the LCD pins for other things):

   ```c
   #include "tft/boards/mks_dlc32_2_0_tft_map.h"
   ```

4. In `main/my_machine.h`:

   ```c
   #define BOARD_MY_MACHINE
   #define TFT_ENABLE 1
   #define SDCARD_ENABLE 1     // optional, for the Files and job screens
   ```

5. Build and flash:

   ```bash
   pio run -t upload && pio device monitor
   ```

Send `$I`: the list should include `[PLUGIN:TFT UI v0.3]`.

Full steps and details: [Installation](https://github.com/amd989/grblhal_tft_plugin/wiki/Installation).

## Tests

The logic that does not need the board (G-code reading for Frame and resume from line, macros, job progress, the command lines it sends) lives in `tft_gcode.c` and `tft_logic.c`, with no grblHAL or LVGL calls. Its tests build and run on a PC with gcc or clang:

```sh
./test/run.sh
```

They also run on every push, see `.github/workflows/tests.yml`. Details in [Development](https://github.com/amd989/grblhal_tft_plugin/wiki/Development#tests).

## Wiki

- [Installation](https://github.com/amd989/grblhal_tft_plugin/wiki/Installation)
- [Board Map and Wiring](https://github.com/amd989/grblhal_tft_plugin/wiki/Board-Map-and-Wiring): toolsetter or cycle start on J2, beeper, pins
- [Configuration](https://github.com/amd989/grblhal_tft_plugin/wiki/Configuration): every option you can override in `my_machine.h`
- [Screen Layout](https://github.com/amd989/grblhal_tft_plugin/wiki/Screen-Layout): status bar, tabs, action bar, confirming risky actions
- [Jog and Probing](https://github.com/amd989/grblhal_tft_plugin/wiki/Jog-and-Probing)
- [Running Jobs](https://github.com/amd989/grblhal_tft_plugin/wiki/Running-Jobs): SD card files, Frame, resume from line
- [Tool Changes](https://github.com/amd989/grblhal_tft_plugin/wiki/Tool-Changes): M6 prompt and `$341=3` with a toolsetter
- [Laser Mode](https://github.com/amd989/grblhal_tft_plugin/wiki/Laser-Mode): Laser button, framing with the laser, settings to check
- [Macros](https://github.com/amd989/grblhal_tft_plugin/wiki/Macros): the six buttons set with `$450`-`$455`
- [Touch Calibration](https://github.com/amd989/grblhal_tft_plugin/wiki/Touch-Calibration)
- [Network and WebUI](https://github.com/amd989/grblhal_tft_plugin/wiki/Network-and-WebUI)
- [Troubleshooting](https://github.com/amd989/grblhal_tft_plugin/wiki/Troubleshooting)
- [Development](https://github.com/amd989/grblhal_tft_plugin/wiki/Development)

## License

GPL v3, same as grblHAL.

## Credits

- Based on the MKS DLC32 firmware TFT integration
- [LVGL](https://lvgl.io/) graphics library
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) display driver
