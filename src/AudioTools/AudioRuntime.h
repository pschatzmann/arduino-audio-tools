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
inline static void checkMemory(bool stackCheck=false) {
    #ifdef ESP32
        assert(heap_caps_check_integrity_all(true)); 
        if (stackCheck) printf("stack available: %d \n'", uxTaskGetStackHighWaterMark(NULL));
    #endif    
}


