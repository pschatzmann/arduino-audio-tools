#pragma once

// Abstraction which provides the platform specific USBAudioDevice class implementation
#include "AudioToolsConfig.h"
#include "AudioTools/Concurrency.h"

#if defined(USE_TINYUSB) || defined(ARDUINO_ARCH_STM32)
#  include "USBAudioDeviceBase.h"
#  include "USBAudioDeviceTinyUSB.h"
#  if defined(USE_FREETROS)
#    include "USBAudioDeviceTinyUSBFreeRTOS.h"
     namespace audio_tools { using USBAudioStream = USBAudioDeviceTinyUSBFreeRTOS; }
#  else
     namespace audio_tools { using USBAudioStream = USBAudioDeviceTinyUSB; }
#  endif
#elif defined(ESP32)
#  include "USBAudioDeviceBase.h"
#  include "USBAudioDeviceESP32.h"
#elif defined(IS_ZEPHYR)
#  include "USBAudioDeviceZephyr.h"
#else
#  error "No USBAudioDevice implementation defined for this platform"
#endif
