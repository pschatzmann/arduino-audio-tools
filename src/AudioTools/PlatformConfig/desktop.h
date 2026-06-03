#pragma once
// cmake builds for desktop, zephyr and esp32 without arduino support
#if defined(IS_MIN_DESKTOP) 
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#  include "AudioTools/AudioLibs/Desktop/Time.h"
#  include "AudioTools/AudioLibs/Desktop/Main.h"
#  define USE_SD_NO_NS
#  define USE_TIMER
#  define USE_CPP_TASK
#  define USE_STD_CONCURRENCY
#  ifndef EXIT_ON_STOP
#    define EXIT_ON_STOP
#  endif
#elif defined(IS_DESKTOP_WITH_TIME_ONLY)
#  include "AudioTools/AudioLibs/Desktop/Time.h"
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#  define USE_SD_NO_NS
#  define USE_TIMER
#  define USE_CPP_TASK
#  define USE_STD_CONCURRENCY
#  ifndef EXIT_ON_STOP
#    define EXIT_ON_STOP
#  endif
#elif defined(IS_DESKTOP)
#  include "Arduino.h"
#  include <Client.h>
#  include <WiFi.h>
#  define USE_SD_NO_NS
#  define USE_WIFI
#  define USE_URL_ARDUINO
#  ifndef EXIT_ON_STOP
#    define EXIT_ON_STOP
#  endif
#  define USE_TIMER
#  define USE_CPP_TASK
#  define USE_STD_CONCURRENCY
//#  define USE_3BYTE_INT24
typedef WiFiClient WiFiClientSecure;
#elif defined(ESP32_CMAKE)
#  define ESP32
#  include "esp_idf_version.h"
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#else 
//#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
//#  define USE_CPP_TASK
//#  define USE_STD_CONCURRENCY
//#  define IS_JUPYTER
//#  define USE_STREAM_READ_OVERRIDE
#endif