#pragma once

// Abstraction which provides the platform specific USBAudioDevice class implementation
#include "AudioToolsConfig.h"
#if defined(USE_TINYUSB)
#  include "USBAudioDeviceBase.h"
#  include "USBAudioDeviceTinyUSB.h"
#  if defined(FREERTOS_H) || defined(INC_FREERTOS_H)
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
