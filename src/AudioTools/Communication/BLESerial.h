#pragma once

/**
 * Serial communication via BLE: BLESerialServer and BLESerialClient.
 *
 * On the ESP32 the BLE library of the Arduino core is used by default. On
 * all other platforms (and on the ESP32 if BLE_SERIAL_USE_ARDUINOBLE is
 * defined before including this header) the ArduinoBLE library is used.
 */

#if defined(ESP32) && !defined(BLE_SERIAL_USE_ARDUINOBLE)
#include "AudioTools/Communication/BLESerial/BLESerialServerESP32.h"
#include "AudioTools/Communication/BLESerial/BLESerialClientESP32.h"
namespace audio_tools {
/// @brief Default BLE serial server for the platform
/// @ingroup communications
using BLESerialServer = BLESerialServerESP32;
/// @brief Default BLE serial client for the platform
/// @ingroup communications
using BLESerialClient = BLESerialClientESP32;
}  // namespace audio_tools
#else
#include "AudioTools/Communication/BLESerial/BLESerialServerArduinoBLE.h"
#include "AudioTools/Communication/BLESerial/BLESerialClientArduinoBLE.h"
namespace audio_tools {
/// @brief Default BLE serial server for the platform
/// @ingroup communications
using BLESerialServer = BLESerialServerArduinoBLE;
/// @brief Default BLE serial client for the platform
/// @ingroup communications
using BLESerialClient = BLESerialClientArduinoBLE;
}  // namespace audio_tools
#endif
