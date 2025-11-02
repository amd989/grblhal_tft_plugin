# TFT Plugin Component CMakeLists.txt
# This makes the TFT plugin a sub-component of main

# Collect all LVGL v6 source files
file(GLOB_RECURSE LVGL_SOURCES "lib/lvgl/src/*.c")

# TFT plugin sources
set(COMPONENT_SRCS
    "tft_plugin.c"
    "tft_interface.c"
    "tft_driver.cpp"
    "lvgl_init.c"
    "lib/TFT_eSPI/TFT_eSPI.cpp"
    ${LVGL_SOURCES}
)

# Include directories
set(COMPONENT_ADD_INCLUDEDIRS
    "."
    "lib/lvgl"
    "lib/TFT_eSPI"
)

# Make this available to parent
set(TFT_SRCS ${COMPONENT_SRCS} PARENT_SCOPE)
set(TFT_INCLUDES ${COMPONENT_ADD_INCLUDEDIRS} PARENT_SCOPE)
