#pragma once
#include "AudioLibs/Desktop/NoArduino.h"
#include <iostream>

#define DESKTOP_MILLIS_DEFINED

namespace audio_tools {

/// Waits for the indicated milliseconds
void delay(uint32_t ms) {
    //std::this_thread::sleep_for(std::chrono::milliseconds(ms));    
    auto end = millis()+ms;
    while(millis()<=end);
}

/// Returns the milliseconds since the start
uint32_t millis(){
    using namespace std::chrono;
    // Get current time with precision of milliseconds
    auto now = time_point_cast<milliseconds>(system_clock::now());
    // sys_milliseconds is type time_point<system_clock, milliseconds>
    using sys_milliseconds = decltype(now);
    // Convert time_point to signed integral type
    return now.time_since_epoch().count();
}

}