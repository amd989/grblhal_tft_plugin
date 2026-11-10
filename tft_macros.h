/*
 * tft_macros.h - Macro buttons, stored as grblHAL settings $450-$455
 *
 * Part of grblHAL TFT Plugin
 */

#ifndef _TFT_MACROS_H_
#define _TFT_MACROS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFT_N_MACROS    6
#define TFT_MACRO_LEN   80      // "Label|line|line..."

// Register the settings, from plugin init
void tft_macros_init(void);

// UI task: label of macro idx (0-based), false if the macro is empty
bool tft_macro_label(uint_fast8_t idx, char *buf, size_t size);

// UI task: queue the lines of macro idx, false if it has none
bool tft_macro_run(uint_fast8_t idx);

#ifdef __cplusplus
}
#endif

#endif // _TFT_MACROS_H_
