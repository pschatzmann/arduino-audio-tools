#pragma once
/**
 * @brief MiniAudio
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include <mutex>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define MA_BUFFER_COUNT 100
#define MA_START_COUNT MA_BUFFER_COUNT - 2
#define MA_DELAY 10

namespace audio_tools {

/**
 * @brief Configuration for MiniAudio
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MiniAudioConfig : public AudioInfo {
 public:
  MiniAudioConfig() {
    sample_rate = 44100;
    channels = 2;
    bits_per_sample = 16;
  };
  MiniAudioConfig(const MiniAudioConfig &) = default;
  MiniAudioConfig(const AudioInfo &in) {
    sample_rate = in.sample_rate;
    channels = in.channels;
    bits_per_sample = in.bits_per_sample;
  }

  bool is_input = false;
  bool is_output = true;
  int delay_ms_if_buffer_full = MA_DELAY;
  int buffer_count = MA_BUFFER_COUNT;
  int buffer_start_count = MA_START_COUNT;
};

/**
 * @brief MiniAudio: https://miniaud.io/
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MiniAudioStream : public AudioStream {
 public:
  MiniAudioStream() = default;
  ~MiniAudioStream() { end(); };

  MiniAudioConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    MiniAudioConfig info;
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
      case RXTX_MODE:
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

  void setAudioInfo(AudioInfo in) override {
    AudioStream::setAudioInfo(in);
    if (in.sample_rate != config.sample_rate ||
        in.channels != config.channels ||
        in.bits_per_sample != config.bits_per_sample) {
      config.copyFrom(in);
      if (is_active) {
        is_active = false;
        is_playing = false;
        // This will stop the device so no need to do that manually.
        ma_device_uninit(&device_ma);

        begin();
      }
    }
  }

  bool begin(MiniAudioConfig info) {
    AudioStream::setAudioInfo(info);
    this->config = info;
    return begin();
  }

  bool begin() override {
    TRACEI();
    if (config.is_output && !config.is_input)
      config_ma = ma_device_config_init(ma_device_type_playback);
    else if (!config.is_output && config.is_input)
      config_ma = ma_device_config_init(ma_device_type_capture);
    else if (config.is_output && config.is_input)
      config_ma = ma_device_config_init(ma_device_type_duplex);
    else if (!config.is_output && !config.is_input)
      config_ma = ma_device_config_init(ma_device_type_loopback);

    config_ma.pUserData = this;
    config_ma.playback.channels = config.channels;
    config_ma.sampleRate = config.sample_rate;
    config_ma.dataCallback = data_callback;
    switch (config.bits_per_sample) {
      case 8:
        config_ma.playback.format = ma_format_u8;
        break;
      case 16:
        config_ma.playback.format = ma_format_s16;
        break;
      case 24:
        config_ma.playback.format = ma_format_s24;
        break;
      case 32:
        config_ma.playback.format = ma_format_s32;
        break;
      default:
        LOGE("Invalid format");
        return false;
    }

    if (ma_device_init(NULL, &config_ma, &device_ma) != MA_SUCCESS) {
      // Failed to initialize the device.
      return false;
    }

    // The device is sleeping by default so you'll  need to start it manually.
    if (ma_device_start(&device_ma) != MA_SUCCESS) {
      // Failed to initialize the device.
      return false;
    }

    is_active = true;
    return is_active;
  }

  void end() override {
    is_active = false;
    is_playing = false;
    // This will stop the device so no need to do that manually.
    ma_device_uninit(&device_ma);
    // release buffer memory
    buffer_in.resize(0);
    buffer_out.resize(0);
  }

  int availableForWrite() override {
    return buffer_out.size() == 0 ? 0 : DEFAULT_BUFFER_SIZE;
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (buffer_out.size() == 0) return 0;
    LOGD("write: %zu", len);

    // write data to buffer
    int open = len;
    int written = 0;
    while (open > 0) {
      size_t result = 0;
      {
        std::lock_guard<std::mutex> guard(write_mtx);
        result = buffer_out.writeArray(data + written, open);
        open -= result;
        written += result;
      }

      if (result != len) doWait();
    }

    // activate playing
    // if (!is_playing && buffer_out.bufferCountFilled()>=MA_START_COUNT) {
    if (!is_playing && buffer_out.available() >= config.buffer_start_count * buffer_size) {
      LOGI("starting audio");
      is_playing = true;
    }
    // std::this_thread::yield();
    return written;
  }

  int available() override {
    return buffer_in.size() == 0 ? 0 : buffer_in.available();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    if (buffer_in.size() == 0) return 0;
    LOGD("write: %zu", len);
    std::lock_guard<std::mutex> guard(read_mtx);
    return buffer_in.readArray(data, len);
  }

 protected:
  MiniAudioConfig config;
  ma_device_config config_ma;
  ma_device device_ma;
  bool is_playing = false;
  bool is_active = false;
  bool is_buffers_setup = false;
  RingBuffer<uint8_t> buffer_out{0};
  RingBuffer<uint8_t> buffer_in{0};
  std::mutex write_mtx;
  std::mutex read_mtx;
  int buffer_size = 0;

  // In playback mode copy data to pOutput. In capture mode read data from
  // pInput. In full-duplex mode, both pOutput and pInput will be valid and
  // you can move data from pInput into pOutput. Never process more than
  // frameCount frames.

  void setupBuffers(int size) {
    if (is_buffers_setup) return;
    buffer_size = size;
    int buffer_count = config.buffer_count;
    LOGI("setupBuffers: %d * %d", size, buffer_count);
    if (buffer_out.size() == 0 && config.is_output)
      buffer_out.resize(size * buffer_count);
    if (buffer_in.size() == 0 && config.is_input)
      buffer_in.resize(size * buffer_count);
    is_buffers_setup = true;
  }

  void doWait() {
    //std::this_thread::yield();
    delay(config.delay_ms_if_buffer_full);
    //std::this_thread::sleep_for (std::chrono::milliseconds(MA_DELAY));
  }

  static void data_callback(ma_device *pDevice, void *pOutput,
                            const void *pInput, ma_uint32 frameCount) {
    MiniAudioStream *self = (MiniAudioStream *)pDevice->pUserData;
    AudioInfo cfg = self->audioInfo();

    int bytes = frameCount * cfg.channels * cfg.bits_per_sample / 8;
    self->setupBuffers(bytes);

    if (pInput) {
      int open = bytes;
      int processed = 0;
      while (open > 0) {
        int len = 0;
        {
          std::unique_lock<mutex> guard(self->read_mtx);
          int len =
              self->buffer_in.writeArray((uint8_t *)pInput + processed, open);
          open -= len;
          processed += len;
        }
        if (len == 0) self->doWait();
      }
    }

    if (pOutput) {
      memset(pOutput, 0, bytes);
      if (self->is_playing) {
        int open = bytes;
        int processed = 0;
        while (open > 0) {
          size_t len = 0;
          {
            std::lock_guard<std::mutex> guard(self->write_mtx);
            len = self->buffer_out.readArray((uint8_t *)pOutput + processed,
                                             bytes);
            open -= len;
            processed += len;
          }
          if (len != bytes) self->doWait();
        }
      }
    }
  }
};

}  // namespace audio_tools