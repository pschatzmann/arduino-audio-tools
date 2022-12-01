#pragma once
#include "AudioLibs/NoArduino.h"
#include <iostream>

namespace audio_tools {

/// Waits for the indicated milliseconds
void delay(uint64_t ms) {
    //std::this_thread::sleep_for(std::chrono::milliseconds(ms));    
}

/// Returns the milliseconds since the start
uint64_t millis(){
    using namespace std::chrono;
    // Get current time with precision of milliseconds
    auto now = time_point_cast<milliseconds>(system_clock::now());
    // sys_milliseconds is type time_point<system_clock, milliseconds>
    using sys_milliseconds = decltype(now);
    // Convert time_point to signed integral type
    return now.time_since_epoch().count();
}

size_t HardwareSerial::write(uint8_t ch) {
    cout << ch;
    return 1;
}

}