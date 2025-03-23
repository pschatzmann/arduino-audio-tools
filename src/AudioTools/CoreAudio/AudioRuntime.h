#pragma once

#include "AudioToolsConfig.h"

namespace audio_tools {

/**
 * @brief Public generic methods 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

/// stops any further processing by spinning in an endless loop  @ingroup basic
inline void stop() {
  #ifdef EXIT_ON_STOP
    exit(0);
  #else
    while(true){
      delay(1000);
    }
  #endif
}

/// Executes heap_caps_check_integrity_all()  @ingroup basic
inline static void checkMemory(bool printMemory=false) {
    #if defined(ESP32) && defined(ARDUINO)
        assert(heap_caps_check_integrity_all(true)); 
        if (printMemory) Serial.printf("==> Available stack: %d - heap: %u - psram: %u\n",(int) uxTaskGetStackHighWaterMark(NULL), (unsigned)ESP.getFreeHeap(),(unsigned)ESP.getFreePsram());
    #endif    
}

#ifdef ARDUINO
inline void printNChar(char ch, int n){
  for (int j=0;j<n;j++) Serial.print(ch);
  Serial.println();
}

#ifndef ESP_ARDUINO_VERSION_STR
#  define df2xstr(s)              #s
#  define df2str(s)               df2xstr(s)
#  define ESP_ARDUINO_VERSION_STR df2str(ESP_ARDUINO_VERSION_MAJOR) "." df2str(ESP_ARDUINO_VERSION_MINOR) "." df2str(ESP_ARDUINO_VERSION_PATCH)
#endif

/// prints the available version information
inline void printVersionInfo() {
  printNChar('*',50);
  Serial.print("AudioTools: ");
  Serial.println(AUDIOTOOLS_VERSION);
  Serial.print("Arduino: ");
  Serial.println(ARDUINO);
#ifdef ESP32
  Serial.print("Arduino ESP Core Version: ");
  Serial.println(ESP_ARDUINO_VERSION_STR);
  Serial.print("IDF Version: ");
  Serial.println(IDF_VER);
#endif
  printNChar('*',50);
}

#endif

}