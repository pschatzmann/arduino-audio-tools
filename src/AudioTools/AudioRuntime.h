#pragma once
/**
 * @brief Public generic methods 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


/// stops any further processing by spinning in an endless loop
inline void stop() {
  #ifdef EXIT_ON_STOP
    exit(0);
  #else
    while(true){
      delay(1000);
    }
  #endif
}

/// Executes heap_caps_check_integrity_all()
inline static void checkMemory(bool memoryCheck=false) {
    #ifdef ESP32
        assert(heap_caps_check_integrity_all(true)); 
        if (memoryCheck) printf("==> Available stack: %d - heap: %d\n", uxTaskGetStackHighWaterMark(NULL), ESP.getFreeHeap());
    #endif    
}


