#pragma once

#define USE_SD_NO_NS
#define USE_PWM
#define USE_TIMER
#define NO_INPLACE_INIT_SUPPORT
#define USE_ETHERNET
#define USE_URL_ARDUINO
#ifndef assert
#  define assert(T)
#endif

#define PIN_PWM_START 6
#define PIN_CS SS

#undef PWM_BUFFER_SIZE
#define PWM_BUFFER_SIZE 125

#undef DEFAULT_BUFFER_SIZE
#define DEFAULT_BUFFER_SIZE 125

// logging is using too much memory
#undef USE_AUDIO_LOGGING
#undef LOG_PRINTF_BUFFER_SIZE 
#define LOG_PRINTF_BUFFER_SIZE 60
#define NO_TRACED
#define NO_TRACEI
#define NO_TRACEE

// we use  spi to emulate i2s
#define PIN_I2S_BCK 13
#define PIN_I2S_WS 10 
#define PIN_I2S_DATA_IN 12
#define PIN_I2S_DATA_OUT 11
#define PIN_I2S_MUTE -1

