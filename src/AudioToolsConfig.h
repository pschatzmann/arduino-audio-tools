/**
 * @author Phil Schatzmann
 * @brief AutioTools Configuration
 * @copyright GPLv3
 * 
 */
#pragma once

#define AUDIOTOOLS_VERSION "1.1.3"
#define AUDIOTOOLS_MAJOR_VERSION 1
#define AUDIOTOOLS_MINOR_VERSION 1

// Setup for desktop builds
#include "AudioTools/PlatformConfig/desktop.h"

// Some top level functions: stop(), checkMemory()
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

 #ifdef ESP32
 #  include "AudioTools/PlatformConfig/esp32.h"
 #endif

//----- ESP8266 -----------
#ifdef ESP8266
#  include "AudioTools/PlatformConfig/esp8266.h"
#endif

//------ nRF52840: e.g. NANO33BLE ----------
#if (defined(ARDUINO_SEEED_XIAO_NRF52840_SENSE) || defined(ARDUINO_ARDUINO_NANO33BLE) ) && !defined(ARDUINO_ARCH_ZEPHYR)
#  include "AudioTools/PlatformConfig/nrf52840.h"
#endif

//----- RP2040 MBED -----------
#if defined(ARDUINO_ARCH_MBED_RP2040) 
// install https://github.com/pschatzmann/rp2040-i2s
#  include "AudioTools/PlatformConfig/rp2040mbed.h"
#endif

//----- RP2040 -----------
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
#  include "AudioTools/PlatformConfig/rp2040hower.h"
#endif

//----- AVR -----------
#ifdef __AVR__
#  include "AudioTools/PlatformConfig/avr.h"
#endif

//---- STM32 ------------
#if defined(ARDUINO_ARCH_STM32) || defined(STM32)
#  include "AudioTools/PlatformConfig/stm32.h"
#endif

//---- SAMD ------------
#ifdef ARDUINO_ARCH_SAMD
#  include "AudioTools/PlatformConfig/samd.h"
#endif

//---- GIGA ------------
// DRAFT: Not tested !
#if defined(ARDUINO_GIGA) 
#  include "AudioTools/PlatformConfig/giga.h"
#endif

//---- Portenta ------------
// DRAFT: not tested
#if defined(ARDUINO_ARCH_MBED_PORTENTA)
#  include "AudioTools/PlatformConfig/portenta.h"
#endif

//------ RENESAS ----------
// Arduino UNO R4
#if defined(ARDUINO_ARCH_RENESAS) || defined(_RENESAS_RA_) 
#  include "AudioTools/PlatformConfig/unor4.h"
#endif

// ------ Zephyr -------
#ifdef ARDUINO_ARCH_ZEPHYR
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

