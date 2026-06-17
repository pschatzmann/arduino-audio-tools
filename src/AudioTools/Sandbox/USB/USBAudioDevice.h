#pragma once
#include "USBAudioDeviceBase.h"

// Abstraction which provides the platform specific USBAudioDevice class implementation

#if defined(USE_TINYUSB)
#include "USBAudioDeviceTinyUSB.h"
#elif defined(ESP32)
#include "USBAudioDeviceESP32.h"
#endif