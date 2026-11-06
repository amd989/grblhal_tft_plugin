/*
 * tft_my_plugin.c - grblHAL my_plugin_init() hook for the TFT plugin
 *
 * Part of grblHAL TFT Plugin
 *
 * tft_plugin.cmake compiles this file in place of grbl/my_plugin.c, so grblHAL
 * starts the plugin without any edits to the core. If you have your own
 * main/my_plugin.c, this file is skipped: call tft_plugin_init() from your
 * my_plugin_init() instead.
 */

#include "driver.h"

#if TFT_ENABLE
extern void tft_plugin_init (void);
#endif

void my_plugin_init (void)
{
#if TFT_ENABLE
    tft_plugin_init();
#endif
}
