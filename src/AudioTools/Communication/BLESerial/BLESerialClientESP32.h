#pragma once

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>
#include "AudioTools/Concurrency/RTOS.h"
#include "BLESerialStream.h"

namespace audio_tools {

/**
 * @brief A BLE client that implements a serial protocol, so that it
 * can be used to send and receive audio. In BLE terminology this is a
 * Central. This implementation uses the BLE library of the ESP32 Arduino
 * core (Bluedroid or NimBLE).
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BLESerialClientESP32 : public BLESerialClientBase,
                             public BLEClientCallbacks,
                             public BLEAdvertisedDeviceCallbacks {
 public:
  BLESerialClientESP32(int mtu = 517) : BLESerialClientBase(mtu) {
    selfBLESerialClientESP32() = this;
    rx_buffer.setMutex(&rx_mutex);
  }

  void end() override {
    TRACEI();
    flush();
    is_started = false;
    is_setup = false;
    is_found = false;
    BLEDevice::deinit();
    p_client = nullptr;
  }

 protected:
  BLEClient *p_client = nullptr;
  BLERemoteCharacteristic *ch01_char = nullptr;  // read
  BLERemoteCharacteristic *ch02_char = nullptr;  // write
  BLERemoteCharacteristic *info_char = nullptr;
  BLEAdvertisedDevice advertised_device;
  Mutex rx_mutex;
  volatile bool is_found = false;
  volatile bool is_setup = false;

  static BLESerialClientESP32 *&selfBLESerialClientESP32() {
    static BLESerialClientESP32 *self = nullptr;
    return self;
  }

  bool startClient(const char *localName) override {
    if (!BLEDevice::init(localName)) return false;
    BLEDevice::setMTU(max_transfer_size);
    BLEScan *p_scan = BLEDevice::getScan();
    p_scan->setAdvertisedDeviceCallbacks(this);
    p_scan->setActiveScan(true);
    startScan();
    return true;
  }

  void startScan() {
    BLEScan *p_scan = BLEDevice::getScan();
    if (!p_scan->isScanning()) {
      LOGI("BLESerial: scanning for %s", BLE_SERIAL_SERVICE_UUID);
      // scan in the background until the server was found
      p_scan->start(0, nullptr, false);
    }
  }

  bool isConnected() override {
    return is_setup && p_client != nullptr && p_client->isConnected();
  }

  int getMTU() override {
    int result = max_transfer_size;
    if (isConnected()) {
      int mtu = p_client->getMTU();
      if (mtu > 0 && mtu < result) result = mtu;
    }
    return result;
  }

  bool connect() override {
    if (!is_found) {
      startScan();
      return false;
    }
    TRACEI();

    if (p_client == nullptr) {
      p_client = BLEDevice::createClient();
      p_client->setClientCallbacks(this);
    }

    // connect to the BLE server
    LOGI("BLESerial: connecting to %s",
         advertised_device.getAddress().toString().c_str());
    if (!p_client->connect(&advertised_device)) {
      LOGE("BLESerial: connect failed");
      return reconnectLater();
    }
    p_client->setMTU(max_transfer_size);

    // obtain a reference to the service in the remote BLE server
    BLERemoteService *p_remote_service =
        p_client->getService(BLE_SERIAL_SERVICE_UUID);
    if (p_remote_service == nullptr) {
      LOGE("BLESerial: service not found: %s", BLE_SERIAL_SERVICE_UUID);
      return reconnectLater();
    }

    ch01_char = p_remote_service->getCharacteristic(BLE_CH1_UUID);
    ch02_char = p_remote_service->getCharacteristic(BLE_CH2_UUID);
    if (ch01_char == nullptr || ch02_char == nullptr) {
      LOGE("BLESerial: characteristics not found");
      return reconnectLater();
    }

    info_char = nullptr;
    if (is_audio_info_active) {
      info_char = p_remote_service->getCharacteristic(BLE_INFO_UUID);
      if (info_char == nullptr) {
        LOGW("BLESerial: audio info not supported by server");
      } else if (info_char->canNotify()) {
        info_char->registerForNotify(notifyCallback);
      }
    }

    LOGI("BLESerial: connected with mtu %d", getMTU());
    is_setup = true;
    return true;
  }

  /// disconnect and scan again
  bool reconnectLater() {
    is_found = false;
    if (p_client != nullptr && p_client->isConnected()) p_client->disconnect();
    startScan();
    return false;
  }

  int readPacket(uint8_t *data, int maxLen) override {
    if (!ch01_char->canRead()) return 0;
    // auto: it changed from std::string to String
    auto str = ch01_char->readValue();
    int len = str.length();
    if (len > maxLen) len = maxLen;
    memcpy(data, str.c_str(), len);
    return len;
  }

  bool writePacket(const uint8_t *data, int len) override {
    if (!ch02_char->canWrite() && !ch02_char->canWriteNoResponse()) return false;
    return ch02_char->writeValue((uint8_t *)data, len, write_confirmation_flag);
  }

  void writeAudioInfo(StrView str) override {
    TRACEI();
    if (!isConnected() || info_char == nullptr) return;
    info_char->writeValue((uint8_t *)str.c_str(), str.length() + 1, true);
  }

  bool readAudioInfo() override {
    if (info_char == nullptr || !info_char->canRead()) return false;
    auto str = info_char->readValue();
    receiveAudioInfo((const uint8_t *)str.c_str(), str.length());
    return str.length() > 0;
  }

  void onConnect(BLEClient *pClient) override { TRACEI(); }

  void onDisconnect(BLEClient *pClient) override {
    TRACEI();
    is_setup = false;
    is_found = false;
  }

  /// scanning results
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(
            BLEUUID(BLE_SERIAL_SERVICE_UUID))) {
      LOGI("BLESerial: service '%s' found", BLE_SERIAL_SERVICE_UUID);
      advertised_device = advertisedDevice;
      is_found = true;
      // we found what we are looking for
      BLEDevice::getScan()->stop();
    }
  }

  static void notifyCallback(BLERemoteCharacteristic *characteristic,
                             uint8_t *data, size_t length, bool isNotify) {
    TRACEI();
    BLESerialClientESP32 *self = selfBLESerialClientESP32();
    if (self != nullptr && characteristic == self->info_char) {
      self->receiveAudioInfo(data, length);
    }
  }
};

}  // namespace audio_tools
