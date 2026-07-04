#pragma once

// Abstraction which provides the platform specific USBAudioDevice class implementation
#include "AudioToolsConfig.h"

#if defined(USE_TINYUSB) 
#  include "USBAudioDeviceBase.h"
#  include "USBAudioDeviceTinyUSB.h"
#  if defined(USE_FREETROS)
#    include "AudioTools/Concurrency.h"
#    include "USBAudioDeviceTinyUSBFreeRTOS.h"
     namespace audio_tools { using USBAudioStream = USBAudioDeviceTinyUSBFreeRTOS; }
#  else
     namespace audio_tools { using USBAudioStream = USBAudioDeviceTinyUSB; }
#  endif
#elif defined(ARDUINO_ARCH_STM32)
#  include "USBAudioDeviceTinyUSB.h"
   namespace audio_tools { using USBAudioStream = USBAudioDeviceTinyUSB; }
#elif defined(ESP32)
#  include "AudioTools/Concurrency.h"
#  include "USBAudioDeviceBase.h"
#  include "USBAudioDeviceESP32.h"
#elif defined(IS_ZEPHYR)
#  include "USBAudioDeviceZephyr.h"
#else
#  error "No USBAudioDevice implementation defined for this platform"
#endif
