#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections.h"
#include "AudioFilter/Filter.h"
#include "AudioTypes.h"

/**
 * @defgroup convert Converters
 * @ingroup tools
 * @brief Convert Audio
 * You can add a converter as argument to the StreamCopy::copy() or better use
 * is with a ConverterStream.
 */

namespace audio_tools {

/**
 * @brief Abstract Base class for Converters
 * A converter is processing the data in the indicated array
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
class BaseConverter {
 public:
  BaseConverter() = default;
  BaseConverter(BaseConverter const &) = delete;
  virtual ~BaseConverter() = default;

  BaseConverter &operator=(BaseConverter const &) = delete;

  virtual size_t convert(uint8_t *src, size_t size) = 0;
};

/**
 * @brief Dummy converter which does nothing
 * @ingroup convert
 * @tparam T
 */
class NOPConverter : public BaseConverter {
 public:
  size_t convert(uint8_t(*src), size_t size) override { return size; };
};

/**
 * @brief Multiplies the values with the indicated factor adds the offset and
 * clips at maxValue. To mute use a factor of 0.0!
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 * @tparam T
 */
template <typename T>
class ConverterScaler : public BaseConverter {
 public:
  ConverterScaler(float factor, T offset, T maxValue, int channels = 2) {
    this->factor_value = factor;
    this->maxValue = maxValue;
    this->offset_value = offset;
    this->channels = channels;
  }

  size_t convert(uint8_t *src, size_t byte_count) {
    T *sample = (T *)src;
    int size = byte_count / channels / sizeof(T);
    for (size_t j = 0; j < size; j++) {
      for (int i = 0; i < channels; i++) {
        *sample = (*sample + offset_value) * factor_value;
        if (*sample > maxValue) {
          *sample = maxValue;
        } else if (*sample < -maxValue) {
          *sample = -maxValue;
        }
        sample++;
      }
    }
    return byte_count;
  }

  /// Defines the factor (volume)
  void setFactor(float factor) { this->factor_value = factor; }

  /// Defines the offset
  void setOffset(T offset) { this->offset_value = offset; }

  /// Determines the actual factor (volume)
  float factor() { return factor_value; }

  /// Determines the offset value
  T offset() { return offset_value; }

 protected:
  int channels;
  float factor_value;
  T maxValue;
  T offset_value;
};

/**
 * @brief Makes sure that the avg of the signal is set to 0
 * @ingroup convert
 * @tparam T
 */
template <typename T>
class ConverterAutoCenterT : public BaseConverter {
 public:
  ConverterAutoCenterT(int channels = 2, bool isDynamic = false) {
    this->channels = channels;
    this->is_dynamic = isDynamic;
  }

  size_t convert(uint8_t(*src), size_t byte_count) override {
    size_t size = byte_count / channels / sizeof(T);
    T *sample = (T *)src;
    setup((T *)src, size);
    // convert data
    if (is_setup) {
      if (!is_dynamic) {
        for (size_t j = 0; j < size; j++) {
          for (int ch = 0; ch < channels; ch++) {
            sample[(j * channels) + ch] =
                sample[(j * channels) + ch] - offset_to[ch];
          }
        }
      } else {
        for (size_t j = 0; j < size; j++) {
          for (int ch = 0; ch < channels; ch++) {
            sample[(j * channels) + ch] = sample[(j * channels) + ch] -
                                          offset_from[ch] +
                                          (offset_step[ch] * size);
          }
        }
      }
    }
    return byte_count;
  }

 protected:
  Vector<float> offset_from{0};
  Vector<float> offset_to{0};
  Vector<float> offset_step{0};
  Vector<float> total{0};
  float left = 0.0;
  float right = 0.0;
  bool is_setup = false;
  bool is_dynamic;
  int channels;

  void setup(T *src, size_t size) {
    if (size == 0) return;
    if (!is_setup || is_dynamic) {
      if (offset_from.size() == 0) {
        offset_from.resize(channels);
        offset_to.resize(channels);
        offset_step.resize(channels);
        total.resize(channels);
      }

      // save last offset
      for (int ch = 0; ch < channels; ch++) {
        offset_from[ch] = offset_to[ch];
        total[ch] = 0.0;
      }

      // calculate new offset
      for (size_t j = 0; j < size; j++) {
        for (int ch = 0; ch < channels; ch++) {
          total[ch] += src[(j * channels) + ch];
        }
      }
      for (int ch = 0; ch < channels; ch++) {
        offset_to[ch] = total[ch] / size;
        offset_step[ch] = (offset_to[ch] - offset_from[ch]) / size;
      }
      is_setup = true;
    }
  }
};

/**
 * @brief Makes sure that the avg of the signal is set to 0
 * @ingroup convert
 */
class ConverterAutoCenter : public BaseConverter {
 public:
  ConverterAutoCenter() = default;

  ConverterAutoCenter(AudioInfo info) {
    begin(info.channels, info.bits_per_sample);
  }

  ConverterAutoCenter(int channels, int bitsPerSample) {
    begin(channels, bitsPerSample);
  }
  ~ConverterAutoCenter() {
    if (p_converter != nullptr) {
      delete p_converter;
      p_converter = nullptr;
    }
  }

  void begin(int channels, int bitsPerSample, bool isDynamic = false) {
    this->channels = channels;
    this->bits_per_sample = bitsPerSample;
    if (p_converter != nullptr) delete p_converter;
    switch (bits_per_sample) {
      case 8: {
        p_converter = new ConverterAutoCenterT<int8_t>(channels, isDynamic);
        break;
      }
      case 16: {
        p_converter = new ConverterAutoCenterT<int16_t>(channels, isDynamic);
        break;
      }
      case 24: {
        p_converter = new ConverterAutoCenterT<int24_t>(channels, isDynamic);
        break;
      }
      case 32: {
        p_converter = new ConverterAutoCenterT<int32_t>(channels, isDynamic);
        break;
      }
    }
  }

  size_t convert(uint8_t *src, size_t size) override {
    if (p_converter == nullptr) return 0;
    return p_converter->convert(src, size);
  }

 protected:
  int channels;
  int bits_per_sample;
  BaseConverter *p_converter = nullptr;
};

/**
 * @brief Switches the left and right channel
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 * @tparam T
 */
template <typename T>
class ConverterSwitchLeftAndRight : public BaseConverter {
 public:
  ConverterSwitchLeftAndRight(int channels = 2) { this->channels = channels; }

  size_t convert(uint8_t *src, size_t byte_count) override {
    if (channels == 2) {
      int size = byte_count / channels / sizeof(T);
      T *sample = (T *)src;
      for (size_t j = 0; j < size; j++) {
        T temp = *sample;
        *sample = *(sample + 1);
        *(sample + 1) = temp;
        sample += 2;
      }
    }
    return byte_count;
  }

 protected:
  int channels = 2;
};

/**
 * @brief Configure ConverterFillLeftAndRight
 * @ingroup convert
 */
enum FillLeftAndRightStatus { Auto, LeftIsEmpty, RightIsEmpty };

/**
 * @brief Make sure that both channels contain any data
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 * @tparam T
 */

template <typename T>
class ConverterFillLeftAndRight : public BaseConverter {
 public:
  ConverterFillLeftAndRight(FillLeftAndRightStatus config = Auto,
                            int channels = 2) {
    this->channels = channels;
    switch (config) {
      case LeftIsEmpty:
        left_empty = true;
        right_empty = false;
        is_setup = true;
        break;
      case RightIsEmpty:
        left_empty = false;
        right_empty = true;
        is_setup = true;
        break;
      case Auto:
        is_setup = false;
        break;
    }
  }

  size_t convert(uint8_t *src, size_t byte_count) {
    if (channels == 2) {
      int size = byte_count / channels / sizeof(T);
      setup((T *)src, size);
      if (left_empty && !right_empty) {
        T *sample = (T *)src;
        for (size_t j = 0; j < size; j++) {
          *sample = *(sample + 1);
          sample += 2;
        }
      } else if (!left_empty && right_empty) {
        T *sample = (T *)src;
        for (size_t j = 0; j < size; j++) {
          *(sample + 1) = *sample;
          sample += 2;
        }
      }
    }
    return byte_count;
  }

 private:
  bool is_setup = false;
  bool left_empty = true;
  bool right_empty = true;
  int channels;

  void setup(T *src, size_t size) {
    if (!is_setup) {
      for (int j = 0; j < size; j++) {
        if (*src != 0) {
          left_empty = false;
          break;
        }
        src += 2;
      }
      for (int j = 0; j < size - 1; j++) {
        if (*(src) != 0) {
          right_empty = false;
          break;
        }
        src += 2;
      }
      // stop setup as soon as we found some data
      if (!right_empty || !left_empty) {
        is_setup = true;
      }
    }
  }
};

/**
 * @brief special case for internal DAC output, the incomming PCM buffer needs
 *  to be converted from signed 16bit to unsigned
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 * @tparam T
 */
template <typename T>
class ConverterToInternalDACFormat : public BaseConverter {
 public:
  ConverterToInternalDACFormat(int channels = 2) { this->channels = channels; }

  size_t convert(uint8_t *src, size_t byte_count) override {
    int size = byte_count / channels / sizeof(T);
    T *sample = (T *)src;
    for (int i = 0; i < size; i++) {
      for (int j = 0; j < channels; j++) {
        *sample = *sample + 0x8000;
        sample++;
      }
    }
    return byte_count;
  }

 protected:
  int channels;
};

/**
 * @brief We combine a datastream which consists of multiple channels into less
 * channels. E.g. 2 to 1 The last target channel will contain the combined
 * values of the exceeding source channels.
 * @ingroup convert
 * @tparam T
 */
template <typename T>
class ChannelReducerT : public BaseConverter {
 public:
  ChannelReducerT() = default;

  ChannelReducerT(int channelCountOfTarget, int channelCountOfSource) {
    from_channels = channelCountOfSource;
    to_channels = channelCountOfTarget;
  }

  void setSourceChannels(int channelCountOfSource) {
    from_channels = channelCountOfSource;
  }

  void setTargetChannels(int channelCountOfTarget) {
    to_channels = channelCountOfTarget;
  }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    LOGD("convert %d -> %d", from_channels, to_channels);
    assert(to_channels <= from_channels);
    int frame_count = size / (sizeof(T) * from_channels);
    size_t result_size = 0;
    T *result = (T *)target;
    T *source = (T *)src;
    int reduceDiv = from_channels - to_channels + 1;

    for (int i = 0; i < frame_count; i++) {
      // copy first to_channels-1
      for (int j = 0; j < to_channels - 1; j++) {
        *result++ = *source++;
        result_size += sizeof(T);
      }
      // commbined last channels
      T total = (int16_t)0;
      for (int j = to_channels - 1; j < from_channels; j++) {
        total += *source++ / reduceDiv;
      }
      *result++ = total;
      result_size += sizeof(T);
    }
    return result_size;
  }

 protected:
  int from_channels;
  int to_channels;
};

/**
 * @brief We combine a datastream which consists of multiple channels into less
 * channels. E.g. 2 to 1 The last target channel will contain the combined
 * values of the exceeding source channels.
 * @ingroup convert
 */
class ChannelReducer : public BaseConverter {
 public:
  ChannelReducer(int channelCountOfTarget, int channelCountOfSource,
                 int bitsPerSample) {
    from_channels = channelCountOfSource;
    to_channels = channelCountOfTarget;
    bits = bitsPerSample;
  }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    switch (bits) {
      case 8: {
        ChannelReducerT<int8_t> cr8(to_channels, from_channels);
        return cr8.convert(target, src, size);
      }
      case 16: {
        ChannelReducerT<int16_t> cr16(to_channels, from_channels);
        return cr16.convert(target, src, size);
      }
      case 24: {
        ChannelReducerT<int24_t> cr24(to_channels, from_channels);
        return cr24.convert(target, src, size);
      }
      case 32: {
        ChannelReducerT<int32_t> cr32(to_channels, from_channels);
        return cr32.convert(target, src, size);
      }
    }
    return 0;
  }

 protected:
  int from_channels;
  int to_channels;
  int bits;
};

/**
 * @brief Provides reduced sampling rates
 * @ingroup convert
 */
template <typename T>
class DecimateT : public BaseConverter {
 public:
  DecimateT(int factor, int channels) {
    setChannels(channels);
    setFactor(factor);
    count = 0; // Initialize count to 0
  }

  /// Defines the number of channels
  void setChannels(int channels) { this->channels = channels; }

  /// Sets the factor: e.g. with 4 we keep every fourth sample
  void setFactor(int factor) { this->factor = factor; }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {

    if (size % (sizeof(T) * channels) > 0) {
      LOGE("Buffer size %d is not a multiple of the number of channels %d", (int)size, channels);
      return 0;
    }

    int frame_count = size / (sizeof(T) * channels);
    T *p_target = (T *)target;
    T *p_source = (T *)src;
    size_t result_size = 0;

    for (int i = 0; i < frame_count; i++) {
      if (++count == factor) {
        count = 0;
        // Only keep every "factor" samples
        for (int ch = 0; ch < channels; ch++) {
          *p_target++ = p_source[i * channels + ch]; // Corrected indexing
          result_size += sizeof(T);
        }
      }
    }

    LOGD("decimate %d: %d -> %d bytes",factor, (int)size, (int)result_size);
    return result_size;
  }

  operator bool() { return factor > 1; }

 protected:
  int channels = 2;
  int factor = 1;
  uint16_t count;
};

/**
 * @brief Provides a reduced sampling rate by taking a sample at every factor location (ingoring factor-1 samples)
 * @ingroup convert
 */

class Decimate : public BaseConverter {
 public:
  Decimate() = default;
  Decimate(int factor, int channels, int bits_per_sample) {
    setFactor(factor);
    setChannels(channels);
    setBits(bits_per_sample);
  }
  /// Defines the number of channels
  void setChannels(int channels) { this->channels = channels; }
  void setBits(int bits) { this->bits = bits; }
  /// Sets the factor: e.g. with 4 we keep every forth sample
  void setFactor(int factor) { this->factor = factor; }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }
  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    switch (bits) {
      case 8: {
        DecimateT<int8_t> dec8(factor, channels);
        return dec8.convert(target, src, size);
      }
      case 16: {
        DecimateT<int16_t> dec16(factor, channels);
        return dec16.convert(target, src, size);
      }
      case 24: {
        DecimateT<int24_t> dec24(factor, channels);
        return dec24.convert(target, src, size);
      }
      case 32: {
        DecimateT<int32_t> dec32(factor, channels);
        return dec32.convert(target, src, size);
      }
    }
    return 0;
  }

  operator bool() { return factor > 1; };

 protected:
  int channels = 2;
  int bits = 16;
  int factor = 1;
};

/**
 * @brief We reduce the number of samples in a datastream by summing (binning) or averaging.
 * This will result in the same number of channels but binSize times less samples.
 * If Average is true the sum is divided by binSize.
 * @author Urs Utzinger
 * @ingroup convert
 * @tparam T
 */

// Helper template to define the integer type for the summation based on input
// data type T
template <typename T>
struct AppropriateSumType {
    using type = T;
};

template <>
struct AppropriateSumType<int8_t> {
    using type = int16_t;
};

template <>
struct AppropriateSumType<int16_t> {
    using type = int32_t;
};

template <>
struct AppropriateSumType<int24_t> {
    using type = int32_t; // Assuming int24_t is a custom 24-bit integer type
};

template <>
struct AppropriateSumType<int32_t> {
    using type = int64_t;
};

template <typename T>
class BinT : public BaseConverter {
 public:
  BinT() = default;
  BinT(int binSize, int channels, bool average) {
    setChannels(channels);
    setBinSize(binSize);
    setAverage(average); 
    this->partialBinSize = 0;
    //this->partialBin = new T[channels];
    //std::fill(this->partialBin, this->partialBin + channels, 0); // Initialize partialBin with zeros
    this->partialBin = new T[channels]();
  }

  ~BinT() {
    delete[] this->partialBin;
  }

  void setChannels(int channels) { this->channels = channels; }
  void setBinSize(int binSize) { this->binSize = binSize; }
  void setAverage(bool average) { this->average = average; }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    // The binning takes the following into account
    //  1) if size is too small it will add up data to partialBin and return 0 size
    //  2) if there is sufficient data to fill Bins but there is partial data remaining it will be added to the partial Bin
    //  3) if there was previous partial Bin it will be filled with the new data
    //  4) if there is insufficient new data to fill the partial Bin it will fill the partial Bin with the new data

    if (size % (sizeof(T) * channels) > 0) {
      LOGE("Buffer size %d is not a multiple of the number of channels %d", (int)size, channels);
      return 0;
    }

    int sample_count = size / (sizeof(T) * channels);            // new available samples in each channel
    int total_samples = partialBinSize + sample_count;           // total samples available for each channel including previous number of sample in partial bin
    int bin_count = total_samples / binSize;                     // number of bins we can make
    int remaining_samples = total_samples % binSize;             // remaining samples after binning
    T *p_target = (T *)target;
    T *p_source = (T *)src;
    size_t result_size = 0;

    // Allocate sum for each channel with appropriate type
    typename AppropriateSumType<T>::type sums[channels];
    int current_sample = 0;                                      // current sample index

    // Is there a partial bin from the previous call?
    // ----
    if (partialBinSize > 0) {

      int  samples_needed = binSize - partialBinSize;
      bool have_enough_samples = (samples_needed < sample_count);
      int  samples_to_bin = have_enough_samples ? samples_needed : sample_count;

      for (int ch = 0; ch < channels; ch++) {
        sums[ch] = partialBin[ch];
      }

      for (int j = 0; j < samples_to_bin; j++) {
        for (int ch = 0; ch < channels; ch++) {
          sums[ch] += p_source[current_sample * channels + ch];
        }
        current_sample++;
      }

      if (have_enough_samples) {
        // Store the completed bin
        if (average) {
          for (int ch = 0; ch < channels; ch++) {
            p_target[result_size / sizeof(T)] = static_cast<T>(sums[ch] / binSize);
            result_size += sizeof(T);
          }
        } else {
          for (int ch = 0; ch < channels; ch++) {
            p_target[result_size / sizeof(T)] = static_cast<T>(sums[ch]);
            result_size += sizeof(T);
          }
        }
        partialBinSize = 0;
      } else {
        // Not enough samples to complete the bin, update partialBin
        for (int ch = 0; ch < channels; ch++) {
          partialBin[ch] = sums[ch];
        }
        partialBinSize += current_sample;

        LOGD("bin %d: %d of %d remaining %d samples, %d > %d bytes", binSize, current_sample, total_samples, partialBinSize, (int)size, (int)result_size);
        return result_size;
      }
    }

    // Fill bins
    // ----
    for (int i = 0; i < bin_count; i++) {
      for (int ch = 0; ch < channels; ch++) {
        sums[ch] = p_source[current_sample * channels + ch]; // Initialize sums with first value in the input buffer
      }

      for (int j = 1; j < binSize; j++) {
        for (int ch = 0; ch < channels; ch++) {
          sums[ch] += p_source[(current_sample + j) * channels + ch];
        }
      }
      current_sample += binSize;

      // Store the bin result
      if (average) {
        for (int ch = 0; ch < channels; ch++) {
          p_target[result_size / sizeof(T)] = static_cast<T>(sums[ch] / binSize);
          result_size += sizeof(T);
        } 
      } else {
        for (int ch = 0; ch < channels; ch++) {
          p_target[result_size / sizeof(T)] = static_cast<T>(sums[ch]);
          result_size += sizeof(T);
        }
      }
    }

    // Store the remaining samples in the partial bin
    // ----
    for (int i = 0; i < remaining_samples; i++) {
      for (int ch = 0; ch < channels; ch++) {
        partialBin[ch] += p_source[(current_sample + i) * channels + ch];
      }
    }
    partialBinSize = remaining_samples;

    LOGD("bin %d: %d of %d remaining %d samples, %d > %d bytes", binSize, current_sample, total_samples,  partialBinSize, (int) size, (int) result_size);

    return result_size;
  }

 protected:
  int channels = 2;
  int binSize = 1;
  bool average = true;
  T *partialBin;
  int partialBinSize;  
};

/**
 * @brief Provides reduced sampling rates through binning
 * @ingroup convert
 */

class Bin : public BaseConverter {
 public:
  Bin() = default;
  Bin(int binSize, int channels, bool average, int bits_per_sample) {
    setChannels(channels);
    setBinSize(binSize);
    setAverage(average);
    setBits(bits_per_sample);    
  }

  void setChannels(int channels) { this->channels = channels; }
  void setBits(int bits) { this->bits = bits; }
  void setBinSize(int binSize) { this->binSize = binSize; }
  void setAverage(bool average) { this->average = average; }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }
  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    switch (bits) {
      case 8: {
        BinT<int8_t> bin8(binSize, channels, average);
        return bin8.convert(target, src, size);
      }
      case 16: {
        BinT<int16_t> bin16(binSize, channels, average);
        return bin16.convert(target, src, size);
      }
      case 24: {
        BinT<int24_t> bin24(binSize, channels, average);
        return bin24.convert(target, src, size);
      }
      case 32: {
        BinT<int32_t> bin32(binSize, channels, average);
        return bin32.convert(target, src, size);
      }
      default: {
        LOGE("Number of bits %d not supported.", bits);
        return 0;
      }
    }
  }

 protected:
  int channels = 2;
  int bits = 16;
  int binSize = 1;
  bool average = false;
};

/**
 * @brief We calculate the difference between pairs of channels in a datastream.
 *  E.g. if we have 4 channels we end up with 2 channels. 
 *  The channels will be
 *  channel_1 - channel_2 
 *  channel_3 - channel_4
 * This is similar to background subtraction between two channels but will 
 * also work for quadric, sexic or octic audio.
 * This will not work if you provide single channel data!
 * @author Urs Utzinger
 * @ingroup convert
 * @tparam T
 */

template <typename T>
class ChannelDiffT : public BaseConverter {
 public:
  ChannelDiffT() {}

  size_t convert(uint8_t *src, size_t size) override { return convert(src, src, size); }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    LOGD("channel subtract %d samples, %d bytes", (int) (size / sizeof(T)), (int) size);

    // Ensure the buffer size is even for pairs of channels
    if (size % (sizeof(T) * 2) > 0) {
      LOGE("Buffer size is not even");
      return 0;
    }

    int sample_count = size / (sizeof(T) * 2); // Each pair of channels produces one output sample
    T *p_result = (T *)target;
    T *p_source = (T *)src;

    for (int i = 0; i < sample_count; i++) {
      // *p_result++ = *p_source++ - *p_source++;
      auto tmp = *p_source++;
      tmp -= *p_source++;
      *p_result++ = tmp;
    }

    return sizeof(T) * sample_count;
  }
};

class ChannelDiff : public BaseConverter {
 public:
  ChannelDiff() = default;
  ChannelDiff(int bitsPerSample) {
    setBits(bitsPerSample);
  }
  void setBits(int bits) { this->bits = bits; }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }
  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    switch (bits) {
      case 8: {
        ChannelDiffT<int8_t> cd8;
        return cd8.convert(target, src, size);
      }
      case 16: {
        ChannelDiffT<int16_t> cd16;
        return cd16.convert(target, src, size);
      }
      case 24: {
        ChannelDiffT<int24_t> cd24;
        return cd24.convert(target, src, size);
      }
      case 32: {
        ChannelDiffT<int32_t> cd32;
        return cd32.convert(target, src, size);
      }
      default: {
        LOGE("Number of bits %d not supported.", bits);
        return 0;
      }
    }
  }

 protected:
  int bits = 16;
};

/**
 * @brief We average pairs of channels in a datastream.
 *  E.g. if we have 4 channels we end up with 2 channels. 
 *  The channels will be 
 *  (channel_1 + channel_2)/2 
 *  (channel_3 - channel_4)/2.
 * This is equivalent of stereo to mono conversion but will 
 * also work for quadric, sexic or octic audio.
 * This will not work if you provide single channel data!
 * @author Urs Utzinger
 * @ingroup convert
 * @tparam T
 */
template <typename T>
class ChannelAvgT : public BaseConverter {
 public:
  ChannelAvgT() {}

  size_t convert(uint8_t *src, size_t size) override { return convert(src, src, size); }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {

    if (size % (sizeof(T) * 2) > 0) {
      LOGE("Buffer size is not even");
      return 0;
    }

    int sample_count = size / (sizeof(T) * 2); // Each pair of channels produces one output sample
    T *p_result = (T *)target;
    T *p_source = (T *)src;

    for (int i = 0; i < sample_count; i++) {
      // *p_result++ = (*p_source++ + *p_source++) / 2; // Average the pair of channels
      auto tmp = *p_source++;
      tmp += *p_source++;
      *p_result++ = tmp / 2;
    }

    LOGD("channel average %d samples, %d bytes", sample_count, (int)size);

    return sizeof(T) * sample_count;
  }
};

class ChannelAvg : public BaseConverter {
 public:
  ChannelAvg() = default;
  ChannelAvg(int bitsPerSample) {
    setBits(bitsPerSample);
  }
  void setBits(int bits) { this->bits = bits; }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }
  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    switch (bits) {
      case 8: {
        ChannelAvgT<int8_t> ca8;
        return ca8.convert(target, src, size);
      }
      case 16: {
        ChannelAvgT<int16_t> ca16;
        return ca16.convert(target, src, size);
      }
      case 24: {
        ChannelAvgT<int24_t> ca24;
        return ca24.convert(target, src, size);
      }
      case 32: {
        ChannelAvgT<int32_t> ca32;
        return ca32.convert(target, src, size);
      }
      default: {
        LOGE("Number of bits %d not supported.", bits);
        return 0;
      }
    }
  }

 protected:
  int bits = 16;
};

/**
 * @brief We first bin the channels then we calculate the difference between pairs of channels in a datastream.
 *  E.g. For binning, if we bin 4 samples in each channel we will have 4 times less samples per channel
 *  E.g. For subtracting if we have 4 channels we end up with 2 channels. 
 *    The channels will be
 *    channel_1 - channel_2 
 *    channel_3 - channel_4
 * This is the same as combining binning and subtracting channels.
 * This will not work if you provide single channel data!
 * @author Urs Utzinger
 * @ingroup convert
 * @tparam T
 */

template <typename T>
class ChannelBinDiffT : public BaseConverter {
 public:
  ChannelBinDiffT() = default;
  ChannelBinDiffT(int binSize, int channels, bool average) {
    setChannels(channels);
    setBinSize(binSize);
    setAverage(average); 
    this->partialBinSize = 0;
    //this->partialBin = new T[channels];
    //std::fill(this->partialBin, this->partialBin + channels, 0); // Initialize partialBin with zeros
    this->partialBin = new T[channels]();
  }

  ~ChannelBinDiffT() {
    delete[] this->partialBin;
  }

  void setChannels(int channels) { 
        if ((channels % 2) > 0) {
          LOGE("Number of channels needs to be even");
          this->channels = channels+1; 
        }
        this->channels = channels; 
  }
  void setBinSize(int binSize) { this->binSize = binSize; }
  void setAverage(bool average) { this->average = average; }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    // The binning works the same as in the BinT class
    // Here we add subtraction before we store the bins

    if (size % (sizeof(T) * channels) > 0) {
      LOGE("Buffer size needs to be multiple of channels")
      return 0;
    }

    int sample_count = size / (sizeof(T) * channels);            // new available samples in each channel
    int total_samples = partialBinSize + sample_count;           // total samples available for each channel including previous number of sample in partial bin
    int bin_count = total_samples / binSize;                     // number of bins we can make
    int remaining_samples = total_samples % binSize;             // remaining samples after binning
    T *p_target = (T *)target;
    T *p_source = (T *)src;
    size_t result_size = 0;

    // Allocate sum for each channel with appropriate type
    typename AppropriateSumType<T>::type sums[channels];
    int current_sample = 0;                                      // current sample index

    // Is there a partial bin from the previous call?
    // ----
    if (partialBinSize > 0) {

      // LOGD("Deal with partial bins");

      int  samples_needed = binSize - partialBinSize;
      bool have_enough_samples = (samples_needed < sample_count);
      int  samples_to_bin = have_enough_samples ? samples_needed : sample_count;

      // initialize
      for (int ch = 0; ch < channels; ch++) {
        sums[ch] = partialBin[ch];
      }

      // continue binning
      for (int j = 0; j < samples_to_bin; j++) {
        for (int ch = 0; ch < channels; ch++) {
          sums[ch] += p_source[current_sample * channels + ch];
        }
        current_sample++;
      }

      // store the bin results or update the partial bin
      if (have_enough_samples) {
        // Subtract two channels and store the completed bin
        if (average) {
          for (int ch = 0; ch < channels; ch+=2) {
            p_target[result_size / sizeof(T)] = static_cast<T>((sums[ch] - sums[ch+1]) / binSize);
            result_size += sizeof(T);
          }
        } else {
          for (int ch = 0; ch < channels; ch+=2) {
            p_target[result_size / sizeof(T)] = static_cast<T>((sums[ch] - sums[ch+1]));
            result_size += sizeof(T);
          }
        }
        partialBinSize = 0;
        // LOGD("Partial bins are empty");

      } else {
        // Not enough samples to complete the bin, update partialBin
        for (int ch = 0; ch < channels; ch++) {
          partialBin[ch] = sums[ch];
        }
        partialBinSize += current_sample;
        LOGD("bin & channel subtract %d: %d of %d remaining %d samples, %d > %d bytes", binSize, current_sample, total_samples, partialBinSize,(int) size,(int) result_size);
        // LOGD("Partial bins were updated");
        return result_size;
      }
    }

    // Fill bins
    // ----
    // LOGD("Fillin bins");
    for (int i = 0; i < bin_count; i++) {

      // LOGD("Current sample %d", current_sample);
      
      for (int ch = 0; ch < channels; ch++) {
        sums[ch] = p_source[current_sample * channels + ch]; // Initialize sums with first value in the input buffer
      }

      for (int j = 1; j < binSize; j++) {
        for (int ch = 0; ch < channels; ch++) {
          sums[ch] += p_source[(current_sample + j) * channels + ch];
        }
      }
      current_sample += binSize;

      // Finish binning, then subtact two channel and store the result
      if (average) {
        for (int ch = 0; ch < channels; ch+=2) {
          p_target[result_size / sizeof(T)] = static_cast<T>((sums[ch]-sums[ch+1]) / binSize);
          result_size += sizeof(T);
        }
      } else {
        for (int ch = 0; ch < channels; ch+=2) {
          p_target[result_size / sizeof(T)] = static_cast<T>((sums[ch]-sums[ch+1]));
          result_size += sizeof(T);
        }
      }  
    }

    // Store the remaining samples in the partial bin
    // ----
    // LOGD("Updating partial bins");
    for (int i = 0; i < remaining_samples; i++) {
      for (int ch = 0; ch < channels; ch++) {
        partialBin[ch] += p_source[(current_sample + i) * channels + ch];
      }
    }
    partialBinSize = remaining_samples;

    LOGD("bin & channel subtract %d: %d of %d remaining %d samples, %d > %d bytes", binSize, current_sample, total_samples, partialBinSize, (int)size, (int) result_size);

    return result_size;
  }

 protected:
  int channels = 2;
  int binSize = 4;
  bool average = true;
  T *partialBin;
  int partialBinSize;  
};

/**
 * @brief Provides combination of binning and subtracting channels
 * @author Urs Utzinger
 * @ingroup convert
 * @tparam T
 */

class ChannelBinDiff : public BaseConverter {
 public:
  ChannelBinDiff() = default;
  ChannelBinDiff(int binSize, int channels, bool average, int bits_per_sample) {
    setChannels(channels);
    setBinSize(binSize);
    setAverage(average);
    setBits(bits_per_sample);    
  }

  void setChannels(int channels) { 
    if ((channels % 2) == 0) {
      this->channels = channels; 
    } else {
      LOGE("Number of channels needs to be even");
      this->channels = channels+1; 
    }
  }

  void setBits(int bits) { this->bits = bits; }
  void setBinSize(int binSize) { this->binSize = binSize; }
  void setAverage(bool average) { this->average = average; }

  size_t convert(uint8_t *src, size_t size) { return convert(src, src, size); }
  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    switch (bits) {
      case 8: {
        ChannelBinDiffT<int8_t> bd8(binSize, channels, average);
        return bd8.convert(target, src, size);
      }
      case 16: {
        ChannelBinDiffT<int16_t> bd16(binSize, channels, average);
        return bd16.convert(target, src, size);
      }
      case 24: {
        ChannelBinDiffT<int24_t> bd24(binSize, channels, average);
        return bd24.convert(target, src, size);
      }
      case 32: {
        ChannelBinDiffT<int32_t> bd32(binSize, channels, average);
        return bd32.convert(target, src, size);
      }
      default: {
        LOGE("Number of bits %d not supported.", bits);
        return 0;
      }
    }
  }

 protected:
  int channels = 2;
  int bits = 16;
  int binSize = 4;
  bool average = true;
};


/**
 * @brief Increases the channel count
 * @ingroup convert
 * @tparam T
 */
template <typename T>
class ChannelEnhancer {
 public:
  ChannelEnhancer() = default;

  ChannelEnhancer(int channelCountOfTarget, int channelCountOfSource) {
    from_channels = channelCountOfSource;
    to_channels = channelCountOfTarget;
  }

  void setSourceChannels(int channelCountOfSource) {
    from_channels = channelCountOfSource;
  }

  void setTargetChannels(int channelCountOfTarget) {
    to_channels = channelCountOfTarget;
  }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    int frame_count = size / (sizeof(T) * from_channels);
    size_t result_size = 0;
    T *result = (T *)target;
    T *source = (T *)src;
    T value = (int16_t)0;
    for (int i = 0; i < frame_count; i++) {
      // copy available channels
      for (int j = 0; j < from_channels; j++) {
        value = *source++;
        *result++ = value;
        result_size += sizeof(T);
      }
      // repeat last value
      for (int j = from_channels; j < to_channels; j++) {
        *result++ = value;
        result_size += sizeof(T);
      }
    }
    return result_size;
  }

  /// Determine the size of the conversion result
  size_t resultSize(size_t inSize) {
    return inSize * to_channels / from_channels;
  }

 protected:
  int from_channels;
  int to_channels;
};

/**
 * @brief Increasing or decreasing the number of channels
 * @ingroup convert
 * @tparam T
 */
template <typename T>
class ChannelConverter {
 public:
  ChannelConverter() = default;

  ChannelConverter(int channelCountOfTarget, int channelCountOfSource) {
    from_channels = channelCountOfSource;
    to_channels = channelCountOfTarget;
  }

  void setSourceChannels(int channelCountOfSource) {
    from_channels = channelCountOfSource;
  }

  void setTargetChannels(int channelCountOfTarget) {
    to_channels = channelCountOfTarget;
  }

  size_t convert(uint8_t *target, uint8_t *src, size_t size) {
    if (from_channels == to_channels) {
      memcpy(target, src, size);
      return size;
    }
    // setup channels
    if (from_channels > to_channels) {
      reducer.setSourceChannels(from_channels);
      reducer.setTargetChannels(to_channels);
    } else {
      enhancer.setSourceChannels(from_channels);
      enhancer.setTargetChannels(to_channels);
    }

    // execute conversion
    if (from_channels > to_channels) {
      return reducer.convert(target, src, size);
    } else {
      return enhancer.convert(target, src, size);
    }
  }

 protected:
  ChannelEnhancer<T> enhancer;
  ChannelReducerT<T> reducer;
  int from_channels;
  int to_channels;
};

/**
 * @brief Combines multiple converters
 * @ingroup convert
 * @tparam T
 */
template <typename T>
class MultiConverter : public BaseConverter {
 public:
  MultiConverter() {}

  MultiConverter(BaseConverter &c1) { add(c1); }

  MultiConverter(BaseConverter &c1, BaseConverter &c2) {
    add(c1);
    add(c2);
  }

  MultiConverter(BaseConverter &c1, BaseConverter &c2, BaseConverter &c3) {
    add(c1);
    add(c2);
    add(c3);
  }

  // adds a converter
  void add(BaseConverter &converter) { converters.push_back(&converter); }

  // The data is provided as int24_t tgt[][2] but  returned as int24_t
  size_t convert(uint8_t *src, size_t size) {
    for (int i = 0; i < converters.size(); i++) {
      converters[i]->convert(src, size);
    }
    return size;
  }

 private:
  Vector<BaseConverter *> converters;
};

// /**
//  * @brief Converts e.g. 24bit data to the indicated smaller or bigger data
//  type
//  * @ingroup convert
//  * @author Phil Schatzmann
//  * @copyright GPLv3
//  *
//  * @tparam T
//  */
// template<typename FromType, typename ToType>
// class FormatConverter {
//     public:
//         FormatConverter(ToType (*converter)(FromType v)){
//             this->convert_ptr = converter;
//         }

//         FormatConverter( float factor, float clip){
//             this->factor = factor;
//             this->clip = clip;
//         }

//         // The data is provided as int24_t tgt[][2] but  returned as int24_t
//         size_t convert(uint8_t *src, uint8_t *target, size_t byte_count_src)
//         {
//             return convert((FromType *)src, (ToType*)target, byte_count_src
//             );
//         }

//         // The data is provided as int24_t tgt[][2] but  returned as int24_t
//         size_t convert(FromType *src, ToType *target, size_t byte_count_src)
//         {
//             int size = byte_count_src / sizeof(FromType);
//             FromType *s = src;
//             ToType *t = target;
//             if (convert_ptr!=nullptr){
//                 // standard conversion
//                 for (int i=size; i>0; i--) {
//                     *t = (*convert_ptr)(*s);
//                     t++;
//                     s++;
//                 }
//             } else {
//                 // conversion using indicated factor
//                 for (int i=size; i>0; i--) {
//                     float tmp = factor * *s;
//                     if (tmp>clip){
//                         tmp=clip;
//                     } else if (tmp<-clip){
//                         tmp = -clip;
//                     }
//                     *t = tmp;
//                     t++;
//                     s++;
//                 }
//             }
//             return size*sizeof(ToType);
//         }

//     private:
//         ToType (*convert_ptr)(FromType v) = nullptr;
//         float factor=0;
//         float clip=0;

// };

/**
 * @brief Reads n numbers from an Arduino Stream
 *
 */
class NumberReader {
 public:
  NumberReader(Stream &in) { stream_ptr = &in; }

  NumberReader() {}

  bool read(int inBits, int outBits, bool outSigned, int n, int32_t *result) {
    bool result_bool = false;
    int len = inBits / 8 * n;
    if (stream_ptr != nullptr && stream_ptr->available() > len) {
      uint8_t buffer[len];
      stream_ptr->readBytes((uint8_t *)buffer, n * len);
      result_bool =
          toNumbers((void *)buffer, inBits, outBits, outSigned, n, result);
    }
    return result_bool;
  }

  /// converts a buffer to a number array
  bool toNumbers(void *bufferIn, int inBits, int outBits, bool outSigned, int n,
                 int32_t *result) {
    bool result_bool = false;
    switch (inBits) {
      case 8: {
        int8_t *buffer = (int8_t *)bufferIn;
        for (int j = 0; j < n; j++) {
          result[j] = scale(buffer[j], inBits, outBits, outSigned);
        }
        result_bool = true;
      } break;
      case 16: {
        int16_t *buffer = (int16_t *)bufferIn;
        for (int j = 0; j < n; j++) {
          result[j] = scale(buffer[j], inBits, outBits, outSigned);
        }
        result_bool = true;
      } break;
      case 32: {
        int32_t *buffer = (int32_t *)bufferIn;
        for (int j = 0; j < n; j++) {
          result[j] = scale(buffer[j], inBits, outBits, outSigned);
        }
        result_bool = true;
      } break;
    }
    return result_bool;
  }

 protected:
  Stream *stream_ptr = nullptr;

  /// scale the value
  int32_t scale(int32_t value, int inBits, int outBits, bool outSigned = true) {
    int32_t result = static_cast<float>(value) /
                     NumberConverter::maxValue(inBits) *
                     NumberConverter::maxValue(outBits);
    if (!outSigned) {
      result += (NumberConverter::maxValue(outBits) / 2);
    }
    return result;
  }
};

/**
 * @brief Converter for 1 Channel which applies the indicated Filter
 * @ingroup convert
 * @author pschatzmann
 * @tparam T
 */
template <typename T>
class Converter1Channel : public BaseConverter {
 public:
  Converter1Channel(Filter<T> &filter) { this->p_filter = &filter; }

  size_t convert(uint8_t *src, size_t size) override {
    T *data = (T *)src;
    for (size_t j = 0; j < size; j++) {
      data[j] = p_filter->process(data[j]);
    }
    return size;
  }

 protected:
  Filter<T> *p_filter = nullptr;
};

/**
 * @brief Converter for n Channels which applies the indicated Filter
 * @ingroup convert
 * @author pschatzmann
 * @tparam T
 */
template <typename T, typename FT>
class ConverterNChannels : public BaseConverter {
 public:
  /// Default Constructor
  ConverterNChannels(int channels) {
    this->channels = channels;
    filters = new Filter<FT> *[channels];
    // make sure that we have 1 filter per channel
    for (int j = 0; j < channels; j++) {
      filters[j] = nullptr;
    }
  }

  /// Destructor
  ~ConverterNChannels() {
    for (int j = 0; j < channels; j++) {
      if (filters[j] != nullptr) {
        delete filters[j];
      }
    }
    delete[] filters;
    filters = 0;
  }

  /// defines the filter for an individual channel - the first channel is 0
  void setFilter(int channel, Filter<FT> *filter) {
    if (channel < channels) {
      if (filters[channel] != nullptr) {
        delete filters[channel];
      }
      filters[channel] = filter;
    } else {
      LOGE("Invalid channel nummber %d - max channel is %d", channel,
           channels - 1);
    }
  }

  // convert all samples for each channel separately
  size_t convert(uint8_t *src, size_t size) {
    int count = size / channels / sizeof(T);
    T *sample = (T *)src;
    for (size_t j = 0; j < count; j++) {
      for (int channel = 0; channel < channels; channel++) {
        if (filters[channel] != nullptr) {
          *sample = filters[channel]->process(*sample);
        }
        sample++;
      }
    }
    return size;
  }

  int getChannels() { return channels; }

 protected:
  Filter<FT> **filters = nullptr;
  int channels;
};

/**
 * @brief Removes any silence from the buffer that is longer then n samples with
 * a amplitude below the indicated threshhold. If you process multiple channels
 * you need to multiply the channels with the number of samples to indicate n
 * @ingroup convert
 * @tparam T
 */

template <typename T>
class SilenceRemovalConverter : public BaseConverter {
 public:
  SilenceRemovalConverter(int n = 8, int aplidudeLimit = 2) {
    set(n, aplidudeLimit);
  }

  virtual size_t convert(uint8_t *data, size_t size) override {
    if (!active) {
      // no change to the data
      return size;
    }
    size_t sample_count = size / sizeof(T);
    size_t write_count = 0;
    T *audio = (T *)data;

    // find relevant data
    T *p_buffer = (T *)data;
    for (int j = 0; j < sample_count; j++) {
      int pos = findLastAudioPos(audio, j);
      if (pos < n) {
        write_count++;
        *p_buffer++ = audio[j];
      }
    }

    // write audio data w/o silence
    size_t write_size = write_count * sizeof(T);
    LOGI("filtered silence from %d -> %d", (int)size, (int)write_size);

    // number of empty samples of prior buffer
    priorLastAudioPos = findLastAudioPos(audio, sample_count - 1);

    // return new data size
    return write_size;
  }

 protected:
  bool active = false;
  const uint8_t *buffer = nullptr;
  int n;
  int priorLastAudioPos = 0;
  int amplidude_limit = 0;

  void set(int n = 5, int aplidudeLimit = 2) {
    LOGI("begin(n=%d, aplidudeLimit=%d", n, aplidudeLimit);
    this->n = n;
    this->amplidude_limit = aplidudeLimit;
    this->priorLastAudioPos = n + 1;  // ignore first values
    this->active = n > 0;
  }

  // find last position which contains audible data
  int findLastAudioPos(T *audio, int pos) {
    for (int j = 0; j < n; j++) {
      // we are before the start of the current buffer
      if (pos - j <= 0) {
        return priorLastAudioPos;
      }
      // we are in the current buffer
      if (abs(audio[pos - j]) > amplidude_limit) {
        return j;
      }
    }
    return n + 1;
  }
};

/**
 * @brief Big value gaps (at the beginning and the end of a recording) can lead
 * to some popping sounds. We will try to set the values to 0 until the first
 * transition thru 0 of the audio curve
 * @ingroup convert
 * @tparam T
 */
template <typename T>
class PoppingSoundRemover : public BaseConverter {
 public:
  PoppingSoundRemover(int channels, bool fromBeginning, bool fromEnd) {
    this->channels = channels;
    from_beginning = fromBeginning;
    from_end = fromEnd;
  }
  virtual size_t convert(uint8_t *src, size_t size) {
    for (int ch = 0; ch < channels; ch++) {
      if (from_beginning)
        clearUpTo1stTransition(channels, ch, (T *)src, size / sizeof(T));
      if (from_end)
        clearAfterLastTransition(channels, ch, (T *)src, size / sizeof(T));
    }
    return size;
  }

 protected:
  bool from_beginning;
  bool from_end;
  int channels;

  void clearUpTo1stTransition(int channels, int channel, T *values,
                              int sampleCount) {
    T first = values[channel];
    for (int j = 0; j < sampleCount; j += channels) {
      T act = values[j];
      if ((first <= 0.0 && act >= 0.0) || (first >= 0.0 && act <= 0.0)) {
        // we found the last transition so we are done
        break;
      } else {
        values[j] = 0;
      }
    }
  }

  void clearAfterLastTransition(int channels, int channel, T *values,
                                int sampleCount) {
    int lastPos = sampleCount - channels + channel;
    T last = values[lastPos];
    for (int j = lastPos; j >= 0; j -= channels) {
      T act = values[j];
      if ((last <= 0.0 && act >= 0.0) || (last >= 0.0 && act <= 0.0)) {
        // we found the last transition so we are done
        break;
      } else {
        values[j] = 0;
      }
    }
  }
};

/**
 * @brief Changes the samples at the beginning or at the end to slowly ramp up
 * the volume
 * @ingroup convert
 * @tparam T
 */
template <typename T>
class SmoothTransition : public BaseConverter {
 public:
  SmoothTransition(int channels, bool fromBeginning, bool fromEnd,
                   float inc = 0.01) {
    this->channels = channels;
    this->inc = inc;
    from_beginning = fromBeginning;
    from_end = fromEnd;
  }
  virtual size_t convert(uint8_t *src, size_t size) {
    for (int ch = 0; ch < channels; ch++) {
      if (from_beginning)
        processStart(channels, ch, (T *)src, size / sizeof(T));
      if (from_end) processEnd(channels, ch, (T *)src, size / sizeof(T));
    }
    return size;
  }

 protected:
  bool from_beginning;
  bool from_end;
  int channels;
  float inc = 0.01;
  float factor = 0;

  void processStart(int channels, int channel, T *values, int sampleCount) {
    for (int j = 0; j < sampleCount; j += channels) {
      if (factor >= 0.8) {
        break;
      } else {
        values[j] = factor * values[j];
      }
      factor += inc;
    }
  }

  void processEnd(int channels, int channel, T *values, int sampleCount) {
    int lastPos = sampleCount - channels + channel;
    for (int j = lastPos; j >= 0; j -= channels) {
      if (factor >= 0.8) {
        break;
      } else {
        values[j] = factor * values[j];
      }
    }
  }
};

/**
 * @brief Copy channel Cx value of type T shifted by S bits to all Cn channels
 * @ingroup convert
 * @tparam T, Cn, Cx, S
 */
template <typename T, size_t Cn, size_t Cx, size_t S>
class CopyChannels : public BaseConverter {
 public:
  CopyChannels() : _max_val(0), _counter(0), _prev_ms(0) {}

  size_t convert(uint8_t *src, size_t size) {
    T *chan = (T *)src;
    size_t samples = (size / Cn) / sizeof(T);
    for (size_t s = 0; s < samples; s++) {
      chan[s * Cn + Cx] = (Cx < Cn) ? chan[s * Cn + Cx] << S : 0;

      for (size_t c = 0; c < Cn; c++) {
        if (c != Cx) {
          chan[s * Cn + c] = chan[s * Cn + Cx];
        }
      }

      if (_max_val < chan[s * Cn]) {
        _max_val = chan[s * Cn];
      }

      _counter++;
      uint32_t now = millis();
      if (now - _prev_ms > 1000) {
        _prev_ms = now;
        LOGI("CopyChannels samples: %u, amplitude: %d", _counter, _max_val);
        _max_val = 0;
      }
    }
    return samples * Cn * sizeof(T);
  }

 private:
  T _max_val;
  uint32_t _counter;
  uint32_t _prev_ms;
};

/**
 * @brief You can provide a lambda expression to conver the data
 * @ingroup convert
 * @tparam T
*/
template <typename T>
class CallbackConverterT : public BaseConverter {
  public:
  CallbackConverterT(T(*callback)(T in, int channel), int channels = 2) {
    this->callback = callback;
    this->channels = channels;
  }

  size_t convert(uint8_t *src, size_t size) {
    int samples = size / sizeof(T);
    for (int j = 0; j < samples; j++) {
      src[j] = callback(src[j], j % channels);
    }
    return size;
  }

 protected:
  T(*callback)(T in, int channel);
  int channels;
};

}  // namespace audio_tools
