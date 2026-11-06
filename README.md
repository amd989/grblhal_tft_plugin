# grblHAL TFT Touchscreen Plugin

A touchscreen user interface plugin for grblHAL using LVGL and TFT_eSPI.

## Features

- Full touchscreen interface for CNC/Laser control
- Real-time position and status display
- Manual jogging controls
- Job running with progress display
- WiFi configuration
- Settings management
- Multi-language support

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

### Step 3: Enable Plugin

Edit `main/my_machine.h`:

```c
#define BOARD_MKS_DLC32_V2P0
#define TFT_ENABLE 1
```

### Step 4: Build

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

## Architecture

```
main/tft/
├── tft_plugin.c          # Plugin entry point and event handlers
├── tft_my_plugin.c       # my_plugin_init() hook, calls tft_plugin_init()
├── tft_plugin.cmake      # Build glue, included from 3rdParty.cmake
├── tft_config.h          # Pins and settings (defaults, override in my_machine.h)
├── tft_interface.c/.h    # Communication with grblHAL
├── tft_driver.cpp/.h     # TFT_eSPI wrapper
├── lvgl_init.cpp/.h      # LVGL initialization
├── ui_state.c/.h         # Cached machine state for the UI
├── ui_screen_manager.c/.h# Screen registration and switching
├── ui_widgets.c/.h       # Reusable LVGL widgets
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
5. Should see: `[PLUGIN:TFT UI v0.1]`

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

### Display shows garbage

- Check SPI pins in board map file
- Verify TFT_WIDTH and TFT_HEIGHT
- Check TFT_ROTATION setting

### Touch not responding

- Verify touch controller pins
- Check touch calibration
- Enable debug output

## License

GPL v3 - same as grblHAL

## Credits

- Based on MKS DLC32 firmware TFT integration
- Uses LVGL graphics library
- Uses TFT_eSPI display driver
- Part of grblHAL ecosystem
