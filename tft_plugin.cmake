# tft_plugin.cmake - build glue for the grblHAL ESP32 driver
#
# Include from the driver's 3rdParty.cmake (in the ESP32 repo root):
#
#   include(${CMAKE_CURRENT_LIST_DIR}/main/tft/tft_plugin.cmake)
#
# main/CMakeLists.txt includes 3rdParty.cmake before it assembles SRCS and
# registers the component, so this file can add sources and set
# directory-scoped include paths and compile options.

set(TFT_PLUGIN_DIR ${CMAKE_CURRENT_LIST_DIR})

file(GLOB_RECURSE TFT_LVGL_SOURCES "${TFT_PLUGIN_DIR}/lib/lvgl/src/*.c")

list(APPEND SRCS
    ${TFT_PLUGIN_DIR}/tft_plugin.c
    ${TFT_PLUGIN_DIR}/tft_interface.c
    ${TFT_PLUGIN_DIR}/tft_driver.cpp
    ${TFT_PLUGIN_DIR}/lvgl_init.cpp
    ${TFT_PLUGIN_DIR}/arduino_spi.cpp
    ${TFT_PLUGIN_DIR}/ui_state.c
    ${TFT_PLUGIN_DIR}/ui_screen_manager.c
    ${TFT_PLUGIN_DIR}/ui_widgets.c
    ${TFT_PLUGIN_DIR}/lib/TFT_eSPI/TFT_eSPI.cpp
    ${TFT_LVGL_SOURCES}
)

# Take over the my_plugin_init() hook, unless the user has their own
# main/my_plugin.c. In that case they must call tft_plugin_init() from it.
if(MY_PLUGIN_SOURCE STREQUAL "grbl/my_plugin.c")
    set(MY_PLUGIN_SOURCE ${TFT_PLUGIN_DIR}/tft_my_plugin.c)
else()
    message(STATUS "TFT plugin: main/my_plugin.c found, call tft_plugin_init() from your my_plugin_init()")
endif()

# ESP-IDF first runs component CMake files in script mode to collect
# requirements, where directory commands are not allowed.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    include_directories(
        ${TFT_PLUGIN_DIR}
        ${TFT_PLUGIN_DIR}/lib/lvgl
        ${TFT_PLUGIN_DIR}/lib/TFT_eSPI
    )

    # ESP-IDF FLAG_ATTR macros define C++ operators that clash in TFT_eSPI.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
endif()
