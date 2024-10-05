#pragma once
#include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#include <iostream>
#include <thread>

// #ifndef DESKTOP_MILLIS_DEFINED
// #define DESKTOP_MILLIS_DEFINED

namespace audio_tools {

/// Returns the milliseconds since the start
inline uint32_t millis(){
    using namespace std::chrono;
    // Get current time with precision of milliseconds
    auto now = time_point_cast<milliseconds>(system_clock::now());
    // sys_milliseconds is type time_point<system_clock, milliseconds>
    using sys_milliseconds = decltype(now);
    // Convert time_point to signed integral type
    return now.time_since_epoch().count();
}

// sleep ms milliseconds
void delay(unsigned long ms){
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));    
}

// sleep us milliseconds
void delayMicroseconds(unsigned int us){
    std::this_thread::sleep_for(std::chrono::microseconds(us));        
}

// Returns the micros of milliseconds passed since epich
inline unsigned long micros(void){
    using namespace std::chrono;
    // Get current time with precision of milliseconds
    auto now = time_point_cast<microseconds>(system_clock::now());
    // sys_milliseconds is type time_point<system_clock, milliseconds>
    using sys_milliseconds = decltype(now);
    // Convert time_point to signed integral type
    return now.time_since_epoch().count();
}


}

// #endif