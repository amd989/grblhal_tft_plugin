/*
 * tft_plugin.h - TFT touchscreen UI plugin for grblHAL
 *
 * Part of grblHAL
 *
 * Copyright (c) 2025
 *
 */

#ifndef _TFT_PLUGIN_H_
#define _TFT_PLUGIN_H_

#define TFT_PLUGIN_VERSION "0.2"    // reported as [PLUGIN:TFT UI v...]

#ifdef __cplusplus
extern "C" {
#endif

// Plugin initialization - called by grblHAL
void tft_plugin_init(void);

#ifdef __cplusplus
}
#endif

#endif // _TFT_PLUGIN_H_
