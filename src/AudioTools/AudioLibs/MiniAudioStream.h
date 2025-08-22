#pragma once
/**
 * @brief MiniAudio
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include <mutex>
#include <atomic>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define MA_BUFFER_COUNT 10
#define MA_BUFFER_SIZE 1200
#define MA_START_COUNT 2
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
  int buffer_size = MA_BUFFER_SIZE;
  int buffer_count = MA_BUFFER_COUNT;
  int buffer_start_count = MA_START_COUNT;
  bool auto_restart_on_underrun = true;  // Automatically restart after buffer underrun
  int underrun_tolerance = 5;            // Number of empty reads before stopping playback
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
      if (is_active.load()) {
        is_active.store(false);
        is_playing.store(false);
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
    setupBuffers(config.buffer_size);
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
      ma_device_uninit(&device_ma);
      return false;
    }

    is_active.store(true);
    return is_active.load();
  }

  void end() override {
    is_active.store(false);
    is_playing.store(false);
    // This will stop the device so no need to do that manually.
    ma_device_uninit(&device_ma);
    // release buffer memory
    buffer_in.resize(0);
    buffer_out.resize(0);
    is_buffers_setup.store(false);
  }

  int availableForWrite() override {
    return buffer_out.size() == 0 ? 0 : DEFAULT_BUFFER_SIZE;
  }

  size_t write(const uint8_t *data, size_t len) override {
    // Input validation
    if (!data || len == 0) {
      LOGW("Invalid write parameters: data=%p, len=%zu", data, len);
      return 0;
    }
    
    if (buffer_out.size() == 0) {
      LOGW("Output buffer not initialized");
      return 0;
    }
    
    if (!is_active.load()) {
      LOGW("Stream not active");
      return 0;
    }
    
    LOGD("write: %zu", len);

    // write data to buffer
    int open = len;
    int written = 0;
    int retry_count = 0;
    const int max_retries = 1000; // Prevent infinite loops
    
    while (open > 0 && retry_count < max_retries) {
      size_t result = 0;
      {
        std::lock_guard<std::mutex> guard(write_mtx);
        result = buffer_out.writeArray(data + written, open);
        open -= result;
        written += result;
      }

      if (result == 0) {
        retry_count++;
        doWait();
      } else {
        retry_count = 0; // Reset on successful write
      }
    }
    
    if (retry_count >= max_retries) {
      LOGE("Write timeout after %d retries, written %d of %zu bytes", max_retries, written, len);
    }

    // activate playing
    // if (!is_playing && buffer_out.bufferCountFilled()>=MA_START_COUNT) {
    int current_buffer_size = buffer_size.load();
    bool should_start_playing = false;
    
    // Start playing if we have enough data and either:
    // 1. We're not playing yet, or 
    // 2. We stopped due to buffer underrun but now have data again
    if (current_buffer_size > 0) {
      int available_data = buffer_out.available();
      int threshold = config.buffer_start_count * current_buffer_size;
      
      if (!is_playing.load() && available_data >= threshold) {
        should_start_playing = true;
      } else if (is_playing.load() && available_data == 0) {
        // Stop playing if buffer is completely empty (helps with long delays)
        LOGW("Buffer empty, pausing playback");
        is_playing.store(false);
      }
    }
    
    if (should_start_playing) {
      LOGI("starting audio playback");
      is_playing.store(true);
    }
    
    // std::this_thread::yield();
    return written;
  }

  int available() override {
    return buffer_in.size() == 0 ? 0 : buffer_in.available();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    if (!data || len == 0) {
      LOGW("Invalid read parameters: data=%p, len=%zu", data, len);
      return 0;
    }
    
    if (buffer_in.size() == 0) {
      LOGW("Input buffer not initialized");
      return 0;
    }
    
    if (!is_active.load()) {
      LOGW("Stream not active");
      return 0;
    }
    
    LOGD("read: %zu", len);
    std::lock_guard<std::mutex> guard(read_mtx);
    return buffer_in.readArray(data, len);
  }

  /// Manually restart playback (useful after long delays)
  void restartPlayback() {
    if (!is_active.load()) {
      LOGW("Cannot restart playback - stream not active");
      return;
    }
    
    int current_buffer_size = buffer_size.load();
    if (current_buffer_size > 0 && buffer_out.available() > 0) {
      LOGI("Manually restarting playback");
      is_playing.store(true);
    } else {
      LOGW("Cannot restart playback - no data available");
    }
  }

  /// Check if playback is currently active
  bool isPlaying() const {
    return is_playing.load();
  }

 protected:
  MiniAudioConfig config;
  ma_device_config config_ma;
  ma_device device_ma;
  std::atomic<bool> is_playing{false};
  std::atomic<bool> is_active{false};
  std::atomic<bool> is_buffers_setup{false};
  RingBuffer<uint8_t> buffer_out{0};
  RingBuffer<uint8_t> buffer_in{0};
  std::mutex write_mtx;
  std::mutex read_mtx;
  std::atomic<int> buffer_size{0};

  // In playback mode copy data to pOutput. In capture mode read data from
  // pInput. In full-duplex mode, both pOutput and pInput will be valid and
  // you can move data from pInput into pOutput. Never process more than
  // frameCount frames.

  void setupBuffers(int size = MA_BUFFER_SIZE) {
    std::lock_guard<std::mutex> guard(write_mtx);
    if (is_buffers_setup.load()) return;
    
    // Validate buffer size
    if (size <= 0 || size > 1024 * 1024) { // Max 1MB per buffer chunk
      LOGE("Invalid buffer size: %d", size);
      return;
    }
    
    buffer_size.store(size);
    int buffer_count = config.buffer_count;
    
    // Validate total buffer size to prevent excessive memory allocation
    size_t total_size = static_cast<size_t>(size) * buffer_count;
    if (total_size > 100 * 1024 * 1024) { // Max 100MB total
      LOGE("Buffer size too large: %zu bytes", total_size);
      return;
    }
    
    LOGI("setupBuffers: %d * %d = %zu bytes", size, buffer_count, total_size);
    
    if (buffer_out.size() == 0 && config.is_output) {
      if (!buffer_out.resize(size * buffer_count)) {
        LOGE("Failed to resize output buffer");
        return;
      }
    }
    if (buffer_in.size() == 0 && config.is_input) {
      if (!buffer_in.resize(size * buffer_count)) {
        LOGE("Failed to resize input buffer");
        return;
      }
    }
    is_buffers_setup.store(true);
  }

  void doWait() {
    //std::this_thread::yield();
    delay(config.delay_ms_if_buffer_full);
    //std::this_thread::sleep_for (std::chrono::milliseconds(MA_DELAY));
  }

  static void data_callback(ma_device *pDevice, void *pOutput,
                            const void *pInput, ma_uint32 frameCount) {
    MiniAudioStream *self = (MiniAudioStream *)pDevice->pUserData;
    if (!self || !self->is_active.load()) {
      return; // Safety check
    }
    
    AudioInfo cfg = self->audioInfo();
    if (cfg.channels == 0 || cfg.bits_per_sample == 0) {
      LOGE("Invalid audio configuration in callback");
      return;
    }

    int bytes = frameCount * cfg.channels * cfg.bits_per_sample / 8;
    if (bytes <= 0 || bytes > 1024 * 1024) { // Sanity check
      LOGE("Invalid byte count in callback: %d", bytes);
      return;
    }
    
    self->setupBuffers(bytes);

    if (pInput && self->buffer_in.size() > 0) {
      int open = bytes;
      int processed = 0;
      int retry_count = 0;
      const int max_retries = 100;
      
      while (open > 0 && retry_count < max_retries && self->is_active.load()) {
        int len = 0;
        {
          std::unique_lock<std::mutex> guard(self->read_mtx);
          len = self->buffer_in.writeArray((uint8_t *)pInput + processed, open);
          open -= len;
          processed += len;
        }
        if (len == 0) {
          retry_count++;
          self->doWait();
        } else {
          retry_count = 0;
        }
      }
    }

    if (pOutput) {
      memset(pOutput, 0, bytes);
      if (self->is_playing.load() && self->buffer_out.size() > 0) {
        int open = bytes;
        int processed = 0;
        int consecutive_failures = 0;
        const int max_failures = self->config.underrun_tolerance;
        
        while (open > 0 && self->is_active.load()) {
          size_t len = 0;
          {
            std::lock_guard<std::mutex> guard(self->write_mtx);
            len = self->buffer_out.readArray((uint8_t *)pOutput + processed, open);
            open -= len;
            processed += len;
          }
          
          if (len == 0) {
            consecutive_failures++;
            // If we can't get data for too long, stop playing to prevent issues
            if (consecutive_failures >= max_failures && self->config.auto_restart_on_underrun) {
              LOGW("Buffer underrun detected, stopping playback");
              self->is_playing.store(false);
              break;
            }
            // Don't wait in callback for too long - just output silence
            break;
          } else {
            consecutive_failures = 0;
          }
        }
      }
    }
  }
};

}  // namespace audio_tools