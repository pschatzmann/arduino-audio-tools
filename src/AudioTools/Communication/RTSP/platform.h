#pragma once

#ifdef ESP32
#include "platform-esp32.h"
#elif defined(ARDUINO_ARCH_RP2040)
#include "platform-rp2040.h"
#else
#include "platform-posix.h"
#endif
