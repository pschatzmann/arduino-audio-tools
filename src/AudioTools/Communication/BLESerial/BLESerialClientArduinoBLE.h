#pragma once

#include <ArduinoBLE.h>
#include "BLESerialStream.h"

namespace audio_tools {

/**
 * @brief A BLE client that implements a serial protocol, so that it
 * can be used to send and receive audio. In BLE terminology this is a
 * Central. This implementation uses the ArduinoBLE library.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BLESerialClientArduinoBLE : public BLESerialClientBase {
 public:
  BLESerialClientArduinoBLE(int mtu = 242) : BLESerialClientBase(mtu) {
    selfBLESerialClientArduinoBLE() = this;
  }

  void end() override {
    TRACEI();
    flush();
    is_started = false;
    is_setup = false;
    if (peripheral) peripheral.disconnect();
    BLE.end();
  }

 protected:
  BLEDevice peripheral;
  BLECharacteristic ch01_char;  // read
  BLECharacteristic ch02_char;  // write
  BLECharacteristic info_char;
  bool is_setup = false;

  static BLESerialClientArduinoBLE *&selfBLESerialClientArduinoBLE() {
    static BLESerialClientArduinoBLE *self = nullptr;
    return self;
  }

  bool startClient(const char *localName) override {
    if (!BLE.begin()) return false;
    BLE.setLocalName(localName);
    return BLE.scanForUuid(BLE_SERIAL_SERVICE_UUID) != 0;
  }

  bool isConnected() override {
    // BLEDevice::connected() also processes the BLE events
    return is_setup && peripheral.connected();
  }

  bool connect() override {
    is_setup = false;
    BLEDevice device = BLE.available();
    if (!device) return false;
    TRACEI();
    BLE.stopScan();
    peripheral = device;

    LOGI("BLESerial: connecting to %s", peripheral.address().c_str());
    if (!peripheral.connect()) {
      LOGE("BLESerial: connect failed");
      return reconnectLater();
    }

    if (!peripheral.discoverService(BLE_SERIAL_SERVICE_UUID)) {
      LOGE("BLESerial: service not found: %s", BLE_SERIAL_SERVICE_UUID);
      return reconnectLater();
    }

    ch01_char = peripheral.characteristic(BLE_CH1_UUID);
    ch02_char = peripheral.characteristic(BLE_CH2_UUID);
    if (!ch01_char || !ch02_char) {
      LOGE("BLESerial: characteristics not found");
      return reconnectLater();
    }

    if (is_audio_info_active) {
      info_char = peripheral.characteristic(BLE_INFO_UUID);
      if (!info_char) {
        LOGW("BLESerial: audio info not supported by server");
      } else if (info_char.canSubscribe()) {
        info_char.setEventHandler(BLEUpdated, onInfoUpdatedCB);
        info_char.subscribe();
      }
    }

    LOGI("BLESerial: connected");
    is_setup = true;
    return true;
  }

  /// disconnect and scan again
  bool reconnectLater() {
    if (peripheral) peripheral.disconnect();
    BLE.scanForUuid(BLE_SERIAL_SERVICE_UUID);
    return false;
  }

  int readPacket(uint8_t *data, int maxLen) override {
    if (!ch01_char.read()) return 0;
    int len = ch01_char.valueLength();
    if (len > maxLen) len = maxLen;
    memcpy(data, ch01_char.value(), len);
    return len;
  }

  bool writePacket(const uint8_t *data, int len) override {
    // returns 1 if successful
    return ch02_char.writeValue(data, len, write_confirmation_flag) != 0;
  }

  void writeAudioInfo(StrView str) override {
    TRACEI();
    if (!isConnected() || !info_char) return;
    info_char.writeValue((const uint8_t *)str.c_str(), str.length() + 1);
  }

  bool readAudioInfo() override {
    if (!info_char || !info_char.read()) return false;
    receiveAudioInfo(info_char.value(), info_char.valueLength());
    return info_char.valueLength() > 0;
  }

  static void onInfoUpdatedCB(BLEDevice device,
                              BLECharacteristic characteristic) {
    TRACEI();
    BLESerialClientArduinoBLE *self = selfBLESerialClientArduinoBLE();
    if (self == nullptr) return;
    self->receiveAudioInfo(characteristic.value(),
                           characteristic.valueLength());
  }
};

}  // namespace audio_tools
