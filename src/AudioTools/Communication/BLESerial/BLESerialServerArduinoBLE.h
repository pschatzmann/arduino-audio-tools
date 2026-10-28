#pragma once

#include <ArduinoBLE.h>
#include "BLESerialStream.h"

namespace audio_tools {

/**
 * @brief A BLE server that implements a serial protocol, so that it
 * can be used to send and receive audio. In BLE terminology this is a
 * Peripheral. This implementation uses the ArduinoBLE library.
 *
 * The data is exchanged with the client when BLE.poll() is called: this is
 * done automatically in readBytes(), available(), write() and
 * availableForWrite().
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BLESerialServerArduinoBLE : public BLESerialServerBase {
 public:
  BLESerialServerArduinoBLE(int mtu = 242) : BLESerialServerBase(mtu) {
    selfBLESerialServerArduinoBLE() = this;
  }

  ~BLESerialServerArduinoBLE() { release(); }

  /// starts a BLE server with the indicated name
  bool begin(const char *name) override {
    TRACEI();
    if (!BLE.begin()) {
      LOGE("BLESerial: starting BLE failed");
      return false;
    }
    setupBuffers();

    // set the local name peripheral advertises
    BLE.setLocalName(name);
    BLE.setDeviceName(name);

    // assign event handlers for connected, disconnected to peripheral
    BLE.setEventHandler(BLEConnected, onConnectCB);
    BLE.setEventHandler(BLEDisconnected, onDisconnectCB);

    setupBLEService();

    // start advertising
    BLE.advertise();
    return true;
  }

  void end() override {
    TRACEI();
    BLE.end();
    release();
  }

  bool connected() override {
    // BLE.connected() also processes the BLE events
    return p_service != nullptr && BLE.connected();
  }

 protected:
  BLEService *p_service = nullptr;
  BLECharacteristic *p_ch01_char = nullptr;
  BLECharacteristic *p_ch02_char = nullptr;
  BLECharacteristic *p_info_char = nullptr;
  Vector<uint8_t> packet{0};

  static BLESerialServerArduinoBLE *&selfBLESerialServerArduinoBLE() {
    static BLESerialServerArduinoBLE *self = nullptr;
    return self;
  }

  void poll() override { BLE.poll(); }

  void release() {
    delete p_info_char;
    delete p_ch02_char;
    delete p_ch01_char;
    delete p_service;
    p_info_char = nullptr;
    p_ch02_char = nullptr;
    p_ch01_char = nullptr;
    p_service = nullptr;
  }

  void setupBLEService() {
    TRACEI();
    release();
    // the value read by the client has a fixed length: ArduinoBLE determines
    // the length of the read response before calling the read event handler
    int read_size = packetSize() + BLE_SERIAL_HEADER_SIZE;
    packet.resize(read_size);
    p_service = new BLEService(BLE_SERIAL_SERVICE_UUID);
    p_ch01_char =
        new BLECharacteristic(BLE_CH1_UUID, BLERead, read_size, true);
    p_ch02_char = new BLECharacteristic(
        BLE_CH2_UUID, BLEWrite | BLEWriteWithoutResponse, getMTU());

    // assign event handlers for characteristic
    p_ch01_char->setEventHandler(BLERead, onReadCB);
    p_ch02_char->setEventHandler(BLEWritten, onWriteCB);
    p_service->addCharacteristic(*p_ch01_char);
    p_service->addCharacteristic(*p_ch02_char);

    if (is_audio_info_active) {
      p_info_char = new BLECharacteristic(
          BLE_INFO_UUID, BLERead | BLEWrite | BLENotify, 40);
      p_info_char->setEventHandler(BLEWritten, onInfoWriteCB);
      p_service->addCharacteristic(*p_info_char);
      writeAudioInfo(toStr(audioInfo()));
    }

    // set the UUID for the service this peripheral advertises
    BLE.setAdvertisedService(*p_service);
    BLE.addService(*p_service);
  }

  void writeAudioInfo(StrView str) override {
    TRACEI();
    if (p_info_char == nullptr) return;
    LOGI("AudioInfo: %s", str.c_str());
    // notifies a subscribed client
    p_info_char->writeValue((const uint8_t *)str.c_str(), str.length() + 1);
  }

  static void onConnectCB(BLEDevice device) { TRACEI(); }

  static void onDisconnectCB(BLEDevice device) {
    TRACEI();
    BLESerialServerArduinoBLE *self = selfBLESerialServerArduinoBLE();
    if (self == nullptr) return;
    self->onDisconnected();
    BLE.advertise();
  }

  /// store the received data
  static void onWriteCB(BLEDevice device, BLECharacteristic characteristic) {
    TRACED();
    BLESerialServerArduinoBLE *self = selfBLESerialServerArduinoBLE();
    if (self == nullptr) return;
    self->receiveData(characteristic.value(), characteristic.valueLength());
  }

  static void onInfoWriteCB(BLEDevice device,
                            BLECharacteristic characteristic) {
    TRACEI();
    BLESerialServerArduinoBLE *self = selfBLESerialServerArduinoBLE();
    if (self == nullptr) return;
    self->receiveAudioInfo(characteristic.value(),
                           characteristic.valueLength());
  }

  /// provide the next batch of data
  static void onReadCB(BLEDevice device, BLECharacteristic characteristic) {
    TRACED();
    BLESerialServerArduinoBLE *self = selfBLESerialServerArduinoBLE();
    if (self == nullptr) return;
    self->provideData(self->packet.data(), self->packet.size());
    self->p_ch01_char->writeValue(self->packet.data(), self->packet.size());
  }
};

}  // namespace audio_tools
