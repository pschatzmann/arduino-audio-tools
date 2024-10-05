#pragma once
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "PDM.h"

namespace audio_tools {

/**
 * @brief Config for MP34DT05 Microphone. Supported sample rates 16000, 41667,
 * Supported bits_per_sample only 16
 *
 */
struct AudioMP34DT05Config : public AudioInfo {
  AudioMP34DT05Config() {
    channels = 1;
    sample_rate = 16000;
    bits_per_sample = 16;
  }
  int gain = 20;  // value of DEFAULT_PDM_GAIN
  int buffer_size = 512;
  int buffer_count = 2;
  // define pins
  // int pin_data = PIN_PDM_DIN;
  // int pin_clk = PIN_PDM_CLK;
  // int pin_pwr = PIN_PDM_PWR;
  void logInfo() {
    AudioInfo::logInfo();
    LOGI("gain: %d", gain);
    LOGI("buffer_size: %d", buffer_size);
  }
};

class AudioMP34DT05 *selfAudioMP34DT05 = nullptr;

/**
 * @brief MP34DT05 Microphone of Nano BLE Sense. We provide a proper Stream
 * implementation. See https://github.com/arduino/ArduinoCore-nRF528x-mbedos
 * @ingroup io
 */
class AudioMP34DT05 : public AudioStream {
 public:
  AudioMP34DT05() { selfAudioMP34DT05 = this; };
  virtual ~AudioMP34DT05() {
    if (p_buffer != nullptr) delete p_buffer;
  };

  AudioMP34DT05Config defaultConfig(int mode = RX_MODE) {
    AudioMP34DT05Config cfg;
    if (mode != RX_MODE) {
      LOGE("TX_MODE is not supported");
    }
    return cfg;
  }

  bool begin() { return begin(config); }

  bool begin(AudioMP34DT05Config cfg) {
    TRACEI();
    config = cfg;
    cfg.logInfo();
    if (p_buffer == nullptr) {
      p_buffer = new NBuffer<uint8_t>(cfg.buffer_size, cfg.buffer_count);
    }
    p_mic->setBufferSize(cfg.buffer_size);
    p_mic->onReceive(onReceiveStatic);
    LOGD("begin(%d,%d)", cfg.channels, cfg.sample_rate);
    bool result = p_mic->begin(cfg.channels, cfg.sample_rate);
    if (!result) {
      LOGE("begin(%d,%d)", cfg.channels, cfg.sample_rate);
    }
    LOGD("setGain: %d", cfg.gain);
    p_mic->setGain(cfg.gain);
    return result;
  }

  void end() {
    TRACEI();
    if (p_mic != nullptr) {
      p_mic->end();
    }

    delete p_buffer;
    p_buffer=nullptr;
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    if (p_buffer == nullptr) return 0;
    return p_buffer->readArray(data, len);
  }

  int available() override {
    if (p_buffer == nullptr) return 0;
    return p_buffer->available();
  }

 protected:
  PDMClass *p_mic = &PDM;
  NBuffer<uint8_t> *p_buffer = nullptr;
  AudioMP34DT05Config config;

  /// for some strange reasons available provides only the right result after
  /// onReceive, so unfortunately we need to use an additional buffer
  void onReceive() {
    int bytesAvailable = p_mic->available();
    // Read into the sample buffer
    uint8_t sampleBuffer[bytesAvailable]={0};
    int read = PDM.read(sampleBuffer, bytesAvailable);
    p_buffer->writeArray(sampleBuffer, read);
  }

  static void onReceiveStatic() { selfAudioMP34DT05->onReceive(); }
};

} // namespace