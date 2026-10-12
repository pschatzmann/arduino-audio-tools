#pragma once

// Abstraction which provides the platform specific USB Audio Host class
// implementation: USBAudioHostStream reads from an attached USB microphone
// and/or writes to an attached USB speaker with the same API as the
// USBAudioStream device.
//
// - Adafruit TinyUSB (USE_TINYUSB / USE_TINYUSB_HOST: RP2040/RP2350, SAMD,
//   nRF52, CH32 with a native host port, Pico-PIO-USB or MAX3421E):
//   USBAudioHostTinyUSB, or USBAudioHostTinyUSBFreeRTOS with USE_FREERTOS
// - ESP32-S2/S3/P4: USBAudioHostESP32 using the ESP-IDF usb_host library.
//   Define USE_TINYUSB before including this file to use the Adafruit
//   TinyUSB host stack instead.
// - STM32: USBAudioHostTinyUSB (Adafruit TinyUSB with host support)
#include "AudioToolsConfig.h"

#if defined(USE_TINYUSB) || defined(USE_TINYUSB_HOST) || \
    (defined(ARDUINO_ARCH_STM32) && !defined(ESP32))
#  include "USBAudioHostTinyUSB.h"
#  if defined(USE_FREERTOS)
#    include "AudioTools/Concurrency.h"
#    include "USBAudioHostTinyUSBFreeRTOS.h"
     namespace audio_tools { using USBAudioHostStream = USBAudioHostTinyUSBFreeRTOS; }
#  else
     namespace audio_tools { using USBAudioHostStream = USBAudioHostTinyUSB; }
#  endif
#elif defined(ESP32)
#  include "AudioTools/Concurrency.h"
#  include "USBAudioHostESP32.h"
   namespace audio_tools { using USBAudioHostStream = USBAudioHostESP32; }
#elif defined(ARDUINO_ARCH_RENESAS)
#  error "USB Audio Host is not supported on Renesas: the core provides no USB host stack"
#elif defined(IS_ZEPHYR)
#  error "USB Audio Host is not supported on Zephyr"
#else
#  error "USB Audio Host is not supported on this platform"
#endif
