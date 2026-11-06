# TFT Plugin - Quick Start

Steps for the grblHAL ESP32 driver (https://github.com/grblHAL/ESP32) on an MKS DLC32 v2.0/v2.1.

## 1. Add the plugin

```bash
cd main
git submodule add https://github.com/amd989/grblhal_tft_plugin.git tft
git submodule update --init --recursive
```

## 2. Hook it into the build

Create `3rdParty.cmake` in the project root:

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/main/tft/tft_plugin.cmake)
```

## 3. Enable it

In `main/my_machine.h`:

```c
#define BOARD_MKS_DLC32_V2P0
#define TFT_ENABLE 1
```

Pin and display settings default to the DLC32 + TS35 values in `tft_config.h`. Define any of them in `my_machine.h` to override.

## 4. Build and flash

```bash
pio run
pio run -t upload && pio device monitor
```

## 5. Check it loaded

Send `$I` in the serial monitor. The plugin should be listed in the `[PLUGIN:...]` lines.

## Troubleshooting

- **Plugin missing from `$I`:** check `TFT_ENABLE` is 1 and `3rdParty.cmake` exists. If you have your own `main/my_plugin.c`, it must call `tft_plugin_init()`.
- **`lvgl.h` or `TFT_eSPI.h` not found:** run `git submodule update --init --recursive` inside `main/tft`.
