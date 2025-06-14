#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Determine Frequency using Audio Correlation.
 * based on https://github.com/akellyirl/AutoCorr_Freq_detect.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
 */
class FrequencyDetectorAutoCorrelation : public AudioStream {
 public:
  FrequencyDetectorAutoCorrelation(int bufferSize) {
    buffer_size = bufferSize;
  };

  FrequencyDetectorAutoCorrelation(int bufferSize, Print& out) {
    p_out = &out;
    buffer_size = bufferSize;
  };

  FrequencyDetectorAutoCorrelation(int bufferSize, Stream& in) {
    p_out = &in;
    p_in = &in;
    buffer_size = bufferSize;
  }

  bool begin(AudioInfo) {
    setAudioInfo(info);
    return begin();
  }

  bool begin() {
    buffer.resize(buffer_size * info.channels * info.bits_per_sample / 8);
    freq.resize(info.channels);
    return AudioStream::begin();
  }

  int available() override {
    if (p_in) return p_in->available();
    return 0;
  }

  int availableForWrite() override {
    if (p_out) return p_out->availableForWrite();
    return DEFAULT_BUFFER_SIZE;
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    size_t result = p_in->readBytes(data, len);
    for (int i = 0; i < len; i++) {
      buffer.write(data[i]);
      if (buffer.isFull()) {
        switch (info.bits_per_sample) {
          case 16:
            detect<int16_t>((int16_t*)buffer.data(),
                            buffer.available() / sizeof(int16_t));
            break;
          case 24:
            detect<int24_t>((int24_t*)buffer.data(),
                            buffer.available() / sizeof(int24_t));
            break;
          case 32:
            detect<int32_t>((int32_t*)buffer.data(),
                            buffer.available() / sizeof(int32_t));
            break;
        }
        buffer.reset();
      }
    }
    return result;
  }

  virtual size_t write(const uint8_t* data, size_t len) override {
    for (int i = 0; i < len; i++) {
      buffer.write(data[i]);
      if (buffer.isFull()) {
        switch (info.bits_per_sample) {
          case 16:
            detect<int16_t>((int16_t*)buffer.data(),
                            buffer.available() / sizeof(int16_t));
            break;
          case 24:
            detect<int24_t>((int24_t*)buffer.data(),
                            buffer.available() / sizeof(int24_t));
            break;
          case 32:
            detect<int32_t>((int32_t*)buffer.data(),
                            buffer.available() / sizeof(int32_t));
            break;
        }
        buffer.reset();
      }
    }

    size_t result = len;
    if (p_out != nullptr) result = p_out->write(data, len);
    return result;
  }

  /// provides the determined frequncy
  float frequency(int channel) {
    if (channel >= info.channels) {
      LOGE("Invalid channel: %d", channel);
      return 0;
    }
    return freq[channel];
  }

  AudioInfo defaultConfig() {
    AudioInfo result;
    return result;
  }

  void setFrequencyCallback(void (*callback)(int channel, float freq)) {
    freq_callback = callback;
  }

 protected:
  Vector<float> freq;
  Print* p_out = nullptr;
  Stream* p_in = nullptr;
  void (*freq_callback)(int channel, float freq);
  int buffer_size = 0;
  SingleBuffer<uint8_t> buffer;

  template <class T>
  void detect(T* samples, size_t len) {
    freq.resize(info.channels);
    for (int ch = 0; ch < info.channels; ch++) {
      freq[ch] = detectFrequencyForChannel(ch, samples, len);
      if (freq_callback) freq_callback(ch, freq[ch]);
    }
  }

  template <class T>
  float detectFrequencyForChannel(int ch, T* samples, size_t len) {
    LOGI("detectFrequencyForChannel: %d / len: %u", ch, (unsigned int)len);
    // Copy buffer in correct order
    int sample_rate = info.sample_rate;
    int channels = info.channels;
    int buffer_size = len / info.channels;
    // float channel_data[buffer_size];
    // memset(channel_data, 0, sizeof(channel_data));
    T* channel_data = (T*)samples;

    // // extract data for the indicated channel
    // int pos = 0;
    // for (size_t i = ch; i < len; i += info.channels) {
    //   channel_data[pos++] = samples[i];
    // }

    // // Remove DC offset
    // float mean = 0.0f;
    // for (float v : channel_data) mean += v;
    // mean /= buffer_size;
    // for (float& v : channel_data) v -= mean;

    LOGI("buffer size (samples): %u", buffer_size);
    // LOGI("mean: %f", mean);

    // Autocorrelation
    size_t min_lag = sample_rate / 1000;  // 1000 Hz max
    size_t max_lag = sample_rate / 50;    // 50 Hz min
    if (max_lag >= buffer_size) max_lag = buffer_size - 1;

    LOGI("lag min/max: %u / %u", min_lag, max_lag);

    float max_corr = 0.0f;
    size_t best_lag = 0;
    for (size_t lag = min_lag; lag < max_lag; ++lag) {
      float sum = 0.0f;
      for (size_t i = 0; i < buffer_size - lag; ++i) {
        sum += samples[i * channels] * channel_data[(i + lag) * channels];
      }
      if (sum > max_corr) {
        max_corr = sum;
        best_lag = lag;
      }
    }

    LOGI("best_lag: %u / max_corr: %f", best_lag, max_corr);

    if (best_lag == 0) return 0.0f;
    return (float)sample_rate / best_lag;
  }
};

/**
 * @brief Determine Frequency using upward 0 crossings.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
 */

class FrequencyDetectorZeroCrossing : public AudioStream {
 public:
  FrequencyDetectorZeroCrossing() = default;

  FrequencyDetectorZeroCrossing(Print& out) { p_out = &out; };

  FrequencyDetectorZeroCrossing(Stream& in) {
    p_out = &in;
    p_in = &in;
  };

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return AudioStream::begin();
  }

  int available() override {
    if (p_in) return p_in->available();
    return 0;
  }

  int availableForWrite() override {
    if (p_out) return p_out->availableForWrite();
    return DEFAULT_BUFFER_SIZE;
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    size_t result = p_in->readBytes(data, len);
    switch (info.bits_per_sample) {
      case 16:
        detect<int16_t>((int16_t*)data, len / sizeof(int16_t));
        break;
      case 24:
        detect<int24_t>((int24_t*)data, len / sizeof(int24_t));
        break;
      case 32:
        detect<int32_t>((int32_t*)data, len / sizeof(int32_t));
        break;
    }
    return result;
  }

  virtual size_t write(const uint8_t* data, size_t len) override {
    switch (info.bits_per_sample) {
      case 16:
        detect<int16_t>((int16_t*)data, len / sizeof(int16_t));
        break;
      case 24:
        detect<int24_t>((int24_t*)data, len / sizeof(int24_t));
        break;
      case 32:
        detect<int32_t>((int32_t*)data, len / sizeof(int32_t));
        break;
    }

    size_t result = len;
    if (p_out != nullptr) result = p_out->write(data, len);
    return result;
  }

  /// provides the determined frequncy
  float frequency(int channel) {
    if (channel >= info.channels) {
      LOGE("Invalid channel: %d", channel);
      return 0;
    }
    return states[channel].freq;
  }

  void setFrequencyCallback(void (*callback)(int channel, float freq)) {
    freq_callback = callback;
  }

 protected:
  struct ChannelState {
    int count = 0;        ///< Sample count since last zero crossing
    bool active = false;  ///< True if counting is active
    float freq = 0.0f;    ///< Last detected frequency
  };
  Vector<ChannelState> states;
  Print* p_out = nullptr;
  Stream* p_in = nullptr;
  int count = 0;
  bool active = false;
  void (*freq_callback)(int channel, float freq);

  template <class T>
  void detect(T* samples, size_t len) {
    states.resize(info.channels);
    for (int ch = 0; ch < info.channels; ch++) {
      detectChannel(ch, samples, len);
    }
  }

  template <class T>
  void detectChannel(int channel, T* samples, size_t len) {
    ChannelState& state = states[channel];
    for (int i = channel; i < (int)(len - info.channels); i += info.channels) {
      if (state.active) state.count++;
      // Upward zero crossing
      if (samples[i] <= 0 && samples[i + info.channels] > 0) {
        if (state.count > 0) {
          state.freq = (1.0f * info.sample_rate) / state.count;
          if (freq_callback) freq_callback(channel, state.freq);
        }
        state.count = 0;
        state.active = true;
      }
    }
  }
};

}  // namespace audio_tools
