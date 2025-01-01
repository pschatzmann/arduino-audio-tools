#pragma once

#include "AudioConfig.h"


#if defined(ESP32_CMAKE) && !defined(ARDUINO) 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// delay and millis is needed by this framework
#define DESKTOP_MILLIS_DEFINED

inline void delay(uint32_t ms){ vTaskDelay(ms / portTICK_PERIOD_MS);}
inline uint32_t millis() {return (xTaskGetTickCount() * portTICK_PERIOD_MS);}
inline void delayMicroseconds(uint32_t ms) {esp_rom_delay_us(ms);}
inline uint64_t micros() { return xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;}

#endif

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
inline static void checkMemory(bool memoryCheck=false) {
    #if defined(ESP32) && defined(ARDUINO)
        assert(heap_caps_check_integrity_all(true)); 
        if (memoryCheck) printf("==> Available stack: %d - heap: %u\n",(int) uxTaskGetStackHighWaterMark(NULL), (unsigned)ESP.getFreeHeap());
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