#pragma once

#include "AudioConfig.h"

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
        if (memoryCheck) printf("==> Available stack: %d - heap: %d\n", uxTaskGetStackHighWaterMark(NULL), ESP.getFreeHeap());
    #endif    
}


