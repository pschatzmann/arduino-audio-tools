#pragma once

// Dependendency: https://github.com/stm32duino/STM32FreeRTOS

#if defined(ARDUINO_ARCH_STM32)
#  if __has_include("STM32FreeRTOS.h")
#    include "STM32FreeRTOS.h"
#    include "RTOS.h"
#    define USE_FREETROS
#  else
#    warning("STM32FreeRTOS not found")
#  endif

#include "AudioTools/Concurrency/LockGuard.h"
#include "AudioTools/Concurrency/SynchronizedQueue.h"
#include "AudioTools/Concurrency/SynchronizedBuffer.h"

#endif
