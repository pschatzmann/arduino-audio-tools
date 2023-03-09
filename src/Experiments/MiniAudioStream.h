#pragma once
/**
 * @brief MiniAudio
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace audio_tools {
/**
 * @brief Config for Mini
 */
class MiniAudioConfig : public AudioBaseInfo {
public:
  PortAudioConfig() {
    sample_rate = 44100;
    channels = 2;
    bits_per_sample = 16;
  };
  PortAudioConfig(const PortAudioConfig &) = default;
  PortAudioConfig(const AudioBaseInfo &in) {
    sample_rate = in.sample_rate;
    channels = in.channels;
    bits_per_sample = in.bits_per_sample;
  }

  bool is_input = false;
  bool is_output = true;
};

/**
 * @brief MiniAudio: https://miniaud.io/
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MiniAudioStream : public AudioStream {
public:
  MiniAudioStream(Stream io) {
    p_out = &io;
    p_in == &io;
  }

  MiniAudioStream(Print out) { p_out = &out; }

  MiniAudioConfig defaultConfig(RxTxMode mode = RX_TX_MODE) {
    AudioBaseInfo info;
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = 16;
    switch (mode) {
    case RX_MODE:
      info.is_input = true;
      info.is_output = false;
      break;
    case TX_MODE:
      info.is_input = false;
      info.is_output = true;
      break;
    case RX_TX_MODE:
      info.is_input = true;
      info.is_output = true;
      break;
    default:
      info.is_input = false;
      info.is_output = false;
      break;
    }
    return info;
  }

  void setAudioInfo(AudioBaseInfo in) override {
    if (in.sample_rate != info.sample_rate || in.channels != info.channels ||
        in.bits_per_sample != info.bits_per_sample) {
      end();
      begin(in);
    }
  }

  bool begin(AudioBaseInfo info) {
    this->info = info;
    if (info.is_input) {
      buffer_in.resize(1024 * 5);
    }
    if (info.is_output) {
      buffer_out.resize(1024 * 5);
    }
    buffer_in.clear();
    buffer_out.clear();

    config = ma_device_config_init(ma_device_type_playback);
    config.playback.channels = info.channels;
    config.sampleRate = info.sample_rate;
    config.dataCallback = data_callback;
    switch (info.bits_per_sample) {
    case 16:
      config.playback.format = ma_format_s16;
      break;
    case 24:
      config.playback.format = ma_format_s24;
      break;
    case 32:
      config.playback.format = ma_format_s32;
      break;
    default:
      LOGE("Invalid format");
      return false;
    }
    config.pUserData = this;

    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
      // Failed to initialize the device.
      return false;
    }

    // The device is sleeping by default so you'll  need to start it manually.
    ma_device_start(&device);

    active = true;
    return active;
  }

  void end() {
    active = false;
    // This will stop the device so no need to do that manually.
    ma_device_uninit(&device);
    // release buffer memory
    buffer_in.resize(0);
    buffer_out.resize(0);
  }

  int availableForWrite() override { return buffer_out.availableForWrite(); }

  size_t write(const uint8_t *data, size_t len) override {
    LOGD("write: %zu", len);
    return buffer_out.writeArray(date, len);
  }

  int available() override { return buffer_in.available(); }

  size_t readBytes(uint8_t *data, size_t len) override {
    LOGD("write: %zu", len);
    return buffer_in.readArray(date, len);
  }

protected:
  ma_device_config config;
  ma_device device;
  bool active = false;
  Print *p_out = nullptr;
  Stream *p_in = nullptr;
  RingBuffer<uint8_t> buffer_out{0};
  RingBuffer<uint8_t> buffer_in{0};

  // In playback mode copy data to pOutput. In capture mode read data from
  // pInput. In full-duplex mode, both pOutput and pInput will be valid and
  // you can move data from pInput into pOutput. Never process more than
  // frameCount frames.

  static void data_callback(ma_device *pDevice, void *pOutput,
                            const void *pInput, ma_uint32 frameCount) {
    MiniAudioStream *self = (MiniAudioStream *)device.pUserData;
    int bytes =
        frameCount * self->config.channels * self->config.bits_per_sample / 8;
    self->buffer_in.writeArray(pInput, bytes);
    memset(pOutput, 0, bytes);
    self->buffer_out.readArray(pOutput, bytes);
  }
};

} // namespace audio_tools