#pragma once

#include "AudioBLEStream.h"
#include "ConstantsArduino.h"
#include <ArduinoBLE.h>
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {

class AudioBLEServer;
class AudioBLEServer *selfAudioBLEServer = nullptr;

/**
 * @brief A simple BLE server that implements the serial protocol, so that it
 * can be used to send and recevie audio. In BLE terminologiy this is a
 * Peripheral.
 * This implementation uses the ArduinoBLE library!
 * This is working only correctly if the client sets the max MTU to a value >= 256.
 * Otherwise some of the transmitted information gets silently dropped
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioBLEServer : public AudioBLEStream {
public:
  AudioBLEServer(int mtu = 0) : AudioBLEStream(mtu) {
    selfAudioBLEServer = this;
  }

  // starts a BLE server with the indicated name
  bool begin(const char *name) {
    TRACEI();
    ble_server_name = name;

    if (!BLE.begin()) {
      LOGE("starting BLE failed");
      return false;
    }

    // set the local name peripheral advertises
    BLE.setLocalName(ble_server_name);

    // assign event handlers for connected, disconnected to peripheral
    BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
    BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

    // setup serice with characteristics
    setupBLEService();

    // start advertising
    BLE.advertise();

    return true;
  }

  void end() override {
    TRACEI();
    flush();
    BLE.end();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    if (!checkCentralConnected())
      return 0;
    size_t read_size = getReadSize(len);
    return receive_buffer.readArray(data, read_size);
  }

  int available() override {
    if (!checkCentralConnected())
      return 0;
    if (is_framed)
      return receive_sizes.peek();
    return this->receive_buffer.available();
  }

  size_t write(const uint8_t *data, size_t len) override {
    LOGD("AudioBLEStream::write: %d", len);
    if (!checkCentralConnected())
      return 0;
    if (is_framed && availableForWrite() < len) {
      return 0;
    }
    return transmit_buffer.writeArray(data, len);
  }

  int availableForWrite() override {
    if (!checkCentralConnected())
      return 0;
    setupTXBuffer();
    int result = transmit_buffer.availableForWrite();
    // make sure we copy always a consistent amount of data
    if (result < DEFAULT_BUFFER_SIZE)
      result = 0;
    return result;
  }

  bool connected() override { return checkCentralConnected(); }

protected:
  // server
  BLEDevice central;
  BLEService service{BLE_AUDIO_SERVICE_UUID}; // create service
  BLECharacteristic ch01_char{BLE_CH1_UUID, BLERead, getMTU()};
  BLECharacteristic ch02_char{BLE_CH2_UUID, BLEWrite, getMTU()};
  BLECharacteristic info_char{BLE_INFO_UUID, BLERead | BLEWrite | BLENotify,
                              80};
  BLEDescriptor ch01_desc{"2901", "channel 1"};
  BLEDescriptor ch02_desc{"2901", "channel 2"};
  BLEDescriptor info_desc{"2901", "info"};

  RingBuffer<uint8_t> receive_buffer{0};
  RingBuffer<uint16_t> receive_sizes{0};
  RingBuffer<uint8_t> transmit_buffer{0};
  RingBuffer<uint16_t> transmit_buffer_sizes{0};

  static void blePeripheralConnectHandler(BLEDevice device) {
    selfAudioBLEServer->onConnect(device);
  }

  static void blePeripheralDisconnectHandler(BLEDevice device) {
    selfAudioBLEServer->onDisconnect(device);
  }

  static void bleOnWrite(BLEDevice device, BLECharacteristic characteristic) {
    selfAudioBLEServer->onWrite(characteristic);
  }

  static void bleOnRead(BLEDevice device, BLECharacteristic characteristic) {
    TRACED();
    selfAudioBLEServer->onRead(characteristic);
  }

  void onConnect(BLEDevice device) { TRACEI(); }

  void onDisconnect(BLEDevice device) {
    TRACEI();
    BLE.advertise();
  }

  /// store the next batch of data
  void onWrite(BLECharacteristic characteristic) {
    TRACED();
    setupRXBuffer();
    // changed to auto to be version independent (it changed from std::string to
    // String)
    if (StrView(BLE_INFO_UUID).equals(characteristic.uuid())) {
      setAudioInfo((uint8_t *)characteristic.value(),
                   characteristic.valueLength());
    } else {
      receiveAudio((uint8_t *)characteristic.value(),
                   characteristic.valueLength());
    }
  }

  /// provide the next batch of audio data
  void onRead(BLECharacteristic characteristic) {
    TRACED();
    auto uuid = StrView(characteristic.uuid());
    if (uuid == BLE_CH1_UUID || uuid == BLE_CH2_UUID) {
      TRACEI();
      int len = std::min(getMTU(), (int)transmit_buffer.available());
      if (is_framed) {
        len = transmit_buffer_sizes.read();
      }
      LOGI("%s: len: %d, buffer: %d", uuid.c_str(), len,
           transmit_buffer.size());
      if (len > 0) {
        uint8_t tmp[len];
        transmit_buffer.peekArray(tmp, len);
        if (characteristic.writeValue(tmp, len)) {
          transmit_buffer.readArray(tmp, len);
        } else {
          LOGW("writeValue failed")
        }
      }
    }
  }

  bool checkCentralConnected() {
    central = BLE.central();
    // if a central is connected to the peripheral:
    if (central)
      return central.connected();
    return false;
  }

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
    info_char.setValue((uint8_t *)str.c_str(), str.length() + 1);
  }

  int getMTU() override {
    TRACED();
    if (max_transfer_size == 0) {
      // int peer_max_transfer_size =
      //     p_server->getPeerMTU(p_server->getConnId()) - BLE_MTU_OVERHEAD;
      // max_transfer_size = std::min(BLE_MTU - BLE_MTU,
      // peer_max_transfer_size);
      // max_transfer_size = central.mtu() - BLE_MTU_OVERHEAD;
      max_transfer_size = BLE_MTU - BLE_MTU_OVERHEAD;

      LOGI("max_transfer_size: %d", max_transfer_size);
    }
    return max_transfer_size;
  }

  void setupBLEService() {
    TRACEI();
    // set the UUID for the service this peripheral advertises
    BLE.setAdvertisedService(service);

    ch01_char.addDescriptor(ch01_desc);
    ch02_char.addDescriptor(ch02_desc);

    // add the characteristic to the service
    service.addCharacteristic(ch01_char);
    service.addCharacteristic(ch02_char);

    // assign event handlers for characteristic
    ch02_char.setEventHandler(BLEWritten, bleOnWrite);
    ch01_char.setEventHandler(BLERead, bleOnRead);

    if (is_audio_info_active) {
      info_char.addDescriptor(info_desc);
      service.addCharacteristic(info_char);
    }

    // add service
    BLE.addService(service);

    // provide AudioInfo
    if (is_audio_info_active) {
      writeAudioInfoCharacteristic(info);
    }

    // Read callback works only when we provide some initial data
    uint8_t tmp[512] = {0xFF};
    ch01_char.writeValue(tmp, 512, false);
  }

  void setupTXBuffer() {
    if (transmit_buffer.size() == 0) {
      LOGI("Setting transmit_buffer to %d", RX_BUFFER_SIZE);
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