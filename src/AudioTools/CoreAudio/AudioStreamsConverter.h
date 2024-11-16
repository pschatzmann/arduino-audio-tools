#pragma once
#include "AudioIO.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/ResampleStream.h"

namespace audio_tools {

/**
 * @brief Converter for reducing or increasing the number of Channels
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename T>
class ChannelFormatConverterStreamT : public ReformatBaseStream {
 public:
  ChannelFormatConverterStreamT(Stream &stream) { setStream(stream); }
  ChannelFormatConverterStreamT(Print &print) { setOutput(print); }
  ChannelFormatConverterStreamT(ChannelFormatConverterStreamT const &) = delete;
  ChannelFormatConverterStreamT &operator=(
      ChannelFormatConverterStreamT const &) = delete;

  bool begin(int fromChannels, int toChannels) {
    LOGI("begin %d -> %d channels", fromChannels, toChannels);
    //is_output_notify = false;
    from_channels = fromChannels;
    to_channels = toChannels;
    factor = static_cast<float>(toChannels) / static_cast<float>(fromChannels);

    converter.setSourceChannels(from_channels);
    converter.setTargetChannels(to_channels);

    return true;
  }

  bool begin() { return begin(from_channels, to_channels); }

  void setToChannels(uint16_t channels) { to_channels = channels; }

  virtual size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    if (p_print == nullptr) return 0;
    //addNotifyOnFirstWrite();
    if (from_channels == to_channels) {
      return p_print->write(data, len);
    }
    size_t resultBytes = convert(data, len);
    //assert(resultBytes == factor * len);
    p_print->write((uint8_t *)buffer.data(), resultBytes);
    return len;
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    if (p_stream == nullptr) return 0;
    if (from_channels == to_channels) {
      return p_stream->readBytes(data, len);
    }
    size_t in_bytes = 1.0f / factor * len;
    bufferTmp.resize(in_bytes);
    p_stream->readBytes(bufferTmp.data(), in_bytes);
    size_t resultBytes = convert(bufferTmp.data(), in_bytes);
    assert(len == resultBytes);
    memcpy(data, (uint8_t *)buffer.data(), len);
    return len;
  }

  void setAudioInfo(AudioInfo cfg) override {
    from_channels = cfg.channels;
    AudioStream::setAudioInfo(cfg);
  }

  /// Returns the AudioInfo with the to_channels
  AudioInfo audioInfoOut() override {
    AudioInfo out = audioInfo();
    out.channels = to_channels;
    return out;
  }

  virtual int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

  virtual int availableForWrite() override {
    if (p_print == nullptr) return 0;
    return 1.0f / factor * p_print->availableForWrite();
  }

  float getByteFactor() {
    return static_cast<float>(to_channels) / static_cast<float>(from_channels);
  }

 protected:
  int from_channels = 2;
  int to_channels = 2;
  float factor = 1;
  Vector<T> buffer{0};
  Vector<uint8_t> bufferTmp{0};
  ChannelConverter<T> converter;

  size_t convert(const uint8_t *in_data, size_t size) {
    size_t result;
    size_t result_samples = size / sizeof(T) * factor;
    buffer.resize(result_samples);
    result =
        converter.convert((uint8_t *)buffer.data(), (uint8_t *)in_data, size);
    if (result != result_samples * sizeof(T)) {
      LOGE("size %d -> result: %d - expeced: %d", (int)size, (int)result,
           static_cast<int>(result_samples * sizeof(T)));
    }
    return result;
  }
};

/**
 * @brief Channel converter which does not use a template
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ChannelFormatConverterStream : public ReformatBaseStream {
 public:
  ChannelFormatConverterStream() = default;
  ChannelFormatConverterStream(Stream &stream) { setStream(stream); }
  ChannelFormatConverterStream(Print &print) { setOutput(print); }
  ChannelFormatConverterStream(ChannelFormatConverterStream const &) = delete;
  ChannelFormatConverterStream &operator=(
      ChannelFormatConverterStream const &) = delete;

  void setAudioInfo(AudioInfo cfg) override {
    TRACED();
    from_channels = cfg.channels;
    LOGI("--> ChannelFormatConverterStream");
    AudioStream::setAudioInfo(cfg);
    switch (bits_per_sample) {
      case 8:
        getConverter<int8_t>()->setAudioInfo(cfg);
        break;
      case 16:
        getConverter<int16_t>()->setAudioInfo(cfg);
        break;
      case 24:
        getConverter<int24_t>()->setAudioInfo(cfg);
        break;
      case 32:
        getConverter<int32_t>()->setAudioInfo(cfg);
        break;
    }
  }

  /// Returns the AudioInfo with the to_channels
  AudioInfo audioInfoOut() override {
    AudioInfo out = audioInfo();
    out.channels = to_channels;
    return out;
  }

  bool begin(AudioInfo from, AudioInfo to) {
    if (from.sample_rate != to.sample_rate){
      LOGE("invalid sample_rate: %d", (int)to.sample_rate);
      return false;
    }
    if (from.bits_per_sample != to.bits_per_sample){
      LOGE("invalid bits_per_sample: %d", (int)to.bits_per_sample);
      return false;
    }
    return begin(from, to.channels);
  }

  bool begin(AudioInfo cfg, int toChannels) {
    assert(toChannels != 0);
    //is_output_notify = false;
    to_channels = toChannels;
    from_channels = cfg.channels;
    bits_per_sample = cfg.bits_per_sample;
    LOGI("--> ChannelFormatConverterStream");
    AudioStream::setAudioInfo(cfg);
    LOGI("begin %d -> %d channels", cfg.channels, toChannels);
    bool result = setupConverter(cfg.channels, toChannels);
    if (!result) {
      TRACEE()
    }
    return result;
  }

  bool begin() { return begin(audioInfo(), to_channels); }

  void setToChannels(uint16_t channels) { to_channels = channels; }

  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("ChannelFormatConverterStream::write: %d", (int)len);
    if (p_print == nullptr) return 0;
    //addNotifyOnFirstWrite();
    switch (bits_per_sample) {
      case 8:
        return getConverter<int8_t>()->write(data, len);
      case 16:
        return getConverter<int16_t>()->write(data, len);
      case 24:
        return getConverter<int24_t>()->write(data, len);
      case 32:
        return getConverter<int32_t>()->write(data, len);
      default:
        return 0;
    }
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    LOGD("ChannelFormatConverterStream::readBytes: %d", (int)len);
    switch (bits_per_sample) {
      case 8:
        return getConverter<int8_t>()->readBytes(data, len);
      case 16:
        return getConverter<int16_t>()->readBytes(data, len);
      case 24:
        return getConverter<int24_t>()->readBytes(data, len);
      case 32:
        return getConverter<int32_t>()->readBytes(data, len);
      default:
        return 0;
    }
  }

  virtual int available() override {
    switch (bits_per_sample) {
      case 8:
        return getConverter<int8_t>()->available();
      case 16:
        return getConverter<int16_t>()->available();
      case 24:
        return getConverter<int24_t>()->available();
      case 32:
        return getConverter<int32_t>()->available();
      default:
        return 0;
    }
  }

  virtual int availableForWrite() override {
    switch (bits_per_sample) {
      case 8:
        return getConverter<int8_t>()->availableForWrite();
      case 16:
        return getConverter<int16_t>()->availableForWrite();
      case 24:
        return getConverter<int24_t>()->availableForWrite();
      case 32:
        return getConverter<int32_t>()->availableForWrite();
      default:
        return 0;
    }
  }

  float getByteFactor() {
    return static_cast<float>(to_channels) / static_cast<float>(from_channels);
  }

 protected:
  void *converter;
  int bits_per_sample = 0;
  int to_channels;
  int from_channels;

  template <typename T>
  ChannelFormatConverterStreamT<T> *getConverter() {
    return (ChannelFormatConverterStreamT<T> *)converter;
  }

  bool setupConverter(int fromChannels, int toChannels) {
    bool result = true;
    if (p_stream != nullptr) {
      switch (bits_per_sample) {
        case 8:
          converter = new ChannelFormatConverterStreamT<int8_t>(*p_stream);
          getConverter<int8_t>()->begin(fromChannels, toChannels);
          break;
        case 16:
          converter = new ChannelFormatConverterStreamT<int16_t>(*p_stream);
          getConverter<int16_t>()->begin(fromChannels, toChannels);
          break;
        case 24:
          converter = new ChannelFormatConverterStreamT<int24_t>(*p_stream);
          getConverter<int24_t>()->begin(fromChannels, toChannels);
          break;
        case 32:
          converter = new ChannelFormatConverterStreamT<int32_t>(*p_stream);
          getConverter<int32_t>()->begin(fromChannels, toChannels);
          break;
        default:
          result = false;
      }
    } else {
      switch (bits_per_sample) {
        case 8:
          converter = new ChannelFormatConverterStreamT<int8_t>(*p_print);
          getConverter<int8_t>()->begin(fromChannels, toChannels);
          break;
        case 16:
          converter = new ChannelFormatConverterStreamT<int16_t>(*p_print);
          getConverter<int16_t>()->begin(fromChannels, toChannels);
          break;
        case 24:
          converter = new ChannelFormatConverterStreamT<int24_t>(*p_print);
          getConverter<int24_t>()->begin(fromChannels, toChannels);
          break;
        case 32:
          converter = new ChannelFormatConverterStreamT<int32_t>(*p_print);
          getConverter<int32_t>()->begin(fromChannels, toChannels);
          break;
        default:
          result = false;
      }
    }
    return result;
  }
};

/**
 * @brief A more generic templated Converter which converts from a source type to a
 * target type: You can use e.g. uint8_t, int8_t, int16_t, uint16_t, int24_t, uint32_t, int32_t, FloatAudio.AbstractMetaDat.
 * This is quite handy because unsigned values and floating values are supported and you do not need to 
 * resort to use a Codec.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam TFrom specifies the source data type 
 * @tparam TTo spesifies the target data type.
 */

template <typename TFrom, typename TTo>
class NumberFormatConverterStreamT : public ReformatBaseStream {
 public:
  NumberFormatConverterStreamT() = default;
  NumberFormatConverterStreamT(float gain) { setGain(gain); }
  NumberFormatConverterStreamT(Stream &stream) { setStream(stream); }
  NumberFormatConverterStreamT(AudioStream &stream) { setStream(stream); }
  NumberFormatConverterStreamT(Print &print) { setOutput(print); }
  NumberFormatConverterStreamT(AudioOutput &print) { setOutput(print); }

  void setAudioInfo(AudioInfo newInfo) override {
    TRACED();
    if (newInfo.bits_per_sample == sizeof(TFrom) * 8) {
      LOGE("Invalid bits_per_sample %d", newInfo.bits_per_sample);
    }
    ReformatBaseStream::setAudioInfo(newInfo);
  }

  AudioInfo audioInfoOut() override {
    AudioInfo to_format;
    to_format.copyFrom(info);
    to_format.bits_per_sample = sizeof(TTo) * 8;
    return to_format;
  }

  bool begin() override {
    LOGI("begin %d -> %d bits", (int)sizeof(TFrom), (int)sizeof(TTo));
    //is_output_notify = false;
    return true;
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    if (p_print == nullptr) return 0;
    //addNotifyOnFirstWrite();

#ifdef USE_TYPETRAITS
    if (std::is_same<TFrom, TTo>::value) return p_print->write(data, len); 
#else
    if (sizeof(TFrom) == sizeof(TTo)) return p_print->write(data, len);
#endif

    size_t samples = len / sizeof(TFrom);
    size_t result_size = 0;
    TFrom *data_source = (TFrom *)data;

    if (!is_buffered) {
      for (size_t j = 0; j < samples; j++) {
        TTo value = NumberConverter::convert<TFrom, TTo>(data_source[j]);
        result_size += p_print->write((uint8_t *)&value, sizeof(TTo));
      }
    } else {
      int size_bytes = sizeof(TTo) * samples;
      buffer.resize(size_bytes);
      NumberConverter::convertArray<TFrom, TTo>(
          data_source, (TTo *)buffer.data(), samples, gain);
      p_print->write((uint8_t *)buffer.address(), size_bytes);
      buffer.reset();
    }

    return len;
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    LOGD("NumberFormatConverterStreamT::readBytes: %d", (int)len);
    if (p_stream == nullptr) return 0;
    size_t samples = len / sizeof(TTo);
    TTo *data_target = (TTo *)data;
    TFrom source;
    if (!is_buffered) {
      for (size_t j = 0; j < samples; j++) {
        source = 0;
        p_stream->readBytes((uint8_t *)&source, sizeof(TFrom));
        data_target[j] = NumberConverter::convert<TFrom, TTo>(source);
      }
    } else {
      buffer.resize(sizeof(TFrom) * samples);
      readSamples<TFrom>(p_stream, (TFrom *)buffer.address(), samples);
      TFrom *data = (TFrom *)buffer.address();
      NumberConverter::convertArray<TFrom, TTo>(data, data_target, samples,
                                                gain);
      buffer.reset();
    }
    return len;
  }

  virtual int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

  virtual int availableForWrite() override {
    return p_print == nullptr ? 0 : p_print->availableForWrite();
  }

  /// if set to true we do one big write, else we get a lot of single writes per
  /// sample
  void setBuffered(bool flag) { is_buffered = flag; }

  /// Defines the gain (only available when buffered is true)
  void setGain(float value) { gain = value; }

  float getByteFactor() {
    return static_cast<float>(sizeof(TTo)) / static_cast<float>(sizeof(TFrom));
  }

 protected:
  SingleBuffer<uint8_t> buffer{0};
  bool is_buffered = true;
  float gain = 1.0f;
};

/**
 * @brief Converter which converts between bits_per_sample and 16 bits.
 * The templated NumberFormatConverterStreamT class is used based on the
 * information provided by the bits_per_sample in the configuration.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NumberFormatConverterStream : public ReformatBaseStream {
 public:
  NumberFormatConverterStream() = default;
  NumberFormatConverterStream(Stream &stream) { setStream(stream); }
  NumberFormatConverterStream(AudioStream &stream) { setStream(stream); }
  NumberFormatConverterStream(Print &print) { setOutput(print); }
  NumberFormatConverterStream(AudioOutput &print) { setOutput(print); }

  void setAudioInfo(AudioInfo newInfo) override {
    TRACED();
    this->from_bit_per_samples = newInfo.bits_per_sample;
    LOGI("-> NumberFormatConverterStream:")
    AudioStream::setAudioInfo(newInfo);
  }

  AudioInfo audioInfoOut() override {
    AudioInfo result = audioInfo();
    result.bits_per_sample = to_bit_per_samples;
    return result;
  }

  bool begin(AudioInfo info, AudioInfo to, float gain = 1.0f) {
    if (info.sample_rate != to.sample_rate){
      LOGE("sample_rate does not match")
      return false;
    }
    if (info.channels != to.channels){
      LOGE("channels do not match")
      return false;
    }
    return begin(info, to.bits_per_sample, gain);
  }

  bool begin(AudioInfo info, int toBits, float gain = 1.0f) {
    setAudioInfo(info);
    return begin(info.bits_per_sample, toBits, gain);
  }

  bool begin() { return begin(from_bit_per_samples, to_bit_per_samples, gain); }

  void setToBits(uint8_t bits) { to_bit_per_samples = bits; }

  bool begin(int from_bit_per_samples, int to_bit_per_samples,
             float gain = 1.0) {
    assert(to_bit_per_samples > 0);
    //is_output_notify = false;
    this->gain = gain;
    LOGI("begin %d -> %d bits", from_bit_per_samples, to_bit_per_samples);
    bool result = true;
    this->from_bit_per_samples = from_bit_per_samples;
    this->to_bit_per_samples = to_bit_per_samples;

    if (from_bit_per_samples == to_bit_per_samples) {
      LOGI("no bit conversion: %d -> %d", from_bit_per_samples,
           to_bit_per_samples);
    } else if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      converter = new NumberFormatConverterStreamT<int8_t, int16_t>(gain);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      converter = new NumberFormatConverterStreamT<int16_t, int8_t>(gain);
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      converter = new NumberFormatConverterStreamT<int24_t, int16_t>(gain);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      converter = new NumberFormatConverterStreamT<int16_t, int24_t>(gain);
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      converter = new NumberFormatConverterStreamT<int32_t, int16_t>(gain);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      converter = new NumberFormatConverterStreamT<int16_t, int32_t>(gain);
    } else {
      result = false;
      LOGE("bit combination not supported %d -> %d", from_bit_per_samples,
           to_bit_per_samples);
    }

    if (from_bit_per_samples != to_bit_per_samples) {
      setupStream();
    }

    if (!result) {
      TRACEE()
    }
    return result;
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("NumberFormatConverterStream::write: %d", (int)len);
    if (p_print == nullptr) return 0;
    if (from_bit_per_samples == to_bit_per_samples) {
      return p_print->write(data, len);
    }

    if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      return getConverter<int8_t, int16_t>()->write(data, len);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      return getConverter<int16_t, int8_t>()->write(data, len);
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      return getConverter<int24_t, int16_t>()->write(data, len);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      return getConverter<int16_t, int24_t>()->write(data, len);
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      return getConverter<int32_t, int16_t>()->write(data, len);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      return getConverter<int16_t, int32_t>()->write(data, len);
    } else {
      LOGE("bit combination not supported %d -> %d", from_bit_per_samples,
           to_bit_per_samples);
      return 0;
    }
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    LOGD("NumberFormatConverterStream::readBytes: %d", (int)len);
    if (from_bit_per_samples == to_bit_per_samples) {
      return p_stream->readBytes(data, len);
    }
    if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      return getConverter<int8_t, int16_t>()->readBytes(data, len);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      return getConverter<int16_t, int8_t>()->readBytes(data, len);
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      return getConverter<int24_t, int16_t>()->readBytes(data, len);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      return getConverter<int16_t, int24_t>()->readBytes(data, len);
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      return getConverter<int32_t, int16_t>()->readBytes(data, len);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      return getConverter<int16_t, int32_t>()->readBytes(data, len);
    } else {
      TRACEE();
      return 0;
    }
  }

  virtual int available() override {
    if (from_bit_per_samples == to_bit_per_samples) {
      return p_stream->available();
    }
    if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      return getConverter<int8_t, int16_t>()->available();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      return getConverter<int16_t, int8_t>()->available();
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      return getConverter<int24_t, int16_t>()->available();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      return getConverter<int16_t, int24_t>()->available();
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      return getConverter<int32_t, int16_t>()->available();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      return getConverter<int16_t, int32_t>()->available();
    } else {
      TRACEE();
      return 0;
    }
  }

  virtual int availableForWrite() override {
    if (p_print == nullptr) return 0;
    if (from_bit_per_samples == to_bit_per_samples) {
      return p_print->availableForWrite();
    }
    if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      return getConverter<int8_t, int16_t>()->availableForWrite();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      return getConverter<int16_t, int8_t>()->availableForWrite();
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      return getConverter<int24_t, int16_t>()->availableForWrite();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      return getConverter<int16_t, int24_t>()->availableForWrite();
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      return getConverter<int32_t, int16_t>()->availableForWrite();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      return getConverter<int16_t, int32_t>()->availableForWrite();
    } else {
      TRACEE();
      return 0;
    }
  }

  void setBuffered(bool flag) {
    if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      getConverter<int8_t, int16_t>()->setBuffered(flag);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      getConverter<int16_t, int8_t>()->setBuffered(flag);
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      getConverter<int24_t, int16_t>()->setBuffered(flag);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      getConverter<int16_t, int24_t>()->setBuffered(flag);
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      getConverter<int32_t, int16_t>()->setBuffered(flag);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      getConverter<int16_t, int32_t>()->setBuffered(flag);
    }
  }

  float getByteFactor() {
    return static_cast<float>(to_bit_per_samples) /
           static_cast<float>(from_bit_per_samples);
  }

 protected:
  void *converter = nullptr;
  int from_bit_per_samples = 16;
  int to_bit_per_samples = 0;
  float gain = 1.0;

  template <typename TFrom, typename TTo>
  NumberFormatConverterStreamT<TFrom, TTo> *getConverter() {
    return (NumberFormatConverterStreamT<TFrom, TTo> *)converter;
  }

  void setupStream() {
    if (p_stream != nullptr) {
      if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
        getConverter<int8_t, int16_t>()->setStream(*p_stream);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
        getConverter<int16_t, int8_t>()->setStream(*p_stream);
      } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
        getConverter<int24_t, int16_t>()->setStream(*p_stream);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
        getConverter<int16_t, int24_t>()->setStream(*p_stream);
      } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
        getConverter<int32_t, int16_t>()->setStream(*p_stream);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
        getConverter<int16_t, int32_t>()->setStream(*p_stream);
      } else {
        TRACEE();
      }
    } else {
      if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
        getConverter<int8_t, int16_t>()->setOutput(*p_print);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
        getConverter<int16_t, int8_t>()->setOutput(*p_print);
      } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
        getConverter<int24_t, int16_t>()->setOutput(*p_print);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
        getConverter<int16_t, int24_t>()->setOutput(*p_print);
      } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
        getConverter<int32_t, int16_t>()->setOutput(*p_print);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
        getConverter<int16_t, int32_t>()->setOutput(*p_print);
      } else {
        TRACEE();
      }
    }
  }
};

/**
 * @brief Converter which converts bits_per_sample, channels and the
 * sample_rate. The conversion is supported both on the input and output side.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class FormatConverterStream : public ReformatBaseStream {
 public:
  FormatConverterStream() = default;
  FormatConverterStream(Stream &stream) { setStream(stream); }
  FormatConverterStream(Print &print) { setOutput(print); }
  FormatConverterStream(AudioStream &stream) {
    to_cfg = stream.audioInfo();
    from_cfg = stream.audioInfo();
    setStream(stream);
  }
  FormatConverterStream(AudioOutput &print) {
    to_cfg = print.audioInfo();
    setOutput(print);
  }

  void setStream(Stream &io) override {
    TRACED();
    ReformatBaseStream::setStream(io);
    sampleRateConverter.setStream(io);
  }

  void setStream(AudioStream &io) override {
    TRACED();
    ReformatBaseStream::setStream(io);
    sampleRateConverter.setStream(io);
  }

  void setOutput(Print &print) override {
    TRACED();
    ReformatBaseStream::setOutput(print);
    sampleRateConverter.setOutput(print);
  }

  void setOutput(AudioOutput &print) override {
    TRACED();
    ReformatBaseStream::setOutput(print);
    sampleRateConverter.setOutput(print);
  }

  void setAudioInfo(AudioInfo info) override {
    TRACED();
    from_cfg = info;
    sampleRateConverter.setAudioInfo(info);
    numberFormatConverter.setAudioInfo(info);
    channelFormatConverter.setAudioInfo(info);
    ReformatBaseStream::setAudioInfo(info);
  }

  void setAudioInfoOut(AudioInfo to) { to_cfg = to; }

  AudioInfo audioInfoOut() override { return to_cfg; }

  bool begin(AudioInfo from, AudioInfo to) {
    TRACED();
    setAudioInfoOut(to);
    return begin(from);
  }

  /// Starts the processing: call setAudioInfoOut before to define the target
  bool begin(AudioInfo from) {
    setAudioInfo(from);
    return begin();
  }

  /// (Re-)Starts the processing: call setAudioInfo and setAudioInfoOut before
  bool begin() override {
    TRACED();
    //is_output_notify = false;
    // build output chain
    if (getStream() != nullptr) {
      sampleRateConverter.setStream(*getStream());
    }
    if (getPrint() != nullptr) {
      sampleRateConverter.setOutput(*getPrint());
    }
    numberFormatConverter.setStream(sampleRateConverter);
    channelFormatConverter.setStream(numberFormatConverter);

    // start individual converters
    bool result = channelFormatConverter.begin(from_cfg, to_cfg.channels);

    AudioInfo from_actual_cfg(from_cfg);
    from_actual_cfg.channels = to_cfg.channels;
    result &= numberFormatConverter.begin(from_actual_cfg.bits_per_sample,
                                          to_cfg.bits_per_sample);

    numberFormatConverter.setBuffered(is_buffered);
    sampleRateConverter.setBuffered(is_buffered);

    from_actual_cfg.bits_per_sample = to_cfg.bits_per_sample;
    result &= sampleRateConverter.begin(from_actual_cfg, to_cfg.sample_rate);

    // setup reader to support readBytes()
    setupReader();

    if (!result) {
      LOGE("begin failed");
    }
    return result;
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("FormatConverterStream::write: %d", (int)len);
    //addNotifyOnFirstWrite();
    return channelFormatConverter.write(data, len);
  }

  /// Buffering is active by default to minimize the number of output calls
  void setBuffered(bool active) { is_buffered = active; }

  float getByteFactor() {
    return numberFormatConverter.getByteFactor() *
           channelFormatConverter.getByteFactor();
  }

 protected:
  AudioInfo from_cfg;
  AudioInfo to_cfg;
  NumberFormatConverterStream numberFormatConverter;
  ChannelFormatConverterStream channelFormatConverter;
  ResampleStream sampleRateConverter;
  bool is_buffered = true;

  /// e.g if we do channels 2->1 we nead to double the input data
  /// @return
  float byteFactor() {
    return (float)from_cfg.channels / (float)to_cfg.channels *
           (float)from_cfg.bits_per_sample / (float)to_cfg.bits_per_sample;
  }
};

}  // namespace audio_tools