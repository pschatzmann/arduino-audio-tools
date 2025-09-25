#pragma once

//-------ESP32---------
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32)
// the regular ESP32
#  define ESP32_CORE
#endif
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32S2)
#  define ESP32S2
#  define ESP32X
#endif
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32S3)
#  define ESP32S3
#  define ESP32X
#  define USE_TDM
#  define USE_PDM
#  define USE_PDM_RX
#endif
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32C3)
#  define ESP32C3
#  define ESP32X
#  define USE_INT24_FROM_INT
#  define USE_TDM
#  define USE_PDM
#endif
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32C5)
#  define ESP32C5
#  define ESP32X
#  define USE_TDM
#  define USE_PDM
#endif
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32C6)
#  define ESP32C6
#  define ESP32X
#  define USE_TDM
#  define USE_PDM
#endif
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32P4)
#  define ESP32P4
#  define ESP32X
#  define USE_TDM
#  define USE_PDM
#  define USE_PDM_RX
#endif
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32H2)
#  define ESP32H2
#endif

//-------I2S Version -----------------------------------------------
#ifndef USE_LEGACY_I2S
#  define USE_LEGACY_I2S (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
#endif

//-------Config for ESP32 families ---------
#if defined(ESP32)
#  define USE_STRTOD
// We need to use accept instead of available
#  if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) 
#    define USE_SERVER_ACCEPT true              
#  endif
#  if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0) 
#    define USE_CONCURRENCY
//   Print has been fixed 
#    define USE_PRINT_FLUSH true
#  else 
#    define USE_PRINT_FLUSH false
#  endif
#  define USE_SD_SUPPORTS_SPI
//#  define USE_ESP32_LOGGER true
#  if !defined(ARDUINO)
#    define USE_IDF_LOGGER
#  endif
#  if !defined(I2S_USE_APLL)
#    define I2S_USE_APLL false
#  endif
// use ESP_DSP library for ouput mixing 
//#  define USE_ESP32_DSP
#endif

// ----- Regular ESP32 -----
#if defined(ESP32_CORE)

#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0 , 0)
#  define USE_INT24_FROM_INT
#  define USE_PDM
#endif

#define USE_ANALOG
#define USE_I2S
#define USE_PDM_RX

#ifdef ARDUINO
#  define USE_PWM
#  define USE_WIFI
#  define USE_WIFI_CLIENT_SECURE
#  define USE_URL_ARDUINO
#  define USE_AUDIO_SERVER
#  define USE_TIMER
#  define USE_TOUCH_READ
#endif

#define USE_TYPETRAITS
#define USE_STREAM_WRITE_OVERRIDE
#define USE_STREAM_READ_OVERRIDE
#define USE_EXT_BUTTON_LOGIC
// support for psram -> set to true
#define USE_ALLOCATOR true
#define HAS_IOSTRAM
#define USE_TASK false

#define PWM_FREQENCY 30000
#define PIN_PWM_START 12
#define PIN_I2S_BCK 14
#define PIN_I2S_WS 15
#define PIN_I2S_DATA_IN 32
#define PIN_I2S_DATA_OUT 22
#define PIN_I2S_MCK -1
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 23). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#define PIN_ADC1 34 

#define I2S_AUTO_CLEAR true

// URLStream
#define URL_STREAM_CORE 0
#define URL_STREAM_PRIORITY 2
#define URL_STREAM_BUFFER_COUNT 10
#define STACK_SIZE 30000
#define URL_CLIENT_TIMEOUT 60000;
#define URL_HANDSHAKE_TIMEOUT 120000

// // Default LED
// #ifndef LED_BUILTIN
// # define LED_BUILTIN 13 // pin number is specific to your esp32 board
// #endif

// support for old idf releases
#if ESP_IDF_VERSION_MAJOR < 4 && !defined(I2S_COMM_FORMAT_STAND_I2S)
# define I2S_COMM_FORMAT_STAND_I2S (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB)
# define I2S_COMM_FORMAT_STAND_MSB (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB)
# define I2S_COMM_FORMAT_STAND_PCM_LONG (I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_PCM_LONG)
# define I2S_COMM_FORMAT_STAND_PCM_SHORT (I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_PCM_SHORT)

typedef int eps32_i2s_sample_rate_type;
#else
typedef uint32_t eps32_i2s_sample_rate_type;
#endif

#endif

//-------ESP32C3, ESP32S3, ESP32S2---------

#if defined(ESP32X) 
# ifdef ARDUINO
#  include "esp32-hal-log.h"
# endif
# if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0 , 0)
#  define USE_INT24_FROM_INT
#  define USE_ANALOG
# endif

#define USE_PWM
#define USE_URL_ARDUINO
#define USE_WIFI
#define USE_WIFI_CLIENT_SECURE
#define USE_I2S
#define USE_AUDIO_SERVER
#define USE_TYPETRAITS
#define USE_TIMER
#define USE_STREAM_WRITE_OVERRIDE
#define USE_STREAM_READ_OVERRIDE
// support for psram -> set to true
#define USE_ALLOCATOR true
//#define USE_INITIALIZER_LIST

#define PWM_FREQENCY 30000
#define PIN_PWM_START 1
#define PIN_I2S_MCK -1
#define PIN_I2S_BCK 6
#define PIN_I2S_WS 7
#define PIN_I2S_DATA_OUT 8
#define PIN_I2S_DATA_IN 9
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 5). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#define PIN_ADC1 21 

#define I2S_AUTO_CLEAR true

// URLStream
//#define USE_ESP8266_AUDIO
#define URL_STREAM_CORE 0
#define URL_STREAM_PRIORITY 2
#define URL_STREAM_BUFFER_COUNT 10
#define STACK_SIZE 30000
#define URL_CLIENT_TIMEOUT 60000;
#define URL_HANDSHAKE_TIMEOUT 120000

// // Default LED
// #ifndef LED_BUILTIN
// # define LED_BUILTIN 13 // pin number is specific to your esp32 board
// #endif

typedef uint32_t eps32_i2s_sample_rate_type;

#endif

//-------ESP32H2---------

#if defined(ESP32H2) 
#include "esp32-hal-log.h"
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0 , 0)
#  define USE_INT24_FROM_INT
#  define USE_ANALOG
#endif

#define ESP32H2
#define USE_TDM
#define USE_PWM
#define USE_I2S
#define USE_PDM
#define USE_TYPETRAITS
#define USE_TIMER
#define USE_STREAM_WRITE_OVERRIDE
#define USE_STREAM_READ_OVERRIDE
// support for psram -> set to true
#define USE_ALLOCATOR true
//#define USE_INITIALIZER_LIST

#define PWM_FREQENCY 30000
#define PIN_PWM_START 1
#define PIN_I2S_MCK -1
#define PIN_I2S_BCK 6
#define PIN_I2S_WS 7
#define PIN_I2S_DATA_OUT 8
#define PIN_I2S_DATA_IN 9
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 5). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#define PIN_ADC1 21 
#define I2S_AUTO_CLEAR true

typedef uint32_t eps32_i2s_sample_rate_type;

#endif
