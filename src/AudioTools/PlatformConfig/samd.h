#pragma once

#define NO_INPLACE_INIT_SUPPORT
#ifndef SEEED_XIAO_M0
#  define USE_I2S
#endif
#define USE_INT24_FROM_INT
#define PIN_I2S_BCK 1
#define PIN_I2S_WS PIN_I2S_BCK+1
#define PIN_I2S_DATA_IN 3
#define PIN_I2S_DATA_OUT 3
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define USE_SD_NO_NS
#define PIN_CS 4

#ifdef ARDUINO_SAMD_MKRWIFI1010
#define USE_WIFI_NININA
#define USE_URL_ARDUINO
#define USE_AUDIO_SERVER
#endif
