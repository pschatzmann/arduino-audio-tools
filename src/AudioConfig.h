/**
 * @author Phil Schatzmann
 * @brief AutioTools Configuration
 * @copyright GPLv3
 * 
 */
#pragma once
#include <string.h>
#include "AudioTools/AudioLogger.h"
/**
 * ------------------------------------------------------------------------- 
 * @brief Optional Functionality - comment out if not wanted
 */

// Activate ESP32 Audio - for ESP32, ESP8266 and Raspberry Pico
#define USE_ESP8266_AUDIO

// Activate the A2DP library - for ESP32 only
#ifdef ESP32
#define USE_A2DP
#endif

/**
 * ------------------------------------------------------------------------- 
 * @brief Common Default Settings that can usually be changed in the API
 */
#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // pin number is specific to your esp32 board
#endif

#define DEFAULT_BUFFER_SIZE 1024
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_CHANNELS 2
#define DEFAULT_BITS_PER_SAMPLE 16
#define I2S_DEFAULT_PORT 0
#define I2S_BUFFER_SIZE 1024
#define I2S_BUFFER_COUNT 5
#define A2DP_BUFFER_SIZE 4096
#define A2DP_BUFFER_COUNT 8
#define DEFAUT_ADC_PIN 34

/**
 * ------------------------------------------------------------------------- 
 * @brief Platform specific Settings
 * 
 */
#ifdef ESP32
#include "esp32-hal-log.h"
#define I2S_SUPPORT
#define PIN_I2S_BCK 14
#define PIN_I2S_WS 15
#define PIN_I2S_DATA_IN 32
#define PIN_I2S_DATA_OUT 22
#define I2S_USE_APLL false  
// Default Setting: The mute pin can be switched off by setting it to -1. Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE 23
#define SOFT_MUTE_VALUE LOW  
#endif

#ifdef ESP8266
#define I2S_SUPPORT
#define PIN_I2S_BCK -1
#define PIN_I2S_WS -1
#define PIN_I2S_DATA_IN -1
#define PIN_I2S_DATA_OUT -1
#define I2S_USE_APLL false  
#define PIN_I2S_MUTE 23
#define SOFT_MUTE_VALUE LOW  
#endif

/**
 * ------------------------------------------------------------------------- 
 * @brief Logging
 * Logging Configuration in Arduino -> set USE_AUDIO_LOGGING to false if you want to deactivate Logging.
 * When using cmake you can set -DUSE_AUDIO_LOGGING=false
 * You can also change the LOG_LEVEL and LOG_STREAM here.
 * However it is recommended to do it in your Sketch e.g with AudioLogger::instance().begin(Serial,AudioLogger::Warning);
 */
 
#ifndef USE_AUDIO_LOGGING
#define USE_AUDIO_LOGGING false
#endif 

// Logging Implementation
#if USE_AUDIO_LOGGING
#define LOGD(...) AudioLogger::instance().printLog(__FILE__,__LINE__, AudioLogger::Debug,  __VA_ARGS__)
#define LOGI(...) AudioLogger::instance().printLog(__FILE__,__LINE__, AudioLogger::Info,  __VA_ARGS__)
#define LOGW(...) AudioLogger::instance().printLog(__FILE__,__LINE__, AudioLogger::Warning,  __VA_ARGS__)
#define LOGE(...) AudioLogger::instance().printLog(__FILE__,__LINE__, AudioLogger::Error, __VA_ARGS__)
#else 
#define LOGD(...) 
#define LOGI(...) 
#define LOGW(...) 
#define LOGE(...) 
#endif