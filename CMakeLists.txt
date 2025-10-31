# TFT Touchscreen Plugin for grblHAL
add_library(tft INTERFACE)

target_sources(tft INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/tft_plugin.c
    ${CMAKE_CURRENT_LIST_DIR}/tft_interface.c
    # Add more files as you create them:
    # ${CMAKE_CURRENT_LIST_DIR}/tft_driver.c
    # ${CMAKE_CURRENT_LIST_DIR}/lvgl_init.c
    # ${CMAKE_CURRENT_LIST_DIR}/tft_task.c
    # ${CMAKE_CURRENT_LIST_DIR}/screens/screen_ready.c
    # ${CMAKE_CURRENT_LIST_DIR}/screens/screen_control.c
)

target_include_directories(tft INTERFACE ${CMAKE_CURRENT_LIST_DIR})
