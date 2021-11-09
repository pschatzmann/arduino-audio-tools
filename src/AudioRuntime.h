#pragma once

inline static void checkMemory(){
#ifdef ESP32
    heap_caps_check_integrity_all(true); 
    printf("stack available: %d \n'", uxTaskGetStackHighWaterMark(NULL));
#endif    
}