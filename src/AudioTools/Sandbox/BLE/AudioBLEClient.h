#pragma once

#include "AudioBLEStream.h"
#include "ConstantsArduino.h"
#include <ArduinoBLE.h>

namespace audio_tools {

class AudioBLEClient;
static AudioBLEClient *selfAudioBLEClient = nullptr;

/**
 * @brief A simple BLE client that implements the serial protocol, so that it
 * can be used to send and recevie audio. In BLE terminology this is a Central
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioBLEClient : public AudioBLEStream {
public:
  AudioBLEClient(int mtu = BLE_MTU) : AudioBLEStream(mtu) {
    selfAudioBLEClient = this;
    max_transfer_size = mtu;
  }

  /// starts a BLE client
  bool begin(const char *localName, int seconds) {
    TRACEI();
    // Init BLE device
    BLE.begin();
    BLE.setLocalName(localName);
    BLE.scanForUuid(BLE_AUDIO_SERVICE_UUID);
    return true;
  }

  void end() override {
    TRACEI();
    flush();
    BLE.end();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    if (!setupBLEClient()) {
      return 0;
    }

    if (!ch01_char.canRead())
      return 0;

    return ch01_char.readValue(data, len);
  }

  int available() override { return BLE_MTU - BLE_MTU_OVERHEAD; }

  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    if (!setupBLEClient()) {
      return 0;
    }

    if (!ch02_char.canWrite()) {
      return 0;
    }

    if (is_framed) {
      writeChannel2Characteristic(data, len);
      delay(1);
    } else {
      // send only data with max mtu
      for (int j = 0; j < len; j++) {
        write_buffer.write(data[j]);
        if (write_buffer.isFull()) {
          writeChannel2Characteristic(write_buffer.data(),
                                      write_buffer.available());
          write_buffer.reset();
        }
      }
    }
    return len;
  }

  int availableForWrite() override {
    return is_framed ? (BLE_MTU - BLE_MTU_OVERHEAD) : DEFAULT_BUFFER_SIZE;
  }

  bool connected() override { return setupBLEClient(); }

  void setWriteThrottle(int ms) { write_throttle = ms; }

  void setConfirmWrite(bool flag) { write_confirmation_flag = flag; }

protected:
  BLEDevice peripheral;
  BLECharacteristic ch01_char;
  BLECharacteristic ch02_char;
  BLECharacteristic info_char;
  SingleBuffer<uint8_t> write_buffer{0};
  int write_throttle = 0;
  bool write_confirmation_flag = false;

  void writeAudioInfoCharacteristic(AudioInfo info) override {
    TRACEI();
    // send update via BLE
    info_char.writeValue((uint8_t *)&info, sizeof(AudioInfo));
  }

  void writeChannel2Characteristic(const uint8_t *data, size_t len) {
    if (ch02_char.canWrite()) {
      ch02_char.writeValue((uint8_t *)data, len, write_confirmation_flag);
      delay(write_throttle);
    }
  }

  bool readAudioInfoCharacteristic() {
    if (!info_char.canRead())
      return false;
    const uint8_t *str = info_char.value();
    int len = info_char.valueLength();
    if (len > 0) {
      setAudioInfo(str, len);
      return true;
    }
    return false;
  }

  static void onInfoUpdated(BLEDevice central,
                            BLECharacteristic characteristic) {
    selfAudioBLEClient->setAudioInfo(characteristic.value(),
                                     characteristic.valueLength());
  }

  bool setupBLEClient() {
    // if we are already connected there is nothing to do
    if (peripheral.connected()) {
      return true;
    }

    TRACEI();

    // setup buffer
    if (write_buffer.size() == 0) {
      write_buffer.resize(getMTU() - BLE_MTU_OVERHEAD);
    }

    peripheral = BLE.available();
    if (!peripheral) {
      return false;
    }

    BLE.stopScan();

    if (!peripheral.connect()) {
      return false;
    }

    if (peripheral.discoverAttributes()) {
      LOGI("Attributes discovered");
    } else {
      LOGE("Attribute discovery failed!");
      peripheral.disconnect();
      return false;
    }

    ch01_char = peripheral.characteristic(BLE_CH1_UUID);
    if (!ch01_char) {
      peripheral.disconnect();
      return false;
    }

    ch02_char = peripheral.characteristic(BLE_CH2_UUID);
    if (!ch02_char) {
      peripheral.disconnect();
      return false;
    }

    if (is_audio_info_active) {
      info_char = peripheral.characteristic(BLE_INFO_UUID);
      info_char.setEventHandler(BLEUpdated, onInfoUpdated);
    }

    return true;
  }
};

} // namespace audio_tools