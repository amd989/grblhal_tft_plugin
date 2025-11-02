/*
 * Arduino.h - Minimal Arduino compatibility for TFT_eSPI on ESP-IDF
 *
 * This header allows TFT_eSPI library to compile under pure ESP-IDF framework.
 */

#ifndef _ARDUINO_H_
#define _ARDUINO_H_

#include <string.h>  // memset, memcpy, etc.
#include <math.h>    // sin, cos, round, etc.
#include "arduino_compat.h"

// Arduino utility macros
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

#endif // _ARDUINO_H_
