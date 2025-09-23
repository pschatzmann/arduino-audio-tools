#pragma once

#ifndef STM32
#  define STM32
#endif

#define USE_I2S
#define USE_PWM
#define USE_TIMER
#define USE_ANALOG
#define USE_ANALOG_ARDUINO
#define USE_INT24_FROM_INT

#define PIN_ANALOG_START PA0
#define PIN_PWM_START PA0
#define PWM_DEFAULT_TIMER TIM2
#define PWM_FREQ_TIMER_NO 3
#define USE_SD_NO_NS

#define PIN_I2S_BCK -1
#define PIN_I2S_WS -1
#define PIN_I2S_DATA_IN -1
#define PIN_I2S_DATA_OUT -1
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS -1

#define USE_ETHERNET
#define USE_URL_ARDUINO
#define USE_AUDIO_SERVER

