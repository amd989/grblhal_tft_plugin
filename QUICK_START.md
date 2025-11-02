# TFT Plugin - Quick Start Checklist

Follow these steps IN ORDER to get your plugin working.

## ☐ Step 1: Set Up CMakeLists.txt

⚠️ **IMPORTANT:** There's already a CMakeLists.txt in this directory. You need to replace it.

```bash
cd grblhal/main/tft
# Backup existing file
mv CMakeLists.txt CMakeLists.txt.backup
# Use corrected version
mv CORRECTED_CMakeLists.txt CMakeLists.txt
```

The file should look like this (matching other grblHAL plugins):

```cmake
add_library(tft INTERFACE)

target_sources(tft INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/tft_plugin.c
    ${CMAKE_CURRENT_LIST_DIR}/tft_interface.c
)

target_include_directories(tft INTERFACE ${CMAKE_CURRENT_LIST_DIR})
```

**Note:** This uses the INTERFACE library pattern like all other grblHAL plugins (keypad, laser, etc.), NOT the ESP-IDF component pattern.

## ☐ Step 2: Enable Plugin in my_machine.h

Edit `grblhal/main/my_machine.h`:

```c
// Near the top, after board selection:
#define TFT_ENABLE 1
```

## ☐ Step 3: Build

```bash
cd grblhal
idf.py build
```

Or with PlatformIO:

```bash
pio run
```

**Expected:** No errors, plugin compiles successfully.

## ☐ Step 4: Flash and Test

```bash
idf.py flash monitor
```

Or:

```bash
pio run -t upload && pio device monitor
```

**Expected Output:**

```
[MSG:Grbl 1.1f ['$' for help]]
[TFT Plugin initialized]         <-- SUCCESS!
```

## ☐ Step 5: Verify Plugin Registration

In serial monitor, send:

```
$I
```

**Expected Output:**

```
[VER:1.1f.20250101:grblHAL]
[OPT:VHM,35,512,3,0]
[PLUGIN:TFT UI v0.1]             <-- SUCCESS!
[NEWOPT:ETH,RT+,HOME,TC,LATHE...]
ok
```

## ☐ Step 6: Test Event Handlers

### Test State Change:

```
$H
```

The plugin's `tft_state_changed()` function is called with `STATE_HOMING`.

### Test Position Updates:

```
?
```

The plugin's `tft_realtime_report()` function is called with position data.

## ✅ Plugin is Working!

If you reached here without errors, your plugin infrastructure is working!

---

## Next: Add Display Support

Now that the plugin loads, add the display:

### Option 1: Test with Simple Text (Quick Test)

Create `tft_driver.c`:

```c
#include "driver.h"
#if TFT_ENABLE

// TODO: Add TFT_eSPI includes
void tft_driver_init(void) {
    // TODO: Initialize TFT
    hal.stream.write("[TFT Driver initialized]" ASCII_EOL);
}

#endif
```

Update `tft_plugin.c`:

```c
// Add at top
extern void tft_driver_init(void);

// In tft_plugin_init():
void tft_plugin_init(void) {
    tft_driver_init();  // Add this line
    // ... rest of code ...
}
```

Update `CMakeLists.txt`:

```cmake
add_library(tft INTERFACE)

target_sources(tft INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/tft_plugin.c
    ${CMAKE_CURRENT_LIST_DIR}/tft_interface.c
    ${CMAKE_CURRENT_LIST_DIR}/tft_driver.c  # Add this
)

target_include_directories(tft INTERFACE ${CMAKE_CURRENT_LIST_DIR})
```

Build and flash. You should see:
```
[TFT Plugin initialized]
[TFT Driver initialized]
```

### Option 2: Add Full TFT_eSPI (Recommended)

See `GRBLHAL_PLUGIN_SETUP_GUIDE.md` Phase 1 for complete instructions.

---

## Troubleshooting

### ❌ Build Error: "No such file or directory: CMakeLists.txt"

**Fix:** Create `grblhal/main/tft/CMakeLists.txt` (see Step 1)

### ❌ Build Error: "TFT_ENABLE undeclared"

**Fix:** Add `#define TFT_ENABLE 1` to `my_machine.h` (see Step 2)

### ❌ Plugin doesn't print initialization message

**Causes:**
1. `TFT_ENABLE` not defined
2. Plugin not compiled into build
3. `my_plugin_init()` not being called

**Fix:** Verify `#if TFT_ENABLE` in `tft_plugin.c` and rebuild

### ❌ Multiple definition errors

**Fix:** Make sure ALL code is wrapped in `#if TFT_ENABLE ... #endif`

---

## Success Criteria

- ✅ Build completes without errors
- ✅ `[TFT Plugin initialized]` appears on boot
- ✅ `[PLUGIN:TFT UI v0.1]` appears in `$I` output
- ✅ No crashes or resets

## What You Have Now

```
grblhal/main/tft/
├── tft_plugin.c          ✅ Plugin entry point
├── tft_plugin.h          ✅ API
├── tft_interface.c       ✅ Command interface
├── tft_interface.h       ✅ Interface API
├── CMakeLists.txt        ✅ Build config
├── README.md             ✅ Documentation
├── QUICK_START.md        📖 This file
└── PLUGIN_CMakeLists.txt ℹ️  Template
```

## Ready for Display Code!

Once the above steps work, you're ready to add:
1. TFT driver (TFT_eSPI)
2. LVGL graphics
3. UI task
4. Screens from MKS firmware

Refer to `GRBLHAL_PLUGIN_SETUP_GUIDE.md` for detailed instructions on each phase.

---

**Having Issues?** Check:
1. All files have `#if TFT_ENABLE` guards
2. CMakeLists.txt exists and is valid
3. `my_machine.h` has `#define TFT_ENABLE 1`
4. Build output shows tft_plugin.c being compiled
5. Serial output shows initialization message
