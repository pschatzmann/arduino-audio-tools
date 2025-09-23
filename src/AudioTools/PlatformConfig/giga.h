// DRAFT Support - Not tested !

#pragma once

#include <Arduino_AdvancedAnalog.h>

#define IS_MBED
#define USE_INT24_FROM_INT
#define USE_TYPETRAITS
#define USE_ANALOG
#define USE_STREAM_WRITE_OVERRIDE
#ifndef ANALOG_BUFFER_SIZE
#  define ANALOG_BUFFER_SIZE 1024
#endif
#define ANALOG_BUFFERS 10
#define USE_URL_ARDUINO
#define USE_AUDIO_SERVER

#define PIN_ANALOG_START A7
#define PIN_DAC_1 A12
#define PIN_DAC_2 A13