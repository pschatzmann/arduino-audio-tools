/**
 * @file AudioRuntime.cpp
 * @author Phil Schatzmann
 * @brief Some platform specific exceptional things which can't be implemented in a header
 * @version 0.1
 * @date 2022-02-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioConfig.h"

#if defined(ARDUINO_ARCH_RP2040) && defined(FIX_SYNC_SYNCHRONIZE)
extern "C" void __sync_synchronize(){
}
#endif

