#pragma once
#include "AudioBasic/Int24.h"
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/BaseConverter.h"
#include "AudioTools/Buffers.h"

namespace audio_tools {

#if !defined(ARDUINO) || defined(IS_DESKTOP)
#define FLUSH_OVERRIDE override
#else
#define FLUSH_OVERRIDE
#endif

/**
 * @brief Abstract Audio Ouptut class
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioOutput : public Print,
                    public AudioInfoSupport,
                    public AudioInfoSource {
 public:
  virtual ~AudioOutput() = default;

  virtual size_t write(const uint8_t *buffer, size_t size) override = 0;

  virtual size_t write(uint8_t ch) override {
    if (tmp.isFull()) {
      flush();
    }
    return tmp.write(ch);
  }

  virtual int availableForWrite() override { return DEFAULT_BUFFER_SIZE; }

  // removed override because some old implementation did not define this method
  // as virtual
  virtual void flush() FLUSH_OVERRIDE {
    if (tmp.available() > 0) {
      write((const uint8_t *)tmp.address(), tmp.available());
    }
  }

  // overwrite to do something useful
  virtual void setAudioInfo(AudioInfo info) override {
    TRACED();
    cfg = info;
    info.logInfo();
    if (p_notify != nullptr) {
      p_notify->setAudioInfo(info);
    }
  }

  virtual void setNotifyAudioChange(AudioInfoSupport &bi) override {
    p_notify = &bi;
  }

  /// If true we need to release the related memory in the destructor
  virtual bool isDeletable() { return false; }

  virtual AudioInfo audioInfo() override { return cfg; }

  /// Writes n 0 values (= silence)
  /// @param len
  virtual void writeSilence(size_t len) {
    int16_t zero = 0;
    for (int j = 0; j < len / 2; j++) {
      write((uint8_t *)&zero, 2);
    }
  }

  virtual bool begin() { return true; }
  virtual void end() {}

 protected:
  int tmpPos = 0;
  AudioInfoSupport *p_notify = nullptr;
  AudioInfo cfg;
  SingleBuffer<uint8_t> tmp{MAX_SINGLE_CHARS};
};

/**
 * @brief Stream Wrapper which can be used to print the values as readable ASCII
 * to the screen to be analyzed in the Serial Plotter The frames are separated
 * by a new line. The channels in one frame are separated by a ,
 * @ingroup io
 * @tparam T
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename T>
class CsvOutput : public AudioOutput {
 public:
  CsvOutput(int buffer_size = DEFAULT_BUFFER_SIZE, bool active = true) {
    this->active = active;
  }

  /// Constructor
  CsvOutput(Print &out, int channels = 2, int buffer_size = DEFAULT_BUFFER_SIZE,
            bool active = true) {
    this->out_ptr = &out;
    this->active = active;
    cfg.channels = channels;
  }

  /// Starts the processing with 2 channels
  bool begin() {
    TRACED();
    this->active = true;
    return true;
  }

  AudioInfo defaultConfig(RxTxMode mode) { return defaultConfig(); }

  /// Provides the default configuration
  AudioInfo defaultConfig() {
    AudioInfo info;
    info.channels = 2;
    info.sample_rate = 44100;
    info.bits_per_sample = sizeof(T) * 8;
    return info;
  }

  /// Starts the processing with the number of channels defined in AudioInfo
  bool begin(AudioInfo info) {
    TRACED();
    cfg = info;
    this->active = true;
    return cfg.channels != 0;
  }

  /// Starts the processing with the defined number of channels
  void begin(int channels, Print &out = Serial) {
    TRACED();
    this->out_ptr = &out;
    this->active = true;
    cfg.channels = channels;
  }

  /// Sets the CsvOutput as inactive
  void end() {
    TRACED();
    active = false;
  }

  /// defines the number of channels
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    info.logInfo();
    cfg = info;
  };

  /// Writes the data - formatted as CSV -  to the output stream
  virtual size_t write(const uint8_t *data, size_t len) {
    if (!active) return 0;
    LOGD("CsvOutput::write: %d", (int)len);
    if (cfg.channels == 0) {
      LOGW("Channels not defined: using 2");
      cfg.channels = 2;
    }
    size_t lenChannels = len / (sizeof(T) * cfg.channels);
    if (lenChannels > 0) {
      writeFrames((T *)data, lenChannels);
    } else if (len == sizeof(T)) {
      // if the write contains less then a frame we buffer the data
      T *data_value = (T *)data;
      out_ptr->print(data_value[0]);
      channel++;
      if (channel == cfg.channels) {
        out_ptr->println();
        channel = 0;
      } else {
        out_ptr->print(", ");
      }
    } else {
      LOGE("Unsuppoted size: %d for channels %d and bits: %d", (int)len,
           cfg.channels, cfg.bits_per_sample);
    }
    return len;
  }

  int availableForWrite() { return 1024; }

 protected:
  T *data_ptr;
  Print *out_ptr = &Serial;
  int channel = 0;
  bool active = false;

  void writeFrames(T *data_ptr, int frameCount) {
    for (size_t j = 0; j < frameCount; j++) {
      for (int ch = 0; ch < cfg.channels; ch++) {
        if (out_ptr != nullptr && data_ptr != nullptr) {
          int value = *data_ptr;
          out_ptr->print(value);
        }
        data_ptr++;
        if (ch < cfg.channels - 1) Serial.print(", ");
      }
      Serial.println();
    }
  }
};

// legacy name
template <typename T>
using CsvStream = CsvOutput<T>;

/**
 * @brief Creates a Hex Dump
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class HexDumpOutput : public AudioOutput {
 public:
  HexDumpOutput(int buffer_size = DEFAULT_BUFFER_SIZE, bool active = true) {
    this->active = active;
  }

  /// Constructor
  HexDumpOutput(Print &out, int buffer_size = DEFAULT_BUFFER_SIZE,
                bool active = true) {
    this->out_ptr = &out;
    this->active = active;
  }

  void begin(AudioInfo info) {
    TRACED();
    info.logInfo();
    this->active = true;
    pos = 0;
  }

  bool begin() {
    TRACED();
    this->active = true;
    pos = 0;
    return active;
  }

  /// Sets the CsvOutput as inactive
  void end() {
    TRACED();
    active = false;
  }

  void flush() {
    Serial.println();
    pos = 0;
  }

  virtual size_t write(const uint8_t *data, size_t len) {
    if (!active) return 0;
    TRACED();
    for (size_t j = 0; j < len; j++) {
      out_ptr->print(data[j], HEX);
      out_ptr->print(" ");
      pos++;
      if (pos == 8) {
        Serial.print(" - ");
      }
      if (pos == 16) {
        Serial.println();
        pos = 0;
      }
    }
    return len;
  }

  //
  AudioInfo defaultConfig(RxTxMode mode = TX_MODE) {
    AudioInfo info;
    return info;
  }

 protected:
  Print *out_ptr = &Serial;
  int pos = 0;
  bool active = false;
};

// legacy name
using HexDumpStream = HexDumpOutput;

/**
 * @brief Mixing of multiple outputs to one final output
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <typename T>
class OutputMixer : public Print {
 public:
  OutputMixer(Print &finalOutput, int outputStreamCount) {
    p_final_output = &finalOutput;
    setOutputCount(outputStreamCount);
  };

  void setOutputCount(int count) {
    output_count = count;
    buffers.resize(count);
    for (int i = 0; i < count; i++) {
      buffers[i] = nullptr;
    }
    weights.resize(count);
    for (int i = 0; i < count; i++) {
      weights[i] = 1.0;
    }

    update_total_weights();
  }

  /// Defines a new weight for the indicated channel: If you set it to 0 it is
  /// muted.
  void setWeight(int channel, float weight) {
    if (channel < size()) {
      weights[channel] = weight;
    } else {
      LOGE("Invalid channel %d - max is %d", channel, size() - 1);
    }
    update_total_weights();
  }

  bool begin(int copy_buffer_size_bytes = DEFAULT_BUFFER_SIZE,
             MemoryType memoryType = PS_RAM) {
    is_active = true;
    size_bytes = copy_buffer_size_bytes;
    stream_idx = 0;
    memory_type = memoryType;
    allocate_buffers();
    return true;
  }

  /// Remove all input streams
  void end() {
    total_weights = 0.0;
    is_active = false;
    // release memory for buffers
    free_buffers();
  }

  /// Number of stremams to which are mixed together
  int size() { return output_count; }

  size_t write(uint8_t) override { return 0; }

  /// Write the data from a simgle stream which will be mixed together (the
  /// stream idx is increased)
  size_t write(const uint8_t *buffer_c, size_t bytes) override {
    size_t result = write(stream_idx, buffer_c, bytes);
    // after writing the last stream we flush
    stream_idx++;
    if (stream_idx >= output_count) {
      flushMixer();
    }
    return result;
  }

  /// Write the data for an individual stream idx which will be mixed together
  size_t write(int idx, const uint8_t *buffer_c, size_t bytes) {
    LOGD("write idx %d: %d", stream_idx, bytes);
    size_t result = 0;
    RingBuffer<T> *p_buffer = idx < output_count ? buffers[idx] : nullptr;
    assert(p_buffer != nullptr);
    size_t samples = bytes / sizeof(T);
    if (p_buffer->availableForWrite() < samples) {
      LOGW(
          "Available Buffer too small %d: requested: %d -> increase the "
          "buffer size",
          p_buffer->availableForWrite(), samples);
    } else {
      result = p_buffer->writeArray((T *)buffer_c, samples) * sizeof(T);
    }
    return result;
  }

  /// Provides the bytes available to write for the current stream buffer
  int availableForWrite() override {
    return is_active ? availableForWrite(stream_idx) : 0;
  }

  /// Provides the bytes available to write for the indicated stream index
  int availableForWrite(int idx) {
    RingBuffer<T> *p_buffer = buffers[idx];
    if (p_buffer == nullptr) return 0;
    return p_buffer->availableForWrite();
  }

  /// Force output to final destination
  void flushMixer() {
    LOGD("flush");
    bool result = false;

    // determine ringbuffer with mininum available data
    size_t samples = size_bytes / sizeof(T);
    for (int j = 0; j < output_count; j++) {
      samples = MIN(samples, (size_t)buffers[j]->available());
    }

    if (samples > 0) {
      result = true;
      // mix data from ringbuffers to output
      output.resize(samples);
      memset(output.data(), 0, samples * sizeof(T));
      for (int j = 0; j < output_count; j++) {
        float weight = weights[j];
        // sum up input samples to result samples
        for (int i = 0; i < samples; i++) {
          output[i] += weight * buffers[j]->read() / total_weights;
        }
      }

      // write output
      LOGD("write to final out: %d", samples * sizeof(T));
      p_final_output->write((uint8_t *)output.data(), samples * sizeof(T));
    }
    stream_idx = 0;
    return;
  }

 protected:
  Vector<RingBuffer<T> *> buffers{0};
  Vector<T> output{0};
  Vector<float> weights{0};
  Print *p_final_output = nullptr;
  float total_weights = 0.0;
  bool is_active = false;
  int stream_idx = 0;
  int size_bytes;
  int output_count;
  MemoryType memory_type;
  void *p_memory = nullptr;

  void update_total_weights() {
    total_weights = 0.0;
    for (int j = 0; j < weights.size(); j++) {
      total_weights += weights[j];
    }
  }

  void allocate_buffers() {
    // allocate ringbuffers for each output
    for (int j = 0; j < output_count; j++) {
      if (buffers[j] != nullptr) {
        delete buffers[j];
      }
#if defined(ESP32) && defined(ARDUINO)
      if (memory_type == PS_RAM && ESP.getFreePsram() >= size_bytes) {
        p_memory = ps_malloc(size_bytes);
        LOGI("Buffer %d allocated %d bytes in PS_RAM", j, size_bytes);
      } else {
        p_memory = malloc(size_bytes);
        LOGI("Buffer %d allocated %d bytes in RAM", j, size_bytes);
      }
      if (p_memory != nullptr) {
        buffers[j] = new (p_memory) RingBuffer<T>(size_bytes / sizeof(T));
      } else {
        LOGE("Not enough memory to allocate %d bytes", size_bytes);
      }
#else
      buffers[j] = new RingBuffer<T>(size_bytes / sizeof(T));
#endif
    }
  }

  void free_buffers() {
    // allocate ringbuffers for each output
    for (int j = 0; j < output_count; j++) {
      if (buffers[j] != nullptr) {
        delete buffers[j];
#ifdef ESP32
        if (p_memory != nullptr) {
          free(p_memory);
        }
#endif
        buffers[j] = nullptr;
      }
    }
  }
};

/**
 * @brief A simple class to determine the volume
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VolumeOutput : public AudioOutput {
 public:
  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return true;
  }

  void setAudioInfo(AudioInfo info) {
    this->info = info;
    if (info.channels > 0) {
      volumes.resize(info.channels);
      volumes_tmp.resize(info.channels);
    }
  }

  size_t write(const uint8_t *buffer, size_t size) {
    clear();
    switch (info.bits_per_sample) {
      case 16:
        updateVolumes<int16_t>(buffer, size);
        break;
      case 24:
        updateVolumes<int24_t>(buffer, size);
        break;
      case 32:
        updateVolumes<int32_t>(buffer, size);
        break;
      default:
        LOGE("Unsupported bits_per_sample: %d", info.bits_per_sample);
        break;
    }
    return size;
  }

  /// Determines the volume (max amplitude). The range depends on the
  /// bits_per_sample.
  float volume() { return f_volume; }

  /// Determines the volume for the indicated channel. You must call the begin
  /// method to define the number of channels
  float volume(int channel) {
    if (volumes.size() == 0) {
      LOGE("begin not called!");
      return 0.0f;
    }
    if (channel >= volumes.size()) {
      LOGE("invalid channel %d", channel);
      return 0.0f;
    }
    return volumes[channel];
  }

  /// Resets the actual volume
  void clear() {
    f_volume_tmp = 0;
    for (int j = 0; j < info.channels; j++) {
      volumes_tmp[j] = 0;
    }
  }

 protected:
  AudioInfo info;
  float f_volume_tmp = 0;
  float f_volume = 0;
  Vector<float> volumes{0};
  Vector<float> volumes_tmp{0};

  template <typename T>
  void updateVolumes(const uint8_t *buffer, size_t size) {
    T *bufferT = (T *)buffer;
    int samplesCount = size / sizeof(T);
    for (int j = 0; j < samplesCount; j++) {
      float tmp = abs(static_cast<float>(bufferT[j]));
      updateVolume(tmp, j);
    }
    commit();
  }

  void updateVolume(float tmp, int j) {
    if (tmp > f_volume) {
      f_volume_tmp = tmp;
    }
    if (volumes_tmp.size() > 0 && info.channels > 0) {
      volumes_tmp[j % info.channels] = tmp;
    }
  }

  void commit() {
    f_volume = f_volume_tmp;
    for (int j = 0; j < info.channels; j++) {
      volumes[j] = volumes_tmp[j];
    }
  }
};

// legacy name
using VolumePrint = VolumeOutput;

/**
 * @brief Writes to a preallocated memory
 * @ingroup io
 */
class MemoryOutput : public AudioOutput {
 public:
  MemoryOutput(uint8_t *start, int len) {
    p_start = start;
    p_next = start;
    max_size = len;
    if (p_next == nullptr) {
      LOGE("start must not be null");
    }
  }

  size_t write(const uint8_t *buffer, size_t len) override {
    if (p_next == nullptr) return 0;
    if (pos + len <= max_size) {
      memcpy(p_next, buffer, len);
      pos += len;
      p_next += len;
      return len;
    } else {
      LOGE("Buffer too small: pos:%d, size: %d ", pos, (int)max_size);
      return 0;
    }
  }

  int availableForWrite() override { return max_size - pos; }

  int size() { return max_size; }

 protected:
  int pos = 0;
  uint8_t *p_start = nullptr;
  uint8_t *p_next = nullptr;
  size_t max_size;
};

// legacy name
using MemoryPrint = MemoryOutput;

/**
 * @brief Switched Output class. Class which can be used to filter the output
 * out based on a switch: If the switch is on the output is forwarded. If it is
 * off it is just ignored.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OnOffOutput : public AudioOutput {
 public:
  OnOffOutput() = default;
  OnOffOutput(Print &out) { setOutput(out); }

  /// Same as setActive(true)
  bool begin() override {
    setActive(true);
    return true;
  }
  /// Same as setActive(false)
  void end() override { setActive(false); }

  /// Redefines the final output
  void setOutput(Print &out) { p_output = &out; }

  /// set the sitch to on or off
  void setActive(bool on) { is_active = on; }

  /// Determines if the switch is on
  bool isActive() { return is_active; }

  size_t write(const uint8_t *buffer, size_t len) override {
    if (p_output == nullptr) return 0;
    size_t result = len;
    if (is_active) {
      len = p_output->write(buffer, len);
    }
    return len;
  }

 protected:
  Print *p_output = nullptr;
  bool is_active = true;
};

}  // namespace audio_tools