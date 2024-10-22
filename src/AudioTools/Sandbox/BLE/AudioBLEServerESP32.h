#pragma once

#include "AudioBLEStream.h"
#include "ConstantsESP32.h"
//#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {
/**
 * @brief A simple BLE server that implements the serial protocol, so that it
 * can be used to send and recevie audio. In BLE terminologiy this is a Peripheral.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioBLEServer : public AudioBLEStream,
                       public BLECharacteristicCallbacks,
                       public BLEServerCallbacks {
public:
  AudioBLEServer(int mtu = BLE_MTU) : AudioBLEStream(mtu) {}

  // starts a BLE server with the indicated name
  bool begin(const char *name) {
    TRACEI();
    ble_server_name = name;
    BLEDevice::init(name);

    p_server = BLEDevice::createServer();
    p_server->setCallbacks(this);

    setupBLEService();
    p_advertising = BLEDevice::getAdvertising();
    p_advertising->addServiceUUID(BLE_AUDIO_SERVICE_UUID);
    BLEDevice::startAdvertising();
    return true;
  }

  void end() override {
    TRACEI();
    flush();
    BLEDevice::deinit();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    size_t read_size = getReadSize(len);
    return receive_buffer.readArray(data, read_size);
  }

  int available() override {
    if (is_framed)
      return receive_sizes.peek();
    return this->receive_buffer.available();
  }

  size_t write(const uint8_t *data, size_t dataSize) override {
    LOGD("AudioBLEStream::write: %d", dataSize);
    if (!connected()) {
      return 0;
    }
    if (is_framed && availableForWrite() < dataSize) {
      return 0;
    }
    return transmit_buffer.writeArray(data, dataSize);
  }

  int availableForWrite() override {
    int result =  transmit_buffer.availableForWrite();
    // make sure we copy always a consistent amount of data
    if (result < DEFAULT_BUFFER_SIZE) result = 0;
    return result ;
  }

  bool connected() override { return p_server->getConnectedCount() > 0; }

protected:
  // server
  BLEServer *p_server = nullptr;
  BLEService *p_service = nullptr;
  BLEAdvertising *p_advertising = nullptr;
  BLECharacteristic *ch01_char;
  BLECharacteristic *ch02_char;
  BLECharacteristic *info_char;
  BLEDescriptor ch01_desc{"2901"};
  BLEDescriptor ch02_desc{"2901"};
  BLEDescriptor info_desc{"2901"};
  RingBuffer<uint8_t> receive_buffer{0};
  RingBuffer<uint16_t> receive_sizes{0};
  RingBuffer<uint8_t> transmit_buffer{0};
  RingBuffer<uint16_t> transmit_buffer_sizes{0};

  virtual void receiveAudio(const uint8_t *data, size_t size) {
    while (receive_buffer.availableForWrite() < size) {
      // wait for ringbuffer to get freed up
      delay(10);
    }
    if (is_framed)
      receive_sizes.write(size);
    receive_buffer.writeArray(data, size);
  }

  void writeAudioInfoCharacteristic(AudioInfo info) {
    TRACEI();
    // send update via BLE
    StrView str = toStr(info);
    LOGI("AudioInfo: %s", str.c_str());
    info_char->setValue((uint8_t *)str.c_str(), str.length() + 1);
    info_char->notify();
  }

  int getMTU() override {
    TRACED();
    if (max_transfer_size == 0) {
      int peer_max_transfer_size =
          p_server->getPeerMTU(p_server->getConnId()) - BLE_MTU_OVERHEAD;
      max_transfer_size = std::min(BLE_MTU - BLE_MTU, peer_max_transfer_size);

      LOGI("max_transfer_size: %d", max_transfer_size);
    }
    return max_transfer_size;
  }

  void setupBLEService() {
    TRACEI();
    // characteristic property is what the other device does.

    if (p_service == nullptr) {
      p_service = p_server->createService(BLE_AUDIO_SERVICE_UUID);

      ch01_char = p_service->createCharacteristic(
          BLE_CH1_UUID, BLECharacteristic::PROPERTY_READ );
      ch01_desc.setValue("Channel 1");
      ch01_char->addDescriptor(&ch01_desc);
      ch01_char->setCallbacks(this);

      ch02_char = p_service->createCharacteristic(
          BLE_CH2_UUID, BLECharacteristic::PROPERTY_WRITE);
      ch02_desc.setValue("Channel 2");
      ch02_char->addDescriptor(&ch02_desc);
      ch02_char->setCallbacks(this);

      // optional setup of audio info notifications
      if (is_audio_info_active && info_char == nullptr) {

        info_char = p_service->createCharacteristic(
            BLE_INFO_UUID, BLECharacteristic::PROPERTY_NOTIFY |
                               BLECharacteristic::PROPERTY_READ |
                               BLECharacteristic::PROPERTY_NOTIFY |
                               BLECharacteristic::PROPERTY_INDICATE);
        info_desc.setValue("Audio Info");
        info_char->addDescriptor(&info_desc);
        info_char->setCallbacks(this);

      }

      p_service->start();

      getMTU();

      if (info_char != nullptr) {
        writeAudioInfoCharacteristic(info);
      }
    }
  }

  void onConnect(BLEServer *pServer) override {
    TRACEI();
  }

  void onDisconnect(BLEServer *pServer) override {
    TRACEI();
    BLEDevice::startAdvertising();
  }

  /// store the next batch of data
  void onWrite(BLECharacteristic *pCharacteristic) override {
    TRACED();
    setupRXBuffer();
    // changed to auto to be version independent (it changed from std::string to String)
    auto value = pCharacteristic->getValue();
    if (pCharacteristic->getUUID().toString() == BLE_INFO_UUID) {
      setAudioInfo((uint8_t *)&value[0], value.length());
    } else {
      receiveAudio((uint8_t *)&value[0], value.length());
    }
  }

  /// provide the next batch of audio data
  void onRead(BLECharacteristic *pCharacteristic) override {
    TRACED();
    // changed to auto to be version independent (it changed from std::string to String)
    auto uuid = pCharacteristic->getUUID().toString();
    if (uuid == BLE_CH1_UUID || uuid == BLE_CH2_UUID) {
      setupTXBuffer();
      int len = std::min(getMTU() - BLE_MTU_OVERHEAD, (int)transmit_buffer.available());
      if (is_framed) {
        len = transmit_buffer_sizes.read();
      }
      LOGD("%s: len: %d, buffer: %d", uuid.c_str(), len,
           transmit_buffer.size());
      uint8_t tmp[len];
      transmit_buffer.readArray(tmp, len);
      pCharacteristic->setValue(tmp, len);
    }
  }

  void setupTXBuffer() {
    if (transmit_buffer.size() == 0) {
      LOGI("Setting transmit_buffer to %d for mtu %d", RX_BUFFER_SIZE, getMTU());
      transmit_buffer.resize(TX_BUFFER_SIZE);
      if (is_framed) {
        transmit_buffer_sizes.resize(TX_COUNT);
      }
    }
  }

  void setupRXBuffer() {
    if (receive_buffer.size() == 0) {
      LOGI("Setting receive_buffer to %d for mtu %d", RX_BUFFER_SIZE, getMTU());
      receive_buffer.resize(RX_BUFFER_SIZE);
      if (is_framed) {
        receive_sizes.resize(RX_COUNT);
      }
    }
  }

  size_t getReadSize(size_t dataSize) {
    size_t read_size = dataSize;
    if (is_framed) {
      read_size = 0;
      if (receive_sizes.available() > 0) {
        read_size = receive_sizes.read();
      }
      if (dataSize < read_size) {
        LOGE("read size too small: %d - it must be >= %d", dataSize, read_size);
        return 0;
      }
      if (receive_buffer.available() < read_size) {
        LOGE("missing data in buffer");
        return 0;
      }
    }
    return read_size;
  }
};


} // namespace audio_tools