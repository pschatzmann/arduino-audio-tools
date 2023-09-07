#pragma once
#include "AudioTools/AudioStreams.h"
#include "AudioTools/ResampleStream.h"

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
  ChannelFormatConverterStreamT(Print &print) { setStream(print); }
  ChannelFormatConverterStreamT(ChannelFormatConverterStreamT const &) = delete;
  ChannelFormatConverterStreamT &operator=(ChannelFormatConverterStreamT const &) = delete;


  bool begin(int fromChannels, int toChannels) {
    LOGI("begin %d -> %d channels", fromChannels, toChannels);
    from_channels = fromChannels;
    to_channels = toChannels;
    factor = static_cast<float>(toChannels) / static_cast<float>(fromChannels);

    converter.setSourceChannels(from_channels);
    converter.setTargetChannels(to_channels);

    return true;
  }

  virtual size_t write(const uint8_t *data, size_t size) override {
    TRACED();
    if (from_channels == to_channels) {
      return p_print->write(data, size);
    }
    size_t resultBytes = convert(data, size);
    assert(resultBytes = factor * size);
    p_print->write((uint8_t *)buffer.data(), resultBytes);
    return size;
  }

  size_t readBytes(uint8_t *data, size_t size) override {
    TRACED();
    if (p_stream == nullptr) return 0;
    if (from_channels == to_channels) {
      return p_stream->readBytes(data, size);
    }
    size_t in_bytes = 1.0f / factor * size;
    bufferTmp.resize(in_bytes);
    p_stream->readBytes(bufferTmp.data(), in_bytes);
    size_t resultBytes = convert(bufferTmp.data(), in_bytes);
    assert(size == resultBytes);
    memcpy(data, (uint8_t *)buffer.data(), size);
    return size;
  }

  void setAudioInfo(AudioInfo cfg) override {
    AudioStream::setAudioInfo(cfg);
    to_channels = cfg.channels;
  }

  virtual int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

  virtual int availableForWrite() override {
    return 1.0f / factor * p_print->availableForWrite();
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
  ChannelFormatConverterStream(Print &print) { setStream(print); }
  ChannelFormatConverterStream(ChannelFormatConverterStream const &) = delete;
  ChannelFormatConverterStream &operator=(
      ChannelFormatConverterStream const &) = delete;

  void setAudioInfo(AudioInfo cfg) override {
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

  bool begin(AudioInfo cfg, int toChannels) {
    AudioStream::setAudioInfo(cfg);
    LOGI("begin %d -> %d channels", cfg.channels, toChannels);
    bits_per_sample = cfg.bits_per_sample;
    bool result = setupConverter(cfg.channels, toChannels);
    if (!result) {
      TRACEE()
    }
    return result;
  }

  virtual size_t write(const uint8_t *data, size_t size) override {
    LOGD("ChannelFormatConverterStream::write: %d", (int)size);
    switch (bits_per_sample) {
      case 8:
        return getConverter<int8_t>()->write(data, size);
      case 16:
        return getConverter<int16_t>()->write(data, size);
      case 24:
        return getConverter<int24_t>()->write(data, size);
      case 32:
        return getConverter<int32_t>()->write(data, size);
      default:
        return 0;
    }
  }

  size_t readBytes(uint8_t *data, size_t size) override {
    LOGD("ChannelFormatConverterStream::readBytes: %d", (int)size);
    switch (bits_per_sample) {
      case 8:
        return getConverter<int8_t>()->readBytes(data, size);
      case 16:
        return getConverter<int16_t>()->readBytes(data, size);
      case 24:
        return getConverter<int24_t>()->readBytes(data, size);
      case 32:
        return getConverter<int32_t>()->readBytes(data, size);
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

 protected:
  void *converter;
  int bits_per_sample = 0;

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
 * @brief Converter which converts from source bits_per_sample to target
 * bits_per_sample
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T specifies the current data type for the result of the read or
 * write.
 * @tparam TArg is the data type of the Stream or Print Object that is passed in
 * the Constructor
 */

template <typename TFrom, typename TTo>
class NumberFormatConverterStreamT : public ReformatBaseStream {
 public:
  NumberFormatConverterStreamT() = default;
  NumberFormatConverterStreamT(Stream &stream) { setStream(stream); }
  NumberFormatConverterStreamT(Print &print) { setStream(print); }

  bool begin() override {
    LOGI("begin %d -> %d bits", (int) sizeof(TFrom),(int) sizeof(TTo));
    return true;
  }

  virtual size_t write(const uint8_t *data, size_t size) override {
    TRACED();
    if (sizeof(TFrom) == sizeof(TTo)) return p_print->write(data, size);
    size_t samples = size / sizeof(TFrom);
    size_t result_size = 0;
    TFrom *data_source = (TFrom *)data;

    if (!is_buffered) {
      for (size_t j = 0; j < samples; j++) {
        TTo value = NumberConverter::convert<TFrom, TTo>(data_source[j]);
        result_size += p_print->write((uint8_t *)&value, sizeof(TTo));
      }
    } else {
      buffer.resize(sizeof(TTo) * samples);
      for (size_t j = 0; j < samples; j++) {
        TTo value = NumberConverter::convert<TFrom, TTo>(data_source[j]);
        result_size += buffer.writeArray((uint8_t *)&value, sizeof(TTo));
      }
      p_print->write((uint8_t *)buffer.address(), result_size);
      buffer.reset();
    }

    return size;
  }

  size_t readBytes(uint8_t *data, size_t size) override {
    LOGD("NumberFormatConverterStreamT::readBytes: %d", (int)size);
    if (p_stream == nullptr) return 0;
    size_t samples = size / sizeof(TTo);
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
      for (size_t j = 0; j < samples; j++) {
        data_target[j] = NumberConverter::convert<TFrom, TTo>(data[j]);
      }
    }
    return size;
  }

  virtual int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

  virtual int availableForWrite() override {
    return p_print->availableForWrite();
  }

  /// if set to true we do one big write, else we get a lot of single writes per
  /// sample
  void setBuffered(bool flag) { is_buffered = flag; }

 protected:
  SingleBuffer<uint8_t> buffer{0};
  bool is_buffered = false;
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
  NumberFormatConverterStream(Print &print) { setStream(print); }

  void setAudioInfo (AudioInfo info) override {
    this->from_bit_per_samples = info.sample_rate;
    AudioStream::setAudioInfo(info);
  }

  bool begin(AudioInfo info, int to_bit_per_samples) {
    AudioStream::setAudioInfo(info);
    return begin(info.bits_per_sample, to_bit_per_samples);
  }

  bool begin(int from_bit_per_samples, int to_bit_per_samples) {
    LOGI("begin %d -> %d bits", from_bit_per_samples, to_bit_per_samples);
    bool result = true;
    this->from_bit_per_samples = from_bit_per_samples;
    this->to_bit_per_samples = to_bit_per_samples;

    if (from_bit_per_samples == to_bit_per_samples) {
      LOGI("no bit conversion: %d -> %d", from_bit_per_samples,
           to_bit_per_samples);
    } else if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      converter = new NumberFormatConverterStreamT<int8_t, int16_t>();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      converter = new NumberFormatConverterStreamT<int16_t, int8_t>();
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      converter = new NumberFormatConverterStreamT<int24_t, int16_t>();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      converter = new NumberFormatConverterStreamT<int16_t, int24_t>();
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      converter = new NumberFormatConverterStreamT<int32_t, int16_t>();
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      converter = new NumberFormatConverterStreamT<int16_t, int32_t>();
    } else {
      result = false;
      LOGE("bit combination not supported %d -> %d", from_bit_per_samples,
           to_bit_per_samples);
    }

    if (from_bit_per_samples != to_bit_per_samples){
      setupStream();
    }

    if (!result) {
      TRACEE()
    }
    return result;
  }

  virtual size_t write(const uint8_t *data, size_t size) override {
    LOGD("NumberFormatConverterStream::write: %d", (int) size);
    if (from_bit_per_samples == to_bit_per_samples) {
      return p_print->write(data, size);
    }

    if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      return getConverter<int8_t, int16_t>()->write(data, size);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      return getConverter<int16_t, int8_t>()->write(data, size);
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      return getConverter<int24_t, int16_t>()->write(data, size);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      return getConverter<int16_t, int24_t>()->write(data, size);
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      return getConverter<int32_t, int16_t>()->write(data, size);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      return getConverter<int16_t, int32_t>()->write(data, size);
    } else {
      TRACEE();
      return 0;
    }
  }

  size_t readBytes(uint8_t *data, size_t size) override {
    LOGD("NumberFormatConverterStream::readBytes: %d", (int)size);
    if (from_bit_per_samples == to_bit_per_samples) {
      return p_stream->readBytes(data, size);
    }
    if (from_bit_per_samples == 8 && to_bit_per_samples == 16) {
      return getConverter<int8_t, int16_t>()->readBytes(data, size);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
      return getConverter<int16_t, int8_t>()->readBytes(data, size);
    } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
      return getConverter<int24_t, int16_t>()->readBytes(data, size);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
      return getConverter<int16_t, int24_t>()->readBytes(data, size);
    } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
      return getConverter<int32_t, int16_t>()->readBytes(data, size);
    } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
      return getConverter<int16_t, int32_t>()->readBytes(data, size);
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

 protected:
  void *converter = nullptr;
  int from_bit_per_samples = 0;
  int to_bit_per_samples = 0;

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
        getConverter<int8_t, int16_t>()->setStream(*p_print);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 8) {
        getConverter<int16_t, int8_t>()->setStream(*p_print);
      } else if (from_bit_per_samples == 24 && to_bit_per_samples == 16) {
        getConverter<int24_t, int16_t>()->setStream(*p_print);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 24) {
        getConverter<int16_t, int24_t>()->setStream(*p_print);
      } else if (from_bit_per_samples == 32 && to_bit_per_samples == 16) {
        getConverter<int32_t, int16_t>()->setStream(*p_print);
      } else if (from_bit_per_samples == 16 && to_bit_per_samples == 32) {
        getConverter<int16_t, int32_t>()->setStream(*p_print);
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
  FormatConverterStream(Print &print) { setStream(print); }
  FormatConverterStream(AudioStream &stream) {
    setSourceAudioInfo(stream.audioInfo());
    setStream(stream);
  }
  FormatConverterStream(AudioOutput &print) {
    setSourceAudioInfo(print.audioInfo());
    setStream(print);
  }

  void setStream(Stream &io) override {
    TRACED();
    //p_print = &print;
    ReformatBaseStream::setStream(io);
    sampleRateConverter.setStream(io);
  }

  void setStream(Print &print) override {
    TRACED();
    //p_print = &print;
    ReformatBaseStream::setStream(print);
    sampleRateConverter.setStream(print);
  }


  /// Defines the audio info of the stream which has been defined in the
  /// constructor
  void setSourceAudioInfo(AudioInfo from) { from_cfg = from; }

  bool begin(AudioInfo from, AudioInfo to) {
    setSourceAudioInfo(from);
    return begin(to);
  }

  bool begin(AudioInfo to) {
    TRACED();
    setAudioInfo(to);
    to_cfg = to;

    // build output chain
    if (getStream()!=nullptr){
      sampleRateConverter.setStream(*getStream());
    }
    if(getPrint()!=nullptr){
      sampleRateConverter.setStream(*getPrint());
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
    if (getStream()!=nullptr){
      setupReader();
    }

    if (!result) {
      LOGE("begin failed");
    }
    return result;
  }

  virtual size_t write(const uint8_t *data, size_t size) override {
    LOGD("FormatConverterStream::write: %d", (int)size);
    return channelFormatConverter.write(data, size);
  }

  /// Buffering is active by default to minimize the number of output calls
  void setBuffered(bool active){
    is_buffered = active;
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