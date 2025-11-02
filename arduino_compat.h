/*
 * arduino_compat.h - Minimal Arduino compatibility layer for TFT_eSPI on ESP-IDF
 *
 * Provides Arduino API shims for ESP-IDF framework to allow TFT_eSPI to compile.
 */

#ifndef _ARDUINO_COMPAT_H_
#define _ARDUINO_COMPAT_H_

#ifdef __cplusplus
extern "C" {
#endif

// ESP-IDF includes (avoid FreeRTOS headers to prevent C++ conflicts)
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_system.h"

// Pin modes (Arduino compatibility)
#define INPUT           GPIO_MODE_INPUT
#define OUTPUT          GPIO_MODE_OUTPUT
#define INPUT_PULLUP    GPIO_MODE_INPUT

// Pin levels
#define LOW             0
#define HIGH            1

// Arduino types
#define boolean         bool

// Pin macros (ESP32 doesn't use these)
#define digitalPinToBitMask(p) (1UL << (p))
#define digitalPinToPort(p) (0)
#define portOutputRegister(p) ((volatile uint8_t *)(0))

// PROGMEM macros (ESP32 doesn't need special handling for flash access)
#define PROGMEM
#define pgm_read_byte(addr)   (*(const unsigned char *)(addr))
#define pgm_read_word(addr)   (*(const unsigned short *)(addr))
#define pgm_read_dword(addr)  (*(const unsigned long *)(addr))
#define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))

// Forward declare FreeRTOS functions without including headers
// We'll rely on C linkage to resolve these at link time
extern unsigned long xTaskGetTickCount(void);
extern void vTaskDelay(unsigned long);

// FreeRTOS tick period (typically 1ms for ESP32)
#define portTICK_PERIOD_MS_COMPAT 1

// Basic Arduino functions
static inline void yield(void) {
    vTaskDelay(0);
}

static inline unsigned long millis(void) {
    return (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS_COMPAT);
}

static inline void delay(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS_COMPAT);
}

static inline void delayMicroseconds(uint32_t us) {
    extern void ets_delay_us(uint32_t);
    ets_delay_us(us);
}

static inline void pinMode(uint8_t pin, uint8_t mode) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.mode = (gpio_mode_t)mode;
    io_conf.pull_up_en = (mode == INPUT_PULLUP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
}

static inline void digitalWrite(uint8_t pin, uint8_t val) {
    gpio_set_level((gpio_num_t)pin, val);
}

static inline int digitalRead(uint8_t pin) {
    return gpio_get_level((gpio_num_t)pin);
}

#ifdef __cplusplus
}

// C++ only declarations (String, Print, SPI classes)
#include <stddef.h>

// Random function (ESP-IDF compatible) - C++ only to avoid conflicts
static inline long random(long howbig) {
    if (howbig == 0) return 0;
    return esp_random() % howbig;
}

static inline long random(long howsmall, long howbig) {
    if (howsmall >= howbig) return howsmall;
    long diff = howbig - howsmall;
    return random(diff) + howsmall;
}

// Minimal Print class (required by TFT_eSPI)
class Print {
public:
    virtual size_t write(uint8_t) { return 0; }
    size_t write(const char *str) { return 0; }
    size_t print(const char *str) { return 0; }
    size_t println(const char *str = "") { return 0; }
};

// Minimal String class
class String {
public:
    String(const char* str = "") {}
    String(int val) {}
    const char* c_str() const { return ""; }
    int length() const { return 0; }
    bool operator==(const char* str) const { return false; }
    bool operator!=(const char* str) const { return true; }
    void toCharArray(char* buf, unsigned int bufsize, unsigned int index = 0) const {
        if (buf && bufsize > 0) buf[0] = '\0';
    }
};

// ltoa function (not standard in C, Arduino compatibility)
static inline char* ltoa(long value, char* result, int base) {
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result, tmp_char;
    long tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + (tmp_value - value * base)];
    } while (value);

    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

// Arduino PWM functions (using ESP-IDF LEDC driver)
#include "driver/ledc.h"

static inline void ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolution_bits) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)resolution_bits,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = freq,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
}

static inline void ledcAttachPin(uint8_t pin, uint8_t channel) {
    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
}

static inline void ledcWrite(uint8_t channel, uint32_t duty) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

// SPI class stub (TFT_eSPI will use its own SPI handling)
class SPIClass {
public:
    void begin() {}
    void beginTransaction(void*) {}
    void endTransaction() {}
    uint8_t transfer(uint8_t data) { return 0; }
    uint16_t transfer16(uint16_t data) { return 0; }
    void setFrequency(uint32_t freq) {}
};

extern SPIClass SPI;

#endif // __cplusplus

#endif // _ARDUINO_COMPAT_H_
