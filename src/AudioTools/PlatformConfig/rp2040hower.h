#pragma once

#define RP2040_HOWER
#define USE_SD_NO_NS
#define USE_I2S 
#define USE_PWM
#define USE_ANALOG_ARDUINO
#define USE_TYPETRAITS
#define USE_TIMER
#define USE_INT24_FROM_INT

#define PIN_ANALOG_START 26
#define PIN_PWM_START 6
#define PIN_I2S_BCK 26
#define PIN_I2S_WS PIN_I2S_BCK+1
#define PIN_I2S_DATA_IN 28
#define PIN_I2S_DATA_OUT 28
#define PIN_I2S_MCK -1 
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS PIN_SPI0_SS
#define USE_SERVER_ACCEPT true

// fix missing __sync_synchronize symbol
//#define FIX_SYNC_SYNCHRONIZE
#define IRAM_ATTR

#ifndef ANALOG_BUFFER_SIZE 
#define ANALOG_BUFFER_SIZE 256
#endif

#ifndef ANALOG_BUFFERS 
#define ANALOG_BUFFERS 100
#endif

//#define USE_CONCURRENCY
#define USE_SD_SUPPORTS_SPI

// default pins for VS1053 shield
#define VS1053_CS 17 
#define VS1053_DCS 9 
#define VS1053_DREQ 10 
#define VS1053_CS_SD -1
#define VS1053_RESET 11
#define VS1053_DEFINED

// The Pico W has WIFI support: but platformio is messing up, so we support NO_WIFI
#if (LWIP_IPV4==1 || LWIP_IPV6==1) &&!defined(NO_WIFI)
#  include <WiFi.h>
#  define USE_WIFI
#  define USE_WIFI_CLIENT_SECURE
#  define USE_URL_ARDUINO
#  define USE_AUDIO_SERVER
using WiFiServerSecure = BearSSL::WiFiServerSecure;
#endif

