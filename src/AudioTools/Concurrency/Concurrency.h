#pragma once

#if defined(ESP32)
#include "RTOS.h"
#elif defined(RP2040)
#include "RP2040.h"
#elif defined(IS_ZEPHYR)
#include "Zephyr.h"
#elif defined(ARDUINO_ARCH_STM32)
#include "STM32.h"
#elif defined(USE_STD_CONCURRENCY)
#include "Desktop.h"
#endif
#include "Mutex.h"
#include "LockGuard.h"
