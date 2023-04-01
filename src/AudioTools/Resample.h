#pragma once

#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief ConverterStream Helper class which implements the converting
 * readBytes with the help of write.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T class name of the original transformer stream
 */
template <class T>
class TransformationReader {
 public:
  /// @brief setup of the TransformationReader class
  /// @param transform The original transformer stream
  /// @param byteFactor The factor of how much more data we need to read to get
  /// the requested converted bytes
  /// @param source The data source of the data to be converted
  void begin(T *transform, float byteFactor, Stream *source) {
    TRACED();
    active = true;
    byte_factor = byteFactor;
    p_stream = source;
    p_transform = transform;
    if (transform == nullptr) {
      LOGE("transform is NULL");
      active = false;
    }
    if (p_stream == nullptr) {
      LOGE("p_stream is NULL");
      active = false;
    }
  }

  size_t readBytes(uint8_t *data, size_t byteCount) {
    LOGD("TransformationReader::readBytes: %d", byteCount);
    if (!active) {
      LOGE("inactive");
      return 0;
    }
    if (p_stream == nullptr) {
      LOGE("p_stream is NULL");
      return 0;
    }
    int read_size = byte_factor * byteCount;
    buffer.resize(read_size);
    int read = p_stream->readBytes(buffer.data(), read_size);
    assert(read == read_size);
    Print *tmp = setupOutput(data, byteCount);
    size_t result_size = p_transform->write(buffer.data(), read_size);
    restoreOutput(tmp);
    return print_to_array.totalBytesWritten();
  }

  int availableForWrite() { return print_to_array.availableForWrite(); }

 protected:
  class AdapterPrintToArray : public AudioPrint {
   public:
    void begin(uint8_t *array, size_t data_len) {
      TRACED();
      p_data = array;
      max_len = data_len;
      pos = 0;
    }

    int availableForWrite() override { return max_len; }

    size_t write(const uint8_t *data, size_t byteCount) override {
      LOGD("AdapterPrintToArray::write: %d (%d)", byteCount, pos);
      if (pos + byteCount > max_len) return 0;
      memcpy(p_data+pos, data, byteCount);
     
      pos+=byteCount;
      return byteCount;
    }

    int totalBytesWritten() {
      return pos;
    }

   protected:
    uint8_t *p_data;
    size_t max_len;
    size_t pos = 0;
  } print_to_array;
  float byte_factor = 0.0f;
  Stream *p_stream = nullptr;
  Vector<uint8_t> buffer{0};  // we allocate memory only when needed
  T *p_transform = nullptr;
  bool active = false;

  /// Makes sure that the data  is written to the array
  /// @param data
  /// @param byteCount
  /// @return original output of the converter class
  Print *setupOutput(uint8_t *data, size_t byteCount) {
    Print *result = p_transform->getPrint();
    p_transform->setStream(print_to_array);
    print_to_array.begin(data, byteCount);

    return result;
  }
  /// @brief  restores the original output in the converter class
  /// @param out
  void restoreOutput(Print *out) {
    if (out) p_transform->setStream(*out);
  }
};

/**
 * @brief Base class for chained converting streams
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class ReformatBaseStream : public AudioStream {
 public:
  void setupReader(float factor) {
    assert(getStream() != nullptr);
    reader.begin(this, factor, getStream());
  }

  virtual void setStream(Stream &stream) {
    p_stream = &stream;
    p_print = &stream;
  }

  virtual void setStream(Print &print) { p_print = &print; }

  virtual Print *getPrint() { return p_print; }

  virtual Stream *getStream() { return p_stream; }

  size_t readBytes(uint8_t *data, size_t size) override {
    LOGD("ReformatBaseStream::readBytes: %d", size);
    return reader.readBytes(data, size);
  }

  int available() override {
    return DEFAULT_BUFFER_SIZE;  // reader.availableForWrite();
  }

  int availableForWrite() override {
    return DEFAULT_BUFFER_SIZE; //reader.availableForWrite();
  }

 protected:
  TransformationReader<ReformatBaseStream> reader;
  Stream *p_stream = nullptr;
  Print *p_print = nullptr;
  float factor;
};

/**
 * @brief Optional Configuration object. The critical information is the
 * channels and the step_size. All other information is not used.
 *
 */
struct ResampleConfig : public AudioInfo {
  float step_size = 1.0f;
  /// Optional fixed target sample rate
  int to_sample_rate = 0;
};

/**
 * @brief Dynamic Resampling. We can use a variable factor to speed up or slow
 * down the playback.
 * @author Phil Schatzmann
 * @ingroup transform
 * @copyright GPLv3
 * @tparam T
 */
class ResampleStream : public ReformatBaseStream {
 public:
  ResampleStream() = default;

  /// Support for resampling via write.
  ResampleStream(Print &out) { setStream(out); }
  /// Support for resampling via write. The audio information is copied from the
  /// io
  ResampleStream(AudioPrint &out) {
    setAudioInfo(out.audioInfo());
    setStream(out);
  }

  /// Support for resampling via write and read.
  ResampleStream(Stream &io) { setStream(io); }

  /// Support for resampling via write and read. The audio information is copied
  /// from the io
  ResampleStream(AudioStream &io) {
    setAudioInfo(io.audioInfo());
    setStream(io);
  }

  /// Provides the default configuraiton
  ResampleConfig defaultConfig() {
    ResampleConfig cfg;
    cfg.copyFrom(audioInfo());
    return cfg;
  }

  bool begin(ResampleConfig cfg) {
    LOGI("begin step_size: %f", cfg.step_size);
    setAudioInfo(cfg);
    to_sample_rate = cfg.to_sample_rate;

    setupLastSamples(cfg);
    setStepSize(cfg.step_size);
    is_first = true;
    // step_dirty = true;
    bytes_per_frame = info.bits_per_sample / 8 * info.channels;

    // setup reader: e.g. if step size is 2 we need to double the input data
    // reader.begin(this, step_size, p_stream);
    if (p_stream!=nullptr){
      setupReader(step_size);
    }

    return true;
  }

  bool begin(AudioInfo from, int toRate) {
    ResampleConfig rcfg;
    rcfg.to_sample_rate = toRate;
    rcfg.step_size = getStepSize(from.sample_rate, toRate);
    rcfg.copyFrom(from);
    return begin(rcfg);
  }

  bool begin(AudioInfo info, float step) {
    ResampleConfig rcfg;
    rcfg.step_size = step;
    rcfg.copyFrom(info);
    return begin(rcfg);
  }

  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    // update the step size if a fixed to_sample_rate has been defined
    if (to_sample_rate != 0) {
      setStepSize(getStepSize(info.sample_rate, to_sample_rate));
    }
  }

  /// influence the sample rate
  void setStepSize(float step) {
    LOGI("setStepSize: %f", step);
    step_size = step;
  }

  /// calculate the step size the sample rate: e.g. from 44200 to 22100 gives a
  /// step size of 2 in order to provide fewer samples
  float getStepSize(float sampleRateFrom, float sampleRateTo) {
    return sampleRateFrom / sampleRateTo;
  }

  /// Returns the actual step size
  float getStepSize() { return step_size; }

  // int availableForWrite() override { return p_print->availableForWrite(); }

  size_t write(const uint8_t *buffer, size_t bytes) override {
    LOGD("ResampleStream::write: %d", bytes);
    size_t written;
    switch (info.bits_per_sample) {
      case 16:
        return write<int16_t>(p_print, buffer, bytes, written);
      case 24:
        return write<int24_t>(p_print, buffer, bytes, written);
      case 32:
        return write<int32_t>(p_print, buffer, bytes, written);
      default:
        TRACEE();
    }
    return 0;
  }
 protected:
  Vector<uint8_t> last_samples{0};
  float idx = 0;
  bool is_first = true;
  float step_size = 1.0;
  int to_sample_rate = 0;
  int bytes_per_frame = 0;
  TransformationReader<ResampleStream> reader;

  /// Sets up the buffer for the rollover samples
  void setupLastSamples(AudioInfo cfg) {
    int bytes_per_sample = cfg.bits_per_sample / 8;
    int last_samples_size = cfg.channels * bytes_per_sample;
    last_samples.resize(last_samples_size);
    memset(last_samples.data(), 0, last_samples_size);
  }

  /// Writes the buffer to p_print after resampling
  template <typename T>
  size_t write(Print *p_out, const uint8_t *buffer, size_t bytes,
               size_t &written) {
    if (step_size == 1.0) {
      return p_print->write(buffer, bytes);
    }
    // prevent npe
    if (info.channels == 0) {
      LOGE("channels is 0");
      return 0;
    }
    T *data = (T *)buffer;
    int samples = bytes / sizeof(T);
    size_t frames = samples / info.channels;
    written = 0;

    // avoid noise if audio does not start with 0
    if (is_first) {
      is_first = false;
      setupLastSamples<T>(data, 0);
    }

    // process all samples
    while (idx < frames) {
      T frame[info.channels];
      for (int ch = 0; ch < info.channels; ch++) {
        T result = getValue<T>(data, idx, ch);
        if (p_out->availableForWrite() < sizeof(T)) {
          LOGE("Could not write");
        }
        frame[ch] = result;
      }
      written += p_out->write((const uint8_t *)&frame, (size_t)sizeof(frame));
      idx += step_size;
    }

    // save last samples
    setupLastSamples<T>(data, frames - 1);
    idx -= frames;
    // returns requested bytes to avoid rewriting of processed bytes
    return bytes;
  }

  /// get the interpolated value for indicated (float) index value
  template <typename T>
  T getValue(T *data, float frame_idx, int channel) {
    // interpolate value
    int frame_idx1 = frame_idx;
    int frame_idx0 = frame_idx1 - 1;
    T val0 = lookup<T>(data, frame_idx0, channel);
    T val1 = lookup<T>(data, frame_idx1, channel);

    float result = mapFloat(frame_idx, frame_idx1, frame_idx0, val0, val1);
    return round(result);
  }

  // lookup value for indicated frame & channel: index starts with -1;
  template <typename T>
  T lookup(T *data, int frame, int channel) {
    if (frame >= 0) {
      return data[frame * info.channels + channel];
    } else {
      // index -1
      T *pt_last_samples = (T *)last_samples.data();
      return pt_last_samples[channel];
    }
  }
  // store last samples to provide values for index -1
  template <typename T>
  void setupLastSamples(T *data, int frame) {
    for (int ch = 0; ch < info.channels; ch++) {
      T *pt_last_samples = (T *)last_samples.data();
      pt_last_samples[ch] = data[(frame * info.channels) + ch];
    }
  }
};

}  // namespace audio_tools
