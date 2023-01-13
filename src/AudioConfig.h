/**
 * @author Phil Schatzmann
 * @brief AutioTools Configuration
 * @copyright GPLv3
 * 
 */
#pragma once
#if defined(ARDUINO) || defined(IS_DESKTOP)
#  include "Arduino.h"
#else
#  include "AudioLibs/NoArduino.h"
#endif
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "AudioTools/AudioRuntime.h"

// If you don't want to use all the settings from here you can define your own local config settings in AudioConfigLocal.h
#if __has_include("AudioConfigLocal.h") 
#include "AudioConfigLocal.h"
#endif

#define AUDIOTOOLS_VERSION "0.8.0"

/**
 * ------------------------------------------------------------------------- 
 * @brief Logging
 * Logging Configuration in Arduino -> set USE_AUDIO_LOGGING to false if you want to deactivate Logging.
 * When using cmake you can set -DUSE_AUDIO_LOGGING=false
 * You can also change the LOG_LEVEL and LOG_STREAM here.
 * However it is recommended to do it in your Sketch e.g with AudioLogger::instance().begin(Serial,AudioLogger::Warning);
 */
 
#ifndef USE_AUDIO_LOGGING 
#define USE_AUDIO_LOGGING true
#endif

#ifndef LOG_LEVEL 
#define LOG_LEVEL AudioLogger::Warning
#endif

#ifndef LOG_STREAM 
#define LOG_STREAM Serial
#endif

#define LOG_PRINTF_BUFFER_SIZE 256
#define LOG_METHOD __PRETTY_FUNCTION__

// cheange USE_CHECK_MEMORY to 1 to activate memory checks
#define USE_CHECK_MEMORY 0
#if USE_CHECK_MEMORY
#  define CHECK_MEMORY() checkMemory(true)
#else
#  define CHECK_MEMORY() 
#endif

// Change USE_INLINE_VARS to 1 if inline variables are supported
#ifndef USE_INLINE_VARS
#  define USE_INLINE_VARS 0
#endif
/**
 * ------------------------------------------------------------------------- 
 * @brief Common Default Settings that can usually be changed in the API
 */

#ifndef DEFAULT_BUFFER_SIZE 
#define DEFAULT_BUFFER_SIZE 1024
#endif

#ifndef DEFAULT_SAMPLE_RATE 
#define DEFAULT_SAMPLE_RATE 44100
#endif

#ifndef DEFAULT_CHANNELS 
#define DEFAULT_CHANNELS 2
#endif

#ifndef DEFAULT_BITS_PER_SAMPLE 
#define DEFAULT_BITS_PER_SAMPLE 16
#endif

#ifndef I2S_DEFAULT_PORT 
#define I2S_DEFAULT_PORT 0
#endif

#ifndef I2S_BUFFER_SIZE 
#define I2S_BUFFER_SIZE 512
#endif

#ifndef I2S_BUFFER_COUNT 
#define I2S_BUFFER_COUNT 10 // 20
#endif

#ifndef A2DP_BUFFER_SIZE 
#define A2DP_BUFFER_SIZE 512
#endif

#ifndef A2DP_BUFFER_COUNT 
#define A2DP_BUFFER_COUNT 30
#endif

#ifndef CODEC_DELAY_MS 
#define CODEC_DELAY_MS 10
#endif

#ifndef COPY_DELAY_ON_NODATA 
#define COPY_DELAY_ON_NODATA 10
#endif

#ifndef COPY_RETRY_LIMIT 
#define COPY_RETRY_LIMIT 20
#endif

#ifndef MAX_HTTP_HEADER_LINE_LENGTH
#define MAX_HTTP_HEADER_LINE_LENGTH 240
#endif

/**
 * ------------------------------------------------------------------------- 
 * @brief PWM
 */
#ifndef PWM_BUFFER_SIZE 
#define PWM_BUFFER_SIZE 1024
#endif

#ifndef PWM_BUFFERS 
#define PWM_BUFFERS 40
#endif

#ifndef PWM_AUDIO_FREQUENCY 
#define PWM_AUDIO_FREQUENCY 30000
#endif


/**
 * ------------------------------------------------------------------------- 
 * @brief Activate Other Tools
 */

//#define USE_STK
//#define USE_SDFAT
//#define USE_DELTASIGMA 


/**
 * ------------------------------------------------------------------------- 
 * @brief Platform specific Settings
 */

//-------ESP32---------
#if defined(ESP32)  && defined(ARDUINO_ESP32C3_DEV)
#define ESP32C3
#define ESP32X
#endif
#if defined(ESP32)  && defined(ARDUINO_ESP32S2_DEV)
#define ESP32S2
#define ESP32X
#endif
#if defined(ESP32)  && defined(ARDUINO_ESP32S3_DEV)
#define ESP32S3
#define ESP32X
#endif

// ----- Regular ESP32 -----
#if defined(ESP32)  && !defined(ESP32X)
#include "esp32-hal-log.h"
// optional libraries
//#define USE_A2DP
//#define USE_ESP8266_AUDIO

#define USE_PWM
#define USE_URL_ARDUINO
#define USE_WIFI
#define USE_WIFI_CLIENT_SECURE
#define USE_I2S
#define USE_AUDIO_SERVER
#define USE_TYPETRAITS
#define USE_EFFECTS_SUITE
#define USE_TIMER
#define USE_I2S_ANALOG
#define USE_STREAM_WRITE_OVERRIDE

#define PWM_FREQENCY 30000
#define PIN_PWM_START 12
#define PIN_I2S_BCK 14
#define PIN_I2S_WS 15
#define PIN_I2S_DATA_IN 32
#define PIN_I2S_DATA_OUT 22
#define I2S_USE_APLL false  
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 23). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#define PIN_ADC1 34 
#define PIN_ADC2 14

#define I2S_AUTO_CLEAR true

// URLStream
//#define USE_URLSTREAM_TASK
#define URL_STREAM_CORE 0
#define URL_STREAM_PRIORITY 2
#define URL_STREAM_BUFFER_COUNT 10
#define STACK_SIZE 30000
#define URL_CLIENT_TIMEOUT 60000;
#define URL_HANDSHAKE_TIMEOUT 120000

// Default LED
#ifndef LED_BUILTIN
# define LED_BUILTIN 13 // pin number is specific to your esp32 board
#endif

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
#include "esp32-hal-log.h"

#define USE_PWM
#define USE_URL_ARDUINO
#define USE_WIFI
#define USE_WIFI_CLIENT_SECURE
#define USE_I2S
#define USE_AUDIO_SERVER
//#define USE_URLSTREAM_TASK
#define USE_TYPETRAITS
#define USE_EFFECTS_SUITE
#define USE_TIMER
#define USE_STREAM_WRITE_OVERRIDE

#define PWM_FREQENCY 30000
#define PIN_PWM_START 1
#define PIN_I2S_BCK 6
#define PIN_I2S_WS 7
#define PIN_I2S_DATA_OUT 8
#define PIN_I2S_DATA_IN 9
#define I2S_USE_APLL false  
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 5). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#define PIN_ADC1 21 
#define PIN_ADC2 22

#define I2S_AUTO_CLEAR true

// URLStream
//#define USE_ESP8266_AUDIO
#define URL_STREAM_CORE 0
#define URL_STREAM_PRIORITY 2
#define URL_STREAM_BUFFER_COUNT 10
#define STACK_SIZE 30000
#define URL_CLIENT_TIMEOUT 60000;
#define URL_HANDSHAKE_TIMEOUT 120000

// Default LED
#ifndef LED_BUILTIN
# define LED_BUILTIN 13 // pin number is specific to your esp32 board
#endif

typedef uint32_t eps32_i2s_sample_rate_type;

#endif

//----- ESP8266 -----------
#ifdef ESP8266
//#define USE_URL_ARDUINO // commented out because of compile errors
#define USE_I2S
#define USE_AUDIO_SERVER
#define USE_TYPETRAITS
#define USE_EFFECTS_SUITE
#define USE_TIMER
#define USE_URL_ARDUINO
#define USE_WIFI

#define PIN_PWM_START 12
#define PIN_I2S_BCK -1
#define PIN_I2S_WS -1
#define PIN_I2S_DATA_IN -1
#define PIN_I2S_DATA_OUT -1
#define I2S_USE_APLL false  
#define PIN_I2S_MUTE 23
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS

#define URL_CLIENT_TIMEOUT 60000;
#define URL_HANDSHAKE_TIMEOUT 120000

#endif

//------ NANO33BLE ----------
#if defined(ARDUINO_SEEED_XIAO_NRF52840_SENSE) || defined(ARDUINO_ARDUINO_NANO33BLE)
#define USE_NANO33BLE 
#define USE_I2S
#define USE_PWM
#define USE_TYPETRAITS
#define USE_EFFECTS_SUITE
#define USE_TIMER

#define PIN_PWM_START 6
#define PIN_I2S_BCK 2
#define PIN_I2S_WS 1
#define PIN_I2S_DATA_IN 3
#define PIN_I2S_DATA_OUT 3
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
#endif

//----- MBED -----------
#if defined(ARDUINO_ARCH_MBED_RP2040)
// install https://github.com/pschatzmann/rp2040-i2s
#define USE_I2S 1
#define USE_PWM
#define USE_ADC_ARDUINO
#define USE_TYPETRAITS
#define USE_EFFECTS_SUITE
#define USE_TIMER

#define PIN_ADC_START 26
#define PIN_PWM_START 6
#define PIN_I2S_BCK 26
#define PIN_I2S_WS PIN_I2S_BCK+1
#define PIN_I2S_DATA_IN 28
#define PIN_I2S_DATA_OUT 28
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS PIN_SPI0_SS

// fix missing __sync_synchronize symbol
#define FIX_SYNC_SYNCHRONIZE
#define IRAM_ATTR
#ifndef ADC_BUFFER_SIZE 
#define ADC_BUFFER_SIZE 1024
#endif

#ifndef ADC_BUFFERS 
#define ADC_BUFFERS 50
#endif

//#define USE_ESP8266_AUDIO

//----- RP2040 -----------
#elif defined(ARDUINO_ARCH_RP2040)
#define USE_I2S 
#define USE_PWM
#define USE_ADC_ARDUINO
#define USE_TYPETRAITS
#define USE_EFFECTS_SUITE
#define USE_TIMER

#define PIN_ADC_START 26
#define PIN_PWM_START 6
#define PIN_I2S_BCK 26
#define PIN_I2S_WS PIN_I2S_BCK+1
#define PIN_I2S_DATA_IN 28
#define PIN_I2S_DATA_OUT 28
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS PIN_SPI0_SS

// fix missing __sync_synchronize symbol
#define FIX_SYNC_SYNCHRONIZE
#define IRAM_ATTR

#ifndef ADC_BUFFER_SIZE 
#define ADC_BUFFER_SIZE 256
#endif

#ifndef ADC_BUFFERS 
#define ADC_BUFFERS 100
#endif

//#define USE_ESP8266_AUDIO
#endif

// The Pico W has WIFI support
#ifdef ARDUINO_RASPBERRY_PI_PICO_W
#  define USE_URL_ARDUINO
#  define USE_WIFI
#  define USE_WIFI_CLIENT_SECURE 
#  define USE_AUDIO_SERVER
#endif


//----- AVR -----------
#ifdef __AVR__
#define USE_PWM
#define USE_TIMER
#include <Ethernet.h>
#define USE_URL_ARDUINO
#ifndef assert
#define assert(T)
#endif

#define PIN_PWM_START 6
#define PIN_CS SS

#undef PWM_BUFFER_SIZE
#define PWM_BUFFER_SIZE 125

#undef DEFAULT_BUFFER_SIZE
#define DEFAULT_BUFFER_SIZE 125

// logging is using too much memory
#undef LOG_PRINTF_BUFFER_SIZE 
#define LOG_PRINTF_BUFFER_SIZE 80

#define NO_TRACED
#define NO_TRACEI

#endif

//---- STM32 ------------
#if defined(ARDUINO_ARCH_STM32F4) || defined(ARDUINO_ARCH_STM32)
#define STM32
#endif

#ifdef STM32
#define USE_I2S
#define USE_PWM
#define USE_TIMER
#define USE_ADC_ARDUINO
#define ADC_BUFFER_SIZE 1024
#define ADC_BUFFERS 20

#define PIN_ADC_START PA0
#define PIN_PWM_START PA0
#define PWM_DEFAULT_TIMER TIM2
#define PWM_FREQ_TIMER_NO 3

#define PIN_I2S_BCK -1
#define PIN_I2S_WS -1
#define PIN_I2S_DATA_IN -1
#define PIN_I2S_DATA_OUT -1
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS 10
#endif

//---- SAMD ------------

#ifdef ARDUINO_ARCH_SAMD
#define USE_I2S
#define PIN_I2S_BCK 1
#define PIN_I2S_WS PIN_I2S_BCK+1
#define PIN_I2S_DATA_IN 3
#define PIN_I2S_DATA_OUT 3
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#endif

#ifdef ARDUINO_SAMD_MKRWIFI1010
#include <WiFiNINA.h>
#define USE_URL_ARDUINO
#define USE_AUDIO_SERVER
#endif
//------ VS1053 ----------

// Default Pins for VS1053
#define VS1053_CS 5
#define VS1053_DCS 16
#define VS1053_DREQ 4
#define VS1053_RESET 15  
#define VS1053_CS_SD -1

// use 0 for https://github.com/baldram/ESP_VS1053_Library
// use 1 for https://github.com/pschatzmann/arduino-vs1053
#define VS1053_EXT 1
#define VS1053_DEFAULT_VOLUME 0.7


//----------------

#ifdef IS_DESKTOP
#define USE_URL_ARDUINO
#define USE_STREAM_WRITE_OVERRIDE
#endif

#ifndef ARDUINO
#define USE_STREAM_WRITE_OVERRIDE
#endif

#if USE_INLINE_VARS && !defined(INGNORE_INLINE_VARS)
#  define INLINE_VAR inline 
#else
#  define INLINE_VAR static 
#endif

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wvla"
