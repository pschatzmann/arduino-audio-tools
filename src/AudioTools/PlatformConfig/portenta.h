// DRAFT: not tested

#pragma once

#include <Arduino_AdvancedAnalog.h>

#define IS_MBED
#define USE_INT24_FROM_INT
#define USE_TYPETRAITS
#define USE_ANALOG
#define USE_TIMER
#define USE_PWM
#define USE_STREAM_WRITE_OVERRIDE
#ifndef ANALOG_BUFFER_SIZE
#  define ANALOG_BUFFER_SIZE 1024
#endif
#define ANALOG_BUFFERS 10
#define USE_URL_ARDUINO
#define USE_AUDIO_SERVER

#define PIN_ANALOG_START A0
#define PIN_PWM_START D2
#define PIN_DAC_1 D0
#define PIN_DAC_2 D1
