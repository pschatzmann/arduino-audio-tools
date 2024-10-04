#pragma once

#include "AudioBLEStream.h"
#include "ConstantsESP32.h"
//#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

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

class AudioBLEClient : public AudioBLEStream,
                       public BLEClientCallbacks,
                       public BLEAdvertisedDeviceCallbacks {
public:
  AudioBLEClient(int mtu = BLE_MTU) : AudioBLEStream(mtu) {
    selfAudioBLEClient = this;
    max_transfer_size = mtu;
  }

  /// starts a BLE client
  bool begin(const char *localName, int seconds) {
    TRACEI();
    // Init BLE device
    BLEDevice::init(localName);

    // Retrieve a Scanner and set the callback we want to use to be informed
    // when we have detected a new device.
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(this);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(seconds);
    return true;
  }

  void end() override {
    TRACEI();
    flush();
    BLEDevice::deinit();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    setupBLEClient();
    if (!is_client_connected || !is_client_set_up)
      return 0;
    if (!ch01_char->canRead())
      return 0;
    // changed to auto to be version independent (it changed from std::string to
    // String)
    auto str = ch01_char->readValue();
    if (str.length() > 0) {
      memcpy(data, str.c_str(), str.length());
    }
    return str.length();
  }

  int available() override { return BLE_MTU - BLE_MTU_OVERHEAD; }

  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    setupBLEClient();
    if (!is_client_connected || !is_client_set_up)
      return 0;
    if (!ch02_char->canWrite()){
      return 0;
    }

    if (is_framed){
      writeChannel2Characteristic(data, len);
      delay(1);
    } else {
      // send only data with max mtu
      for (int j=0; j<len; j++){
        write_buffer.write(data[j]);
        if (write_buffer.isFull()){
          writeChannel2Characteristic(write_buffer.data(), write_buffer.available());
          write_buffer.reset();
        }
      }
    }
    return len;
  }

  int availableForWrite() override { 
    return is_framed ? (BLE_MTU - BLE_MTU_OVERHEAD) : DEFAULT_BUFFER_SIZE; 
  }

  bool connected() override {
    if (!setupBLEClient()) {
      LOGE("setupBLEClient failed");
    }
    return is_client_connected;
  }

  void setWriteThrottle(int ms){
    write_throttle = ms;
  }

  void setConfirmWrite(bool flag){
    write_confirmation_flag = flag;
  }

protected:
  // client
  BLEClient *p_client = nullptr;
  BLEAdvertising *p_advertising = nullptr;
  BLERemoteService *p_remote_service = nullptr;
  BLEAddress *p_server_address = nullptr;
  BLERemoteCharacteristic *ch01_char = nullptr; // read
  BLERemoteCharacteristic *ch02_char = nullptr; // write
  BLERemoteCharacteristic *info_char = nullptr;
  BLEAdvertisedDevice advertised_device;
  BLEUUID BLUEID_AUDIO_SERVICE_UUID{BLE_AUDIO_SERVICE_UUID};
  BLEUUID BLUEID_CH1_UUID{BLE_CH1_UUID};
  BLEUUID BLUEID_CH2_UUID{BLE_CH2_UUID};
  BLEUUID BLUEID_INFO_UUID{BLE_INFO_UUID};
  SingleBuffer<uint8_t> write_buffer{0};
  int write_throttle = 0;
  bool write_confirmation_flag = false;

  volatile bool is_client_connected = false;
  bool is_client_set_up = false;

  void onConnect(BLEClient *pClient) override {
    TRACEI();
    is_client_connected = true;
  }

  void onDisconnect(BLEClient *pClient) override {
    TRACEI();
    is_client_connected = false;
  };

  void writeAudioInfoCharacteristic(AudioInfo info) override {
    TRACEI();
    // send update via BLE
    info_char->writeValue((uint8_t *)&info, sizeof(AudioInfo));
  }

  void writeChannel2Characteristic(const uint8_t*data, size_t len){
    if (ch02_char->canWrite()) {
      ch02_char->writeValue((uint8_t *)data, len, write_confirmation_flag);
      delay(write_throttle);
    }
  }

  bool readAudioInfoCharacteristic(){
    if (!info_char->canRead())
      return false;
    auto str = info_char->readValue();
    if (str.length() > 0) {
      setAudioInfo((const uint8_t*)str.c_str(), str.length());
      return true;
    }
    return false;
  }

  // Scanning Results
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    TRACEI();
    // Check if the name of the advertiser matches
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLUEID_AUDIO_SERVICE_UUID)) {
      LOGI("Service '%s' found!", BLE_AUDIO_SERVICE_UUID);
      // save advertised_device in class variable
      advertised_device = advertisedDevice;
      // Scan can be stopped, we found what we are looking for
      advertised_device.getScan()->stop();
    }
    delay(10);
  }

  static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic,
                             uint8_t *pData, size_t length, bool isNotify) {
    TRACEI();
    if (pBLERemoteCharacteristic->getUUID().toString() ==
        selfAudioBLEClient->BLE_INFO_UUID) {
      selfAudioBLEClient->setAudioInfo(pData, length);
    }
  }

  bool setupBLEClient() {
    if (is_client_set_up)
      return true;

    TRACEI();

    // setup buffer
    if (write_buffer.size()==0){
      write_buffer.resize(getMTU() - BLE_MTU_OVERHEAD);
    }

    if (p_client == nullptr)
      p_client = BLEDevice::createClient();

    // onConnect and on onDisconnect support
    p_client->setClientCallbacks(this);

    // Connect to the remove BLE Server.
    LOGI("Connecting to %s ...",
         advertised_device.getAddress().toString().c_str());
    // p_client->connect(advertised_device.getAddress(),BLE_ADDR_TYPE_RANDOM);
    p_client->connect(&advertised_device);
    if (!p_client->isConnected()) {
      LOGE("Connect failed");
      return false;
    }
    LOGI("Connected to %s ...",
         advertised_device.getAddress().toString().c_str());

    LOGI("Setting mtu to %d", max_transfer_size);
    assert(max_transfer_size > 0);
    p_client->setMTU(max_transfer_size);

    // Obtain a reference to the service we are after in the remote BLE
    // server.
    if (p_remote_service == nullptr) {
      p_remote_service = p_client->getService(BLUEID_AUDIO_SERVICE_UUID);
      if (p_remote_service == nullptr) {
        LOGE("Failed to find our service UUID: %s", BLE_AUDIO_SERVICE_UUID);
        return (false);
      }
    }

    if (ch01_char == nullptr) {
      ch01_char = p_remote_service->getCharacteristic(BLUEID_CH1_UUID);
      if (ch01_char == nullptr) {
        LOGE("Failed to find char. UUID: %s", BLE_CH1_UUID);
        return false;
      }
    }

    if (ch02_char == nullptr) {
      ch02_char = p_remote_service->getCharacteristic(BLUEID_CH2_UUID);
      if (ch02_char == nullptr) {
        LOGE("Failed to find char. UUID: %s", BLE_CH2_UUID);
        return false;
      }
    }

    if (is_audio_info_active && info_char == nullptr) {
      info_char = p_remote_service->getCharacteristic(BLUEID_INFO_UUID);
      if (info_char == nullptr) {
        LOGE("Failed to find char. UUID: %s", BLE_INFO_UUID);
        return false;
      }
      info_char->registerForNotify(notifyCallback);
      readAudioInfoCharacteristic();

    }
    LOGI("Connected to server: %s", is_client_connected ? "true" : "false");
    is_client_set_up = true;
    is_client_connected = true;
    return is_client_connected;
  }

  int getMTU() override { return BLE_MTU; }


};

} // namespace audio_tools