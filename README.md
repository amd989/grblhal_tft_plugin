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

### Step 2: Enable Plugin

Edit `grblhal/main/my_machine.h` and add:

```c
// Enable TFT plugin
#define TFT_ENABLE 1

// TFT Configuration
#define TFT_DRIVER ST7796
#define TFT_WIDTH 480
#define TFT_HEIGHT 320
```

### Step 3: Build

```bash
cd grblhal
idf.py build
# or
pio run
```

## Configuration

All configuration is done in `my_machine.h`:

```c
// Display settings
#define TFT_ENABLE 1
#define TFT_WIDTH 480
#define TFT_HEIGHT 320
#define TFT_ROTATION 1  // 0-3

// LVGL settings
#define LVGL_BUFFER_SIZE (10 * 480)
#define LVGL_REFRESH_PERIOD 5  // ms

// Features
#define TFT_BEEP_ENABLE 1
#define TFT_LED_ENABLE 1
#define TFT_LANGUAGE_DEFAULT 1  // 0=Chinese, 1=English, 2=German
```

## Architecture

```
main/tft/
├── tft_plugin.c          # Plugin entry point and event handlers
├── tft_plugin.h          # Plugin API
├── tft_interface.c       # Communication with grblHAL
├── tft_interface.h
├── tft_task.c            # FreeRTOS UI task
├── tft_task.h
├── tft_driver.c          # TFT_eSPI wrapper
├── tft_driver.h
├── lvgl_init.c           # LVGL initialization
├── lvgl_init.h
├── screens/              # UI screens
│   ├── screen_ready.c
│   ├── screen_control.c
│   ├── screen_job.c
│   ├── screen_settings.c
│   └── screen_wifi.c
├── CMakeLists.txt        # Build configuration
└── README.md
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
2. Add to CMakeLists.txt
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
- Check CMakeLists.txt includes tft directory
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
