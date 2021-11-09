#pragma once

inline static void checkMemory(bool stackCheck=false){
#ifdef ESP32
    assert(heap_caps_check_integrity_all(true)); 
    if (stackCheck) printf("stack available: %d \n'", uxTaskGetStackHighWaterMark(NULL));
#endif    
}