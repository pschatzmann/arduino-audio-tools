/**
 * @author Phil Schatzmann
 * @brief AutioTools Configuration
 * @copyright GPLv3
 * 
 */
#pragma once

#define AUDIOTOOLS_VERSION "1.0.1"
#define AUDIOTOOLS_MAJOR_VERSION 1
#define AUDIOTOOLS_MIOR_VERSION 0


#if defined(IS_MIN_DESKTOP) 
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#  include "AudioTools/AudioLibs/Desktop/Time.h"
#  include "AudioTools/AudioLibs/Desktop/Main.h"
#  include "AudioTools/AudioLibs/Desktop/File.h"
#  define USE_STREAM_READ_OVERRIDE
#  ifndef EXIT_ON_STOP
#    define EXIT_ON_STOP
#  endif
#elif defined(IS_DESKTOP_WITH_TIME_ONLY)
#  include "AudioTools/AudioLibs/Desktop/Time.h"
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#  ifndef EXIT_ON_STOP
#    define EXIT_ON_STOP
#  endif
#elif defined(IS_DESKTOP)
#  include "Arduino.h"
#  include <Client.h>
#  include <WiFi.h>
#  define USE_WIFI
#  define USE_URL_ARDUINO
#  define USE_STREAM_WRITE_OVERRIDE
#  define USE_STREAM_READ_OVERRIDE
#  define USE_STREAM_READCHAR_OVERRIDE
#  ifndef EXIT_ON_STOP
#    define EXIT_ON_STOP
#  endif
//#  define USE_3BYTE_INT24
typedef WiFiClient WiFiClientSecure;
#elif defined(ARDUINO)
#  include "Arduino.h"
// --- ESP32 ------------
// E.g when using the Espressif IDF. Use cmake for the necesseary defines
#elif defined(ESP32_CMAKE)
#  define ESP32
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#else 
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#  define IS_JUPYTER
#  define USE_STREAM_READ_OVERRIDE
#endif

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "AudioTools/CoreAudio/AudioRuntime.h"


// If you don't want to use all the settings from here you can define your own local config settings in AudioConfigLocal.h
#if __has_include("AudioConfigLocal.h") 
#include "AudioConfigLocal.h"
#endif

// Automatically include all core audio functionality
#ifndef AUDIO_INCLUDE_CORE
#  define AUDIO_INCLUDE_CORE true
#endif

// Use fixed point multiplication instead float for VolumeStream for slightly better performance on platforms without float hardware. Tested on RP2040 at 16 bit per second (still too slow for 32bit)
#ifndef PREFER_FIXEDPOINT
#  define PREFER_FIXEDPOINT false 
#endif

// Add automatic using namespace audio_tools;
#ifndef USE_AUDIOTOOLS_NS
#  define USE_AUDIOTOOLS_NS true
#endif

/**
 * @brief Logging
 * Logging Configuration in Arduino -> set USE_AUDIO_LOGGING to false if you want to deactivate Logging.
 * When using cmake you can set -DUSE_AUDIO_LOGGING=false
 * You can also change the LOG_LEVEL and LOG_STREAM here.
 * However it is recommended to do it in your Sketch e.g with AudioLogger::instance().begin(Serial,AudioLogger::Warning);
 */
 
#ifndef USE_AUDIO_LOGGING 
#  define USE_AUDIO_LOGGING true
#endif

#ifndef LOG_LEVEL 
#  define LOG_LEVEL AudioLogger::Warning
#endif

#ifndef LOG_STREAM 
#  define LOG_STREAM Serial
#endif

#ifndef LOG_PRINTF_BUFFER_SIZE
#  define LOG_PRINTF_BUFFER_SIZE 303
#endif 

#ifndef LOG_METHOD
#  define LOG_METHOD __PRETTY_FUNCTION__
#endif

// cheange USE_CHECK_MEMORY to true to activate memory checks
#ifndef USE_CHECK_MEMORY
#  define USE_CHECK_MEMORY false
#endif


// Activate/deactivate obsolete functionality
#ifndef USE_OBSOLETE
#  define USE_OBSOLETE false
#endif

/**
 * @brief Common Default Settings that can usually be changed in the API
 */

#ifndef DEFAULT_BUFFER_SIZE 
#  define DEFAULT_BUFFER_SIZE 1024
#endif

#ifndef DEFAULT_SAMPLE_RATE 
#  define DEFAULT_SAMPLE_RATE 44100
#endif

#ifndef DEFAULT_CHANNELS 
#  define DEFAULT_CHANNELS 2
#endif

#ifndef DEFAULT_BITS_PER_SAMPLE 
#  define DEFAULT_BITS_PER_SAMPLE 16
#endif

#ifndef I2S_DEFAULT_PORT 
#  define I2S_DEFAULT_PORT 0
#endif

#ifndef I2S_BUFFER_SIZE 
#  define I2S_BUFFER_SIZE 512
#endif

#ifndef I2S_BUFFER_COUNT 
#  define I2S_BUFFER_COUNT 6 // 20
#endif

#ifndef ANALOG_BUFFER_SIZE 
#  define ANALOG_BUFFER_SIZE 512
#endif

#ifndef ANALOG_BUFFER_COUNT 
#  define ANALOG_BUFFER_COUNT 6 // 20
#endif

#ifndef A2DP_BUFFER_SIZE 
#  define A2DP_BUFFER_SIZE 512
#endif

#ifndef A2DP_BUFFER_COUNT 
#  define A2DP_BUFFER_COUNT 30
#endif

#ifndef CODEC_DELAY_MS 
#  define CODEC_DELAY_MS 10
#endif

#ifndef COPY_DELAY_ON_NODATA 
#  define COPY_DELAY_ON_NODATA 10
#endif

#ifndef COPY_RETRY_LIMIT 
#  define COPY_RETRY_LIMIT 20
#endif

#ifndef MAX_SINGLE_CHARS
#  define MAX_SINGLE_CHARS 8
#endif

// max size of http processing buffer
#ifndef HTTP_MAX_LEN
#  define HTTP_MAX_LEN 1024
#endif

// max size of chunked size line
#ifndef HTTP_CHUNKED_SIZE_MAX_LEN
#  define HTTP_CHUNKED_SIZE_MAX_LEN 80
#endif


#ifndef USE_RESAMPLE_BUFFER
#  define USE_RESAMPLE_BUFFER true
#endif

/**
 * @brief PWM
 */
#ifndef PWM_BUFFER_SIZE 
#  define PWM_BUFFER_SIZE 1024
#endif

#ifndef PWM_BUFFER_COUNT 
#  define PWM_BUFFER_COUNT 4
#endif

#ifndef PWM_AUDIO_FREQUENCY 
#  define PWM_AUDIO_FREQUENCY 30000
#endif

// Activate Networking for All Processors
// #define USE_ETHERNET
// #define USE_AUDIO_SERVER
// #define USE_URL_ARDUINO

/**
 * ------------------------------------------------------------------------- 
 * @brief Platform specific Settings
 */
//-------ESP32---------
#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32C3)
#  define ESP32C3
#  define ESP32X
#  define USE_INT24_FROM_INT
#  define USE_TDM
#  define USE_PDM
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

// for all ESP32 families
#if defined(ESP32)
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
#endif

// ----- Regular ESP32 -----
#if defined(ESP32)  && !defined(ESP32X) && !defined(CONFIG_IDF_TARGET_ESP32H2)
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0 , 0)
#  define USE_INT24_FROM_INT
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
#define I2S_USE_APLL true  
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

#if defined(ESP32)  && defined(ESP32X) 
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
#define I2S_USE_APLL true  
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

#if defined(ESP32)  && defined(CONFIG_IDF_TARGET_ESP32H2)
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
#define I2S_USE_APLL true  
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 5). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#define PIN_ADC1 21 
#define I2S_AUTO_CLEAR true

typedef uint32_t eps32_i2s_sample_rate_type;

#endif


//----- ESP8266 -----------
#ifdef ESP8266
#  include <ESP8266WiFi.h>
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

#endif

//------ NANO33BLE ----------
#if (defined(ARDUINO_SEEED_XIAO_NRF52840_SENSE) || defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_MBED_NANO)) && !defined(ARDUINO_ARCH_ZEPHYR)
#define USE_NANO33BLE 
#define USE_INT24_FROM_INT
#define USE_I2S
#define USE_PWM
#define USE_TYPETRAITS
#define USE_TIMER
//#define USE_INITIALIZER_LIST
#define USE_ALT_PIN_SUPPORT

#define PIN_PWM_START 5
#define PIN_I2S_BCK 2
#define PIN_I2S_WS 3
#define PIN_I2S_DATA_IN 4
#define PIN_I2S_DATA_OUT 4
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#endif

//----- RP2040 MBED -----------
#if defined(ARDUINO_ARCH_MBED_RP2040) 
// install https://github.com/pschatzmann/rp2040-i2s
#define RP2040_MBED
#define USE_I2S 1
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
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS 1 //PIN_SPI0_SS

// fix missing __sync_synchronize symbol
#define FIX_SYNC_SYNCHRONIZE
#define IRAM_ATTR
#ifndef ANALOG_BUFFER_SIZE 
#define ANALOG_BUFFER_SIZE 1024
#endif

#ifndef ANALOG_BUFFERS 
#define ANALOG_BUFFERS 50
#endif

//#define USE_ESP8266_AUDIO

//----- RP2040 -----------
#elif defined(ARDUINO_ARCH_RP2040)
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
#endif

// The Pico W has WIFI support: but platformio is messing up, so we support NO_WIFI
#if (defined(ARDUINO_NANO_RP2040_CONNECT) || defined(ARDUINO_RASPBERRY_PI_PICO_W) || defined(ARDUINO_ARCH_RP2040)) && (LWIP_IPV4==1 || LWIP_IPV6==1) &&!defined(NO_WIFI)
#  include <WiFi.h>
#  define USE_WIFI
#  define USE_WIFI_CLIENT_SECURE
#  define USE_URL_ARDUINO
#  define USE_AUDIO_SERVER
using WiFiServerSecure = BearSSL::WiFiServerSecure;
#endif


//----- AVR -----------
#ifdef __AVR__
#define USE_SD_NO_NS
#define USE_PWM
#define USE_TIMER
#define NO_INPLACE_INIT_SUPPORT
// Uncomment to activate network
//#include <Ethernet.h>
//#define USE_URL_ARDUINO
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

#endif


//---- STM32 ------------
#if defined(ARDUINO_ARCH_STM32)
#define STM32
#endif

#ifdef STM32
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


#endif

//---- SAMD ------------

#ifdef ARDUINO_ARCH_SAMD
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
#endif

#ifdef ARDUINO_SAMD_MKRWIFI1010
#include <WiFiNINA.h>
#define USE_URL_ARDUINO
#define USE_AUDIO_SERVER
#endif

//---- GIGA ------------
// DRAFT Support - Not tested !
#if defined(ARDUINO_GIGA) 
#include <WiFi.h>
#include <Arduino_AdvancedAnalog.h>
#define IS_MBED
#define USE_INT24_FROM_INT
#define USE_TYPETRAITS
#define USE_ANALOG
#define USE_STREAM_WRITE_OVERRIDE
#define ANALOG_BUFFER_SIZE 1024
#define ANALOG_BUFFERS 10
#define USE_URL_ARDUINO
#define USE_AUDIO_SERVER

#define PIN_ANALOG_START A7
#define PIN_DAC_1 A12
#define PIN_DAC_2 A13

#endif

// //---- Portenta ------------
// // DRAFT: not tested
#if defined(ARDUINO_ARCH_MBED_PORTENTA)
#include <WiFi.h>
#include <Arduino_AdvancedAnalog.h>
#define IS_MBED
#define USE_INT24_FROM_INT
#define USE_TYPETRAITS
#define USE_ANALOG
#define USE_TIMER
#define USE_PWM
#define USE_STREAM_WRITE_OVERRIDE
#define ANALOG_BUFFER_SIZE 1024
#define ANALOG_BUFFERS 10
#define USE_URL_ARDUINO
#define USE_AUDIO_SERVER

#define PIN_ANALOG_START A0
#define PIN_PWM_START D2
#define PIN_DAC_1 D0
#define PIN_DAC_2 D1
#endif

//------ RENESAS ----------
// Arduino UNO R4
#if defined(ARDUINO_ARCH_RENESAS) || defined(_RENESAS_RA_) 
// no trace to save on memory
#define NO_TRACE
#define LOG_NO_MSG  // around 4K less

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
#  include "WiFiS3.h"
#endif

#endif


// ------ Zephyr -------
#ifdef ARDUINO_ARCH_ZEPHYR
#  define IS_ZEPHYR
#  define NO_INPLACE_INIT_SUPPORT
#  define USE_TYPETRAITS
#  define USE_I2S
#  define PIN_I2S_BCK 2
#  define PIN_I2S_WS 3
#  define PIN_I2S_DATA_IN 4
#  define PIN_I2S_DATA_OUT 4
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#  define PIN_I2S_MUTE -1
#  define SOFT_MUTE_VALUE 0
#  define USE_NANO33BLE 
#  define USE_ALT_PIN_SUPPORT
#endif

//------ VS1053 ----------
// see https://github.com/pschatzmann/arduino-vs1053/wiki/Pinouts-for-Processors-and-Tested-Boards#microcontrollers
// Default Pins for VS1053
#ifndef VS1053_DEFINED
#  define VS1053_CS 5
#  define VS1053_DCS 16
#  define VS1053_DREQ 4
#  define VS1053_RESET 15  
#  define VS1053_CS_SD -1
#endif

// use 0 for https://github.com/baldram/ESP_VS1053_Library
// use 1 for https://github.com/pschatzmann/arduino-vs1053
#define VS1053_EXT 1
#define VS1053_DEFAULT_VOLUME 0.7


//----------------
// Fallback defined if nothing was defined in the platform

#ifndef ARDUINO
#  define USE_STREAM_WRITE_OVERRIDE
#endif

#ifndef ANALOG_MAX_SAMPLE_RATE
#  define ANALOG_MAX_SAMPLE_RATE 44100
#endif

#ifndef PWM_MAX_SAMPLE_RATE
#  define PWM_MAX_SAMPLE_RATE 8000
#endif


#ifndef URL_CLIENT_TIMEOUT
#  define URL_CLIENT_TIMEOUT 60000;
#  define URL_HANDSHAKE_TIMEOUT 120000
#endif

#ifndef USE_TASK
#  define USE_TASK false
#endif

#ifndef USE_SERVER_ACCEPT
#  define USE_SERVER_ACCEPT false
#endif

#ifndef USE_ALLOCATOR
#  define USE_ALLOCATOR false
#endif

#ifndef USE_ESP32_LOGGER
#  define USE_ESP32_LOGGER false
#endif

// Standard Arduino Print provides flush function
#ifndef USE_PRINT_FLUSH
#  define USE_PRINT_FLUSH true
#endif

#ifndef ESP_IDF_VERSION_VAL
#  define ESP_IDF_VERSION_VAL(a, b , c) 0
#endif

#if USE_CHECK_MEMORY
#  define CHECK_MEMORY() checkMemory(true)
#else
#  define CHECK_MEMORY() 
#endif

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wvla"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wdouble-promotion"

#ifdef USE_NO_MEMACCESS
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

#ifdef USE_INITIALIZER_LIST
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif

#undef rewind

// select int24 implementation
#include "AudioTools/CoreAudio/AudioBasic/Int24_3bytes_t.h"
#include "AudioTools/CoreAudio/AudioBasic/Int24_4bytes_t.h"
#include "AudioTools/CoreAudio/AudioBasic/FloatAudio.h"

namespace audio_tools {
    #ifdef USE_3BYTE_INT24
        using int24_t = audio_tools::int24_3bytes_t;
    #else
        using int24_t = audio_tools::int24_4bytes_t;
    #endif
}

/**
 * ------------------------------------------------------------------------- 
 * @brief Set namespace
 * 
 */
#if USE_AUDIOTOOLS_NS
using namespace audio_tools;  
#endif

