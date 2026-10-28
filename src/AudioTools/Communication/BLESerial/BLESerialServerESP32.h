#pragma once

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <BLE2902.h>
#endif
#include "AudioTools/Concurrency/RTOS.h"
#include "BLESerialStream.h"

namespace audio_tools {

/**
 * @brief A BLE server that implements a serial protocol, so that it
 * can be used to send and receive audio. In BLE terminology this is a
 * Peripheral. This implementation uses the BLE library of the ESP32 Arduino
 * core (Bluedroid or NimBLE).
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BLESerialServerESP32 : public BLESerialServerBase,
                             public BLECharacteristicCallbacks,
                             public BLEServerCallbacks {
 public:
  BLESerialServerESP32(int mtu = 517) : BLESerialServerBase(mtu) {
    rx_buffer.setMutex(&rx_mutex);
    tx_buffer.setMutex(&tx_mutex);
    // the callbacks are executed in the BLE task: so we can wait
    receive_timeout_ms = 1000;
  }

  /// starts a BLE server with the indicated name
  bool begin(const char *name) override {
    TRACEI();
    if (!BLEDevice::init(name)) {
      LOGE("BLESerial: BLEDevice::init failed");
      return false;
    }
    BLEDevice::setMTU(max_transfer_size);
    setupBuffers();

    p_server = BLEDevice::createServer();
    p_server->setCallbacks(this);

    setupBLEService();
    BLEAdvertising *p_advertising = BLEDevice::getAdvertising();
    p_advertising->addServiceUUID(BLE_SERIAL_SERVICE_UUID);
    BLEDevice::startAdvertising();
    return true;
  }

  void end() override {
    TRACEI();
    BLEDevice::deinit();
    p_server = nullptr;
    p_service = nullptr;
    ch01_char = nullptr;
    ch02_char = nullptr;
    info_char = nullptr;
  }

  bool connected() override {
    return p_server != nullptr && p_server->getConnectedCount() > 0;
  }

 protected:
  BLEServer *p_server = nullptr;
  BLEService *p_service = nullptr;
  BLECharacteristic *ch01_char = nullptr;
  BLECharacteristic *ch02_char = nullptr;
  BLECharacteristic *info_char = nullptr;
  BLEDescriptor ch01_desc{"2901"};
  BLEDescriptor ch02_desc{"2901"};
  BLEDescriptor info_desc{"2901"};
  Mutex rx_mutex;
  Mutex tx_mutex;
  Vector<uint8_t> packet{0};

  int getMTU() override {
    int result = max_transfer_size;
    if (connected()) {
      int peer_mtu = p_server->getPeerMTU(p_server->getConnId());
      if (peer_mtu > 0 && peer_mtu < result) result = peer_mtu;
    }
    return result;
  }

  void writeAudioInfo(StrView str) override {
    TRACEI();
    if (info_char == nullptr) return;
    LOGI("AudioInfo: %s", str.c_str());
    info_char->setValue((uint8_t *)str.c_str(), str.length() + 1);
    if (connected()) info_char->notify();
  }

  void setupBLEService() {
    TRACEI();
    // the characteristic properties define what the client can do
    p_service = p_server->createService(BLE_SERIAL_SERVICE_UUID);

    ch01_char = p_service->createCharacteristic(
        BLE_CH1_UUID, BLECharacteristic::PROPERTY_READ);
    ch01_desc.setValue("Channel 1");
    ch01_char->addDescriptor(&ch01_desc);
    ch01_char->setCallbacks(this);

    ch02_char = p_service->createCharacteristic(
        BLE_CH2_UUID, BLECharacteristic::PROPERTY_WRITE |
                          BLECharacteristic::PROPERTY_WRITE_NR);
    ch02_desc.setValue("Channel 2");
    ch02_char->addDescriptor(&ch02_desc);
    ch02_char->setCallbacks(this);

    // optional setup of audio info notifications
    if (is_audio_info_active) {
      info_char = p_service->createCharacteristic(
          BLE_INFO_UUID, BLECharacteristic::PROPERTY_READ |
                             BLECharacteristic::PROPERTY_WRITE |
                             BLECharacteristic::PROPERTY_NOTIFY);
      info_desc.setValue("Audio Info");
      info_char->addDescriptor(&info_desc);
#if defined(CONFIG_BLUEDROID_ENABLED)
      // NimBLE adds the CCCD descriptor automatically
      info_char->addDescriptor(new BLE2902());
#endif
      info_char->setCallbacks(this);
      writeAudioInfo(toStr(audioInfo()));
    }

    p_service->start();
  }

  void onConnect(BLEServer *pServer) override { TRACEI(); }

  void onDisconnect(BLEServer *pServer) override {
    TRACEI();
    onDisconnected();
    BLEDevice::startAdvertising();
  }

  /// store the received data
  void onWrite(BLECharacteristic *pCharacteristic) override {
    TRACED();
    // auto: it changed from std::string to String
    auto value = pCharacteristic->getValue();
    if (pCharacteristic == info_char) {
      receiveAudioInfo((const uint8_t *)value.c_str(), value.length());
    } else if (pCharacteristic == ch02_char) {
      receiveData((const uint8_t *)value.c_str(), value.length());
    }
  }

  /// provide the next batch of data
  void onRead(BLECharacteristic *pCharacteristic) override {
    TRACED();
    if (pCharacteristic != ch01_char) return;
    // the value is limited by the MTU: the ATT read response needs 1 byte
    int max = getMTU() - 1;
    if (packet.size() < (size_t)max) packet.resize(max);
    int len = provideData(packet.data(), max);
    pCharacteristic->setValue(packet.data(), len);
  }
};

}  // namespace audio_tools
