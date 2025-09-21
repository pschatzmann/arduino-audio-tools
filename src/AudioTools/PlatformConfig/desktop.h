#pragma once

#if defined(IS_MIN_DESKTOP) 
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#  include "AudioTools/AudioLibs/Desktop/Time.h"
#  include "AudioTools/AudioLibs/Desktop/Main.h"
#  define USE_STREAM_READ_OVERRIDE
#  define USE_SD_NO_NS
#  define USE_TIMER
#  define USE_CPP_TASK
#  ifndef EXIT_ON_STOP
#    define EXIT_ON_STOP
#  endif
#elif defined(IS_DESKTOP_WITH_TIME_ONLY)
#  include "AudioTools/AudioLibs/Desktop/Time.h"
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#  define USE_SD_NO_NS
#  define USE_TIMER
#  define USE_CPP_TASK
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
#  define USE_STREAM_WRITE_OVERRIDE
#  define USE_STREAM_READ_OVERRIDE
#  define USE_STREAM_READCHAR_OVERRIDE
#  ifndef EXIT_ON_STOP
#    define EXIT_ON_STOP
#  endif
#  define USE_TIMER
#  define USE_CPP_TASK
//#  define USE_3BYTE_INT24
typedef WiFiClient WiFiClientSecure;
#elif defined(ARDUINO)
#  include "Arduino.h"
// --- ESP32 ------------
// E.g when using the Espressif IDF. Use cmake for the necesseary defines
#elif defined(ESP32_CMAKE)
#  define ESP32
#  include "esp_idf_version.h"
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#else 
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#  define IS_JUPYTER
#  define USE_STREAM_READ_OVERRIDE
#endif