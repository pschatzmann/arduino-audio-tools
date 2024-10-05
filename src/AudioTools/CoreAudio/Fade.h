#pragma once
#include "AudioConfig.h"
#include "AudioStreams.h"

namespace audio_tools {

/**
 * @brief Fade In and Fade out in order to prevent popping sound when the
 * audio is started or stopped. The fade in/out is performed over the length
 * of the buffer.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Fade {
public:
  void setFadeInActive(bool flag) {
    is_fade_in = flag;
    if (is_fade_in) {
      volume = 0.0;
      is_fade_out = false;
      is_done = false;
    }
  }

  bool isFadeInActive() { return is_fade_in; }

  void setFadeOutActive(bool flag) {
    is_fade_out = flag;
    if (is_fade_out) {
      volume = 1.0;
      is_fade_in = false;
      is_done = false;
    }
  }

  bool isFadeOutActive() { return is_fade_out; }

  /// @brief Updates the amplitude of the data when a fade in or fade out has
  /// been requested
  /// @param data
  /// @param bytes
  /// @param channels
  /// @param bitsPerSample
  void convert(uint8_t *data, int bytes, int channels, int bitsPerSample) {
    this->channels = channels;
    int bytes_per_sample = bitsPerSample / 8;
    switch (bitsPerSample) {
    case 16:
      convertFrames<int16_t>((int16_t *)data,
                             bytes / bytes_per_sample / channels, channels);
      break;
    case 24:
      convertFrames<int24_t>((int24_t *)data,
                             bytes / bytes_per_sample / channels, channels);
      break;
    case 32:
      convertFrames<int32_t>((int32_t *)data,
                             bytes / bytes_per_sample / channels, channels);
      break;
    default:
      LOGE("%s", "Unsupported bitsPerSample");
      break;
    }
  }

  /// Returns true if the conversion has been executed with any data
  bool isFadeComplete() { return is_done; }

protected:
  bool is_fade_in = false;
  bool is_fade_out = false;
  int channels = 2;
  float volume = 1.0;
  bool is_done = false;

  template <typename T> void convertFrames(T *data, int frames, int channels) {
    float delta = 1.0 / frames;
    // handle fade out
    if (is_fade_in) {
      fadeIn<T>(data, frames, channels, delta);
      is_fade_in = false;
    }  else if (is_fade_out) {
      fadeOut<T>(data, frames, channels, delta);
    }
    if (frames > 0) {
      is_done = true;
    }
  }

  template <typename T>
  void fadeOut(T *data, int frames, int channels, float delta) {
    for (int j = 0; j < frames; j++) {
      for (int ch = 0; ch < channels; ch++) {
        data[j * channels + ch] = data[j * channels + ch] * volume;
        if (volume > 0) {
          volume -= delta;
          if (volume < 0) {
            volume = 0;
          }
        }
      }
    }
    is_fade_out = false;
    LOGI("faded out %d frames to volume %f",frames, volume);
  }

  template <typename T>
  void fadeIn(T *data, int frames, int channels, float delta) {
    LOGI("fade in %d frames from volume %f",frames, volume);
    for (int j = 0; j < frames; j++) {
      for (int ch = 0; ch < channels; ch++) {
        data[j * channels + ch] = data[j * channels + ch] * volume;
        volume += delta;
        if (volume > 1.0f) {
          volume = 1.0f;
        }
      }
    }
    volume = 1.0f;
    is_fade_in = false;
  }
};

/**
 * @brief If we end audio and the last sample is not close to 0 we can hear a popping noise.
 * This functionality brings the last value slowly to 0.
 * 
 * @tparam T 
 */
template <typename T> 
class LastSampleFaderT {
public:
  void setChannels(int ch) { channels = ch; last.resize(ch); }
  size_t write(uint8_t *src, size_t size) {
    if (channels == 0){
      LOGE("channels=0");
      return 0;
    }
    int frames = size / sizeof(T) / channels;
    storeLastSamples(frames, src);
    return size;
  };

  /// @brief When we do not have any data any more to fade out we try to bring
  /// the last sample slowly to 0
  void end(Print &print, int steps = 200) {
    T out[channels];
    for (int j = 0; j < steps; j++) {
      for (int ch = 0; ch < channels; ch++) {
        float factor =
            static_cast<float>(steps - j) / static_cast<float>(steps);
        out[ch] = last[ch] * factor;
      }
      print.write((uint8_t *)out, channels * sizeof(T));
    }
  }

protected:
  int channels = 0;
  Vector<T> last{0};

  void storeLastSamples(int frames, uint8_t *src) {
    T *data = (T *)src;
    for (int ch = 0; ch < channels; ch++) {
      last[ch] = data[frames - 2 * channels + ch];
    }
  }
};

/**
 * @brief If we end audio and the last sample is not close to 0 we can hear a popping noise.
 * This functionality brings the last value slowly to 0. Typless implementation.
 */
class LastSampleFader {
public:
  void setChannels(int ch) {
    f16.setChannels(ch);
    f24.setChannels(ch);
    f32.setChannels(ch);
  }

  void setBitsPerSample(int bits){
    bits_per_sample = bits;
  }

  void setAudioInfo(AudioInfo info)  {
    setChannels(info.channels);
    setBitsPerSample(info.bits_per_sample);
  }

  size_t write(uint8_t *src, size_t size) {
    switch(bits_per_sample){
      case 16:
        return f16.write(src, size);
      case 24:
        return f24.write(src, size);
      case 32:
        return f32.write(src, size);
      default:
        LOGE("bits_per_sample is 0");
    }  
    return 0;
  };

  /// @brief When we do not have any data any more to fade out we try to bring
  /// the last sample slowly to 0
  void end(Print &print, int steps = 200) {
    switch(bits_per_sample){
      case 16:
        f16.end(print, steps);
        break;
      case 24:
        f24.end(print, steps);
        break;
      case 32:
        f32.end(print, steps);
        break;
    }
  }

protected:
  int bits_per_sample = 0;
  LastSampleFaderT<int16_t> f16;
  LastSampleFaderT<int24_t> f24;
  LastSampleFaderT<int32_t> f32;
};


/**
 * @brief Stream which can be used to manage fade in and fade out.
 * Before you read or write data you need to call setAudioInfo() to provide
 * the bits_per_sample and channels
 *
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FadeStream : public ModifyingStream {
public:
  FadeStream() = default;
  FadeStream(Print &out) { setOutput(out); }
  FadeStream(Stream &io) { setStream(io); }

  void setStream(Stream &io) override {
    p_io = &io;
    p_out = &io;
  }

  void setOutput(Print &out) override { p_out = &out; }

  /// same as setStream
  void setOutput(Stream &io) {
    p_io = &io;
    p_out = &io;
  }

  /// same as setOutput
  void setStream(Print &out) { p_out = &out; }

  bool begin(AudioInfo info){
    setAudioInfo(info);
    return AudioStream::begin();
  }

  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    fade_last.setAudioInfo(info);
    active = true;
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    if (!active) {
      LOGE("%s", error_msg);
      return 0;
    }
    fade.convert(data, len, info.channels, info.bits_per_sample);
    fade_last.write(data, len);
    return p_io->readBytes(data, len);
  }

  int available() override { return p_io == nullptr ? 0 : p_io->available(); }

  size_t write(const uint8_t *data, size_t len) override {
    if (p_out==nullptr) return 0;
    if (!active) {
      LOGE("%s", error_msg);
      return 0;
    }
    if (fade.isFadeInActive() || fade.isFadeOutActive()){
      fade.convert((uint8_t *)data, len, info.channels, info.bits_per_sample);
    }
    // update last information
    fade_last.write((uint8_t *)data, len);
    // write faded data
    return p_out->write(data, len);
  }

  int availableForWrite() override {
    return p_out == nullptr ? 0 : p_out->availableForWrite();
  }

  void setFadeInActive(bool flag) { fade.setFadeInActive(flag); }

  bool isFadeInActive() { return fade.isFadeInActive(); }

  void setFadeOutActive(bool flag) { fade.setFadeOutActive(flag); }

  bool isFadeOutActive() { return fade.isFadeOutActive(); }

  bool isFadeComplete() { return fade.isFadeComplete(); }

  // If can not provide any more samples we bring the last sample slowy back to 0
  void writeEnd(Print &print, int steps = 200) {
    fade_last.end(print, steps);
  }

protected:
  bool active = false;
  Fade fade;
  LastSampleFader fade_last;
  Print *p_out = nullptr;
  Stream *p_io = nullptr;
  const char *error_msg = "setAudioInfo not called";
};

/**
 * @brief converter which does a fade out or fade in.
 * @ingroup convert
 * @tparam T
 */
template <typename T> class FadeConverter : public BaseConverter{
public:
  FadeConverter(int channels) { setChannels(channels); }

  void setChannels(int ch) { channels = ch; }

  void setFadeInActive(bool flag) { fade.setFadeInActive(flag); }

  bool isFadeInActive() { return fade.isFadeInActive(); }

  void setFadeOutActive(bool flag) { fade.setFadeOutActive(flag); }

  bool isFadeOutActive() { return fade.isFadeOutActive(); }

  bool isFadeComplete() { return fade.isFadeComplete(); }

  virtual size_t convert(uint8_t *src, size_t size) {
    int frames = size / sizeof(T) / channels;
    fade.convertFrames<T>(src, frames, channels);
    return size;
  };

protected:
  int channels = 0;
  Fade fade;
};

} // namespace audio_tools