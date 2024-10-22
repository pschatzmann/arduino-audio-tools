#pragma once

#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"


namespace audio_tools {

/**
 * @ingroup main
 * @brief Transmit and receive data via BLE using a Serial API.
 * The following additional experimental features are offered:
 * setFramed(true) tries to keep the original write sizes;
 * setAudioInfoActive(true) informs about changes in the audio info
 */

class AudioBLEStream : public AudioStream {
public:
  AudioBLEStream(int defaultMTU) { max_transfer_size = defaultMTU; };

  virtual void end() = 0;

  virtual bool connected() = 0;

  void setAudioInfo(AudioInfo info) {
    if (is_audio_info_active && this->info != info) {
      TRACED();
      AudioStream::setAudioInfo(info);
      writeAudioInfoCharacteristic(info);
    }
  }

  operator bool() { return connected(); }

  void setServiceUUID(const char *uuid) { BLE_AUDIO_SERVICE_UUID = uuid; }

  void setRxUUID(const char *uuid) { BLE_CH2_UUID = uuid; }

  void setTxUUID(const char *uuid) { BLE_CH1_UUID = uuid; }

  void setAudioInfoUUID(const char *uuid) { BLE_INFO_UUID = uuid; }

  void setAudioInfoActive(bool flag) { is_audio_info_active = flag; }

  void setFramed(bool flag) { is_framed = flag; }

  StrView toStr(AudioInfo info) {
    snprintf(audio_info_str, 40, "%d:%d:%d", info.sample_rate, info.channels,
            info.bits_per_sample);
    return StrView(audio_info_str);
  }

  AudioInfo toInfo(const uint8_t *str) {
    AudioInfo result;
    sscanf((char*)str,"%d:%d:%d", &result.sample_rate, &result.channels, &result.bits_per_sample);
    return result;
  }

protected:
  // disable copy constructor
  AudioBLEStream(AudioBLEStream const &other) = delete;
  // disable assign constructor
  void operator=(AudioBLEStream const &other) = delete;
  const char *ble_server_name = nullptr;
  uint16_t max_transfer_size = 0;
  bool is_started = false;
  bool is_audio_info_active = false;
  bool is_framed = false;
  char audio_info_str[40];

  // Bluetooth LE GATT UUIDs for the Nordic UART profile Change UUID here if
  // required
  const char *BLE_AUDIO_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  const char *BLE_CH1_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // RX
  const char *BLE_CH2_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // TX
  const char *BLE_INFO_UUID = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";

  virtual int getMTU() = 0;

  // override to implement your own extended logic
  virtual void setAudioInfo(const uint8_t *data, size_t size) {
    if (is_audio_info_active) {
      AudioInfo ai = toInfo(data);
      setAudioInfo(ai);
    }
  }
  // override to implement your own extended logic
  virtual void writeAudioInfoCharacteristic(AudioInfo info) = 0;
};

} // namespace audio_tools