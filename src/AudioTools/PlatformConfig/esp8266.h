#pragma once

#include <ESP8266WiFi.h>
//#define USE_URL_ARDUINO // commented out because of compile errors
#define USE_I2S
#define USE_TYPETRAITS
#define USE_TIMER
#define USE_WIFI
#define USE_AUDIO_SERVER
#define USE_URL_ARDUINO

#define PIN_PWM_START 12
#define PIN_I2S_BCK -1
#define PIN_I2S_WS -1
#define PIN_I2S_DATA_IN -1
#define PIN_I2S_DATA_OUT -1
#define I2S_USE_APLL false  
#define PIN_I2S_MUTE 23
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#define USE_SERVER_ACCEPT 1

#define URL_CLIENT_TIMEOUT 60000;
#define URL_HANDSHAKE_TIMEOUT 120000
#define USE_SD_SUPPORTS_SPI
