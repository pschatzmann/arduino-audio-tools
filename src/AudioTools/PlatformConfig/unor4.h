#pragma once

// no trace to save on memory
#define NO_TRACE
//#define LOG_NO_MSG  // around 4K less

#define USE_INT24_FROM_INT
#define IS_RENESAS 1
#define USE_TYPETRAITS
#define USE_TIMER
#define USE_PWM
#define PIN_PWM_START D2
#define PIN_PWM_COUNT 12
#define USE_ANALOG
#define USE_ANALOG_ARDUINO
#define USE_SD_NO_NS
#define PIN_ANALOG_START A0
#define ANALOG_BUFFER_SIZE 512
#define ANALOG_BUFFERS 5
#define ANALOG_MAX_OUT_CHANNELS 1
#define ANALOG_MAX_SAMPLE_RATE 16000
// default pins for UNO VS1053 shield
#define VS1053_CS 6 
#define VS1053_DCS 7 
#define VS1053_DREQ 2 
#define VS1053_CS_SD 9
#define VS1053_RESET 8
#define VS1053_DEFINED
#define PIN_CS 9

#if defined(ARDUINO) && !defined(ARDUINO_MINIMA)
#  define USE_WIFI
#  define USE_URL_ARDUINO
#  define USE_AUDIO_SERVER
#  define USE_WIFIS3
#endif
