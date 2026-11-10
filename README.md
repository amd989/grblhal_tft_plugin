# grblHAL TFT Touchscreen Plugin

A touchscreen user interface plugin for grblHAL using LVGL and TFT_eSPI.

## Features

- Machine state with the alarm reason, work/machine position, Home, Unlock, Hold, Reset
- Jogging, set work zero, go to zero, Z probe with a touch plate, spindle on/off
- SD card file browser and job screen: progress, elapsed time, feed and spindle overrides, hold and stop
- Frame: trace the outline of a file's X/Y moves before running it
- Wi-Fi or Ethernet status with the IP address on the main screen
- Touch calibration (`$TFTCAL`), saved across resets, and a click on each touch

## Hardware Requirements

- ESP32 microcontroller
- TFT display (ST7796 or compatible)
- Touch controller (XPT2046 or compatible)
- Recommended: MKS DLC32 v2.0/v2.1 board

## Installation

### Step 1: Add Plugin to grblHAL

This plugin is designed to be added as a Git submodule to the grblHAL ESP32 project.

```bash
cd grblhal/main
git submodule add https://github.com/amd989/grblhal_tft_plugin.git tft
git submodule update --init --recursive
```

### Step 2: Hook it into the build

Create `3rdParty.cmake` in the root of the grblHAL ESP32 project (next to `platformio.ini`):

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/main/tft/tft_plugin.cmake)
```

The driver's `main/CMakeLists.txt` already includes this file if it exists. `tft_plugin.cmake` adds the plugin, LVGL and TFT_eSPI sources and takes over grblHAL's `my_plugin_init()` hook, so neither the driver nor the core needs editing.

If you already have your own `main/my_plugin.c`, the hook is left alone. Call `tft_plugin_init()` from your `my_plugin_init()` instead.

### Step 3: Select the board map

The stock `BOARD_MKS_DLC32_V2P0` map gives the LCD header pins to grblHAL (aux outputs, spindle enable on the LCD reset line, an aux input on LCD data/command), which keeps the display dark. The plugin ships a DLC32 map that leaves them to the display. Create `main/boards/my_machine_map.h`:

```c
#include "tft/boards/mks_dlc32_2_0_tft_map.h"
```

Differences from the stock map: the laser PWM stays on GPIO 32, there is no flood coolant output (it was on the backlight pin), no second spindle and no UART1. The board has no spindle enable or direction output, so grblHAL's are parked on unwired shift register bits.

Cycle start is on J2 pin 4 (GPIO 4). With nothing wired it idles high, which grblHAL reports as an active input (`Pn:S`). Send `$14=4` to invert it, so the idle pin reads inactive and a normally open button to GND works. To make that the default after `$RST`, add to `my_machine.h`:

```c
#define DEFAULT_CONTROL_SIGNALS_INVERT_MASK SIGNALS_CYCLESTART_BIT
```

### Step 4: Enable Plugin

Edit `main/my_machine.h`:

```c
#define BOARD_MY_MACHINE
#define TFT_ENABLE 1
#define SDCARD_ENABLE 1     // optional, for the Files and job screens
```

The build stops with an error if `TFT_ENABLE` is used with the stock `BOARD_MKS_DLC32_V2P0` map.

### Step 5: Build

```bash
pio run
```

## Configuration

Defaults for the MKS DLC32 v2.0/v2.1 with the MKS TS35 display live in `tft_config.h`. Override any of them by defining it in `my_machine.h`, for example:

```c
#define TFT_ROTATION 3          // 0-3
#define TFT_BL_PIN 5            // Backlight enable
#define LVGL_REFRESH_PERIOD 10  // ms
```

See `tft_config.h` for the full list (pins, SPI frequencies, backlight PWM, LVGL buffer).

## Running jobs from the SD card

Needs `SDCARD_ENABLE` in `my_machine.h`. **Files** on the main screen lists folders and G-code files (`.nc`, `.ngc`, `.gcode`, `.tap`, `.txt` and the other extensions grblHAL's `$F` accepts). Hidden files and names longer than 40 characters are left out, grblHAL cannot run the latter. Opening the screen mounts the card, so a card inserted after boot works too.

Tapping a file asks for confirmation, then runs it with `$F=<path>` from the current work zero. The machine must be idle. The job screen opens by itself whenever a job starts, also one started from a host:

- progress, current line and elapsed time
- feed and spindle override: -10%, back to 100%, +10%
- Hold / Resume
- Stop, tap twice: motion ramps down, spindle and coolant turn off, the job ends and the position is kept

While a job runs, grblHAL ignores everything from the serial port except real-time commands (`?`, `!`, `~` and overrides), so a host like CNCjs can watch, hold and resume, but not send G-code.

**Frame** on the confirm panel reads the file for the X/Y extent of its moves (absolute and relative, mm and inch, arcs at their true extent; G53, G92, G10, G28/G30 and probing lines are left out) and traces that rectangle as jogs at the current Z, so raise Z first. Stop on the jog screen cancels the rest of the trace.

`examples/tft-test-air.nc` is a safe first job: ten 20 mm squares in X/Y with relative moves and the spindle off. Jog to the middle of the bed and raise Z before running it.

## Jog screen

- Step jogging of X/Y/Z (0.1, 1, 10, 100 mm), Stop cancels a jog
- Zero XY, Zero Z: set work zero (`G10 L20 P0`)
- Go XY0, Go Z0: jog to work zero, Stop cancels it. Go XY0 does not lift Z first.
- Spindle: tap twice to start at `TFT_SPINDLE_RPM`, once to stop
- Probe Z: touch plate probing. The panel shows the probe input live, so the clip can be checked first. It probes down at `TFT_PROBE_FEED` up to `TFT_PROBE_DISTANCE`, again slowly from 2 mm above, sets Z0 on the work surface and lifts `TFT_PROBE_RETRACT` above the plate. A failed probe, Stop or Reset drops the remaining steps, so no zero is set from a failed probe.

## Network status

With `WIFI_ENABLE` (or `ETHERNET_ENABLE`) the main screen shows the first interface, for example `Wi-Fi 192.168.1.42` or `Wi-Fi not connected`. Set the network with `$74=<SSID>` and `$75=<password>`, then reset.

The ESP32 driver's `platformio.ini` defines `NETWORK_STA_HOSTNAME` unquoted, which breaks the build once Wi-Fi is enabled. Comment that line out or quote the value.

### WebUI and mDNS (driver options, not part of this plugin)

Tested on the DLC32 alongside the TFT:

- `WEBUI_ENABLE 3` in `my_machine.h` adds ESP3D-WebUI v3. Put its `index.html.gz` ([ESP3D-WEBUI 3.0, dist/CNC/GRBLHal](https://github.com/luc-github/ESP3D-WEBUI/tree/3.0/dist/CNC/GRBLHal)) on the SD card as `/www/index.html.gz`. Free memory drops to about 84 KB.
- `-DMDNS_ENABLE=1` in `platformio.ini` build flags (not `my_machine.h`) makes the board answer as `grblhal.local`.
- Network services `$70=47`: telnet, websocket, HTTP, FTP and mDNS.
- `$326` is the HTTP port (80) and `$327` the websocket port (81). With both on 80 the websocket daemon answers every page with 400 Bad Request.
- grblHAL's FTP server cannot change directory; store with a full path, e.g. `STOR /www/index.html.gz`.

The WebUI, telnet and USB share one command stream: the last one to connect takes it over.

## Touch beep

The DLC32 beeper sits on shift register output `I2SO(7)`. The plugin clicks it for `TFT_BEEP_MS` on each touch; set `TFT_BEEP_ENABLE 0` to turn it off. The stock map used that output for mist coolant, the plugin map leaves the board without coolant outputs.

## Jog screen probe settings

Set the plate thickness in `my_machine.h`, it defaults to 0:

```c
#define TFT_PROBE_THICKNESS 20.0f   // mm
```

## Architecture

```
main/tft/
├── tft_plugin.c          # Plugin entry point and event handlers
├── tft_my_plugin.c       # my_plugin_init() hook, calls tft_plugin_init()
├── tft_plugin.cmake      # Build glue, included from 3rdParty.cmake
├── tft_config.h          # Pins and settings (defaults, override in my_machine.h)
├── arduino_compat.h      # Arduino API shim for TFT_eSPI on ESP-IDF
├── arduino_spi.cpp       # SPIClass on the ESP-IDF spi_master driver
├── boards/               # Board maps that keep the LCD pins free
├── tft_interface.c/.h    # Communication with grblHAL
├── tft_driver.cpp/.h     # TFT_eSPI wrapper
├── lvgl_init.cpp/.h      # LVGL initialization
├── ui_state.c/.h         # Cached machine state for the UI
├── ui_screen_manager.c/.h# Screen registration and switching
├── ui_widgets.c/.h       # Reusable LVGL widgets
├── screens/              # Screens (screen_main: state, position, Home/Unlock/Hold/Reset)
└── lib/                  # LVGL v6.1.2 and TFT_eSPI submodules
```

## Usage

The UI is event-driven and integrates seamlessly with grblHAL:

### Sending Commands

```c
// From UI code
#include "tft_interface.h"

void on_home_button_click(void) {
    tft_send_command("$H\n");
}

void on_jog_x_plus(void) {
    tft_jog_axis(X_AXIS, 10.0, 1000.0);
}
```

### Receiving Events

The plugin automatically receives grblHAL events:

- `on_state_change()` - Machine state changes
- `on_realtime_report()` - Position and status updates
- `on_program_completed()` - Job completion
- `on_probe_completed()` - Probe cycle done
- `on_homing_completed()` - Homing done

## Development

### Adding a New Screen

1. Create `screens/screen_myfeature.c`
2. Add it to the source list in `tft_plugin.cmake`
3. Create drawing function:

```c
void screen_myfeature_draw(void) {
    // Create LVGL widgets
}

void screen_myfeature_update(void) {
    // Update widget values
}
```

4. Call from state machine in `tft_plugin.c`

### Event Handling Pattern

Always chain event handlers:

```c
static on_state_change_ptr on_state_change;  // Save previous

void tft_plugin_init(void) {
    on_state_change = grbl.on_state_change;  // Save
    grbl.on_state_change = my_state_handler;  // Replace
}

void my_state_handler(sys_state_t state) {
    // Your code here

    if(on_state_change)  // Chain to previous
        on_state_change(state);
}
```

## Testing

### Minimal Test

1. Build and flash
2. Open serial monitor
3. Look for: `[TFT Plugin initialized]`
4. Send `$I` command
5. Should see: `[PLUGIN:TFT UI v0.2]`

### Display Test

1. Screen should show boot logo
2. After 2 seconds, show main screen
3. Touch screen should beep
4. Coordinates should update

## Troubleshooting

### Plugin not loading

- Check `TFT_ENABLE` is defined in `my_machine.h`
- Check `3rdParty.cmake` exists in the project root and includes `main/tft/tft_plugin.cmake`
- If you have your own `main/my_plugin.c`, make sure it calls `tft_plugin_init()`
- Verify submodule is initialized: `git submodule status`

### Touch or buttons not doing anything

Add `#define TFT_DEBUG 1` to `my_machine.h`. Touches, button clicks and commands are then printed on the serial port as `[MSG:TFT ...]` lines, for example:

```
[MSG:TFT touch down raw x=176 y=283]
[MSG:TFT btn unlock clicked]
[MSG:TFT cmd queued: $X]
[MSG:TFT cmd accepted: $X]
```

grblHAL runs commands from the screen silently (no `ok`/`error` reply). A command it cannot take within 1 second (busy, or in Hold) is dropped and logged as `cmd expired`.

### Display shows garbage

- Check SPI pins in board map file
- Verify TFT_WIDTH and TFT_HEIGHT
- Check TFT_ROTATION setting

### Touch not responding

- Verify touch controller pins
- Check touch calibration
- Enable debug output

### Touches land off target

Send `$TFTCAL` (machine idle or in alarm). Hold each red cross until it turns green: four corners, then the center as a check. The calibration is saved only if the center tap lands within 20 px, and is kept in the ESP32 NVS namespace `tft`, so `$RST` does not clear it. The screen gives up after 30 s without a touch.

With `TFT_DEBUG` on, the result is logged as `[MSG:TFT cal x 3930..196 y 160..3875 swap 1 err 3]`. To make it the default for fresh boards, set `TFT_TOUCH_CAL_X0`, `_X1`, `_Y0`, `_Y1` and `_SWAP` in `my_machine.h`.

## License

GPL v3 - same as grblHAL

## Credits

- Based on MKS DLC32 firmware TFT integration
- Uses LVGL graphics library
- Uses TFT_eSPI display driver
- Part of grblHAL ecosystem
