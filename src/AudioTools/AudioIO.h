#pragma once
#include "AudioTools/AudioOutput.h"
#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Base class for Output Adpapters
 *
 */
class AudioOutputAdapter : public AudioOutput {};

/**
 * @brief Wrapper which converts a Print to a AudioOutput
 * @ingroup tools
 */
class AdapterPrintToAudioOutput : public AudioOutputAdapter {
public:
  AdapterPrintToAudioOutput(Print &print) { p_print = &print; }
  void setAudioInfo(AudioInfo info) {}
  size_t write(const uint8_t *buffer, size_t size) {
    return p_print->write(buffer, size);
  }
  /// If true we need to release the related memory in the destructor
  virtual bool isDeletable() { return true; }

protected:
  Print *p_print = nullptr;
};

/**
 * @brief Wrapper which converts a AudioStream to a AudioOutput
 * @ingroup tools
 */
class AdapterAudioStreamToAudioOutput : public AudioOutputAdapter {
public:
  AdapterAudioStreamToAudioOutput() = default;

  AdapterAudioStreamToAudioOutput(AudioStream &stream) { setStream(stream); }

  void setStream(AudioStream &stream) { p_stream = &stream; }

  void setAudioInfo(AudioInfo info) { p_stream->setAudioInfo(info); }
  size_t write(const uint8_t *buffer, size_t size) {
    return p_stream->write(buffer, size);
  }

  /// If true we need to release the related memory in the destructor
  virtual bool isDeletable() { return true; }

protected:
  AudioStream *p_stream = nullptr;
};

/**
 * @brief Wrapper which converts a AudioStream to a AudioOutput
 * @ingroup tools
 */
class AdapterAudioOutputToAudioStream : public AudioStream {
public:
  AdapterAudioOutputToAudioStream() = default;

  AdapterAudioOutputToAudioStream(AudioOutput &stream) { setOutput(stream); }

  void setOutput(AudioOutput &stream) { p_stream = &stream; }

  void setAudioInfo(AudioInfo info) { p_stream->setAudioInfo(info); }
  size_t write(const uint8_t *buffer, size_t size) {
    return p_stream->write(buffer, size);
  }

  /// If true we need to release the related memory in the destructor
  virtual bool isDeletable() { return true; }

protected:
  AudioOutput *p_stream = nullptr;
};

/**
 * @brief Replicates the output to multiple destinations.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiOutput : public AudioOutput {
public:
  /// Defines a MultiOutput with no final output: Define your outputs with add()
  MultiOutput() = default;

  /// Defines a MultiOutput with a single final outputs,
  MultiOutput(AudioOutput &out) { add(out); }

  MultiOutput(AudioStream &out) { add(out); }

  /// Defines a MultiOutput with 2 final outputs
  MultiOutput(AudioOutput &out1, AudioOutput &out2) {
    add(out1);
    add(out2);
  }

  /// Defines a MultiOutput with 2 final outputs
  MultiOutput(AudioStream &out1, AudioStream &out2) {
    add(out1);
    add(out2);
  }

  virtual ~MultiOutput() {
    for (int j = 0; j < vector.size(); j++) {
      if (vector[j]->isDeletable()) {
        delete vector[j];
      }
    }
  }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return true;
  }

  /// Add an additional AudioOutput output
  void add(AudioOutput &out) { vector.push_back(&out); }

  /// Add an AudioStream to the output
  void add(AudioStream &stream) {
    AdapterAudioStreamToAudioOutput *out =
        new AdapterAudioStreamToAudioOutput(stream);
    vector.push_back(out);
  }

  void add(Print &print) {
    AdapterPrintToAudioOutput *out = new AdapterPrintToAudioOutput(print);
    vector.push_back(out);
  }

  void flush() {
    for (int j = 0; j < vector.size(); j++) {
      vector[j]->flush();
    }
  }

  void setAudioInfo(AudioInfo info) {
    for (int j = 0; j < vector.size(); j++) {
      vector[j]->setAudioInfo(info);
    }
  }

  size_t write(const uint8_t *buffer, size_t size) {
    for (int j = 0; j < vector.size(); j++) {
      int open = size;
      int start = 0;
      while (open > 0) {
        int written = vector[j]->write(buffer + start, open);
        open -= written;
        start += written;
      }
    }
    return size;
  }

  size_t write(uint8_t ch) {
    for (int j = 0; j < vector.size(); j++) {
      int open = 1;
      while (open > 0) {
        open -= vector[j]->write(ch);
      }
    }
    return 1;
  }

protected:
  Vector<AudioOutput *> vector;
};

/**
 * @brief AudioStream class that can define a start and (an optional) stop time
 * Usually it is used to wrap an Audio Sink (e.g. I2SStream), but wrapping an
 * Audio Source is supported as well. Only wrap classes which represent PCM
 * data!
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TimedStream : public AudioStream {
public:
  TimedStream(AudioStream &io, long startSeconds = 0, long endSeconds = -1) {
    p_stream = &io;
    p_print = &io;
    p_info = &io;
    setStartSecond(startSeconds);
    setEndSecond(endSeconds);
  }

  TimedStream(AudioOutput &o, long startSeconds = 0, long endSeconds = -1) {
    p_print = &o;
    p_info = &o;
    setStartSecond(startSeconds);
    setEndSecond(endSeconds);
  }

  /// Defines the start time in seconds. The audio before the start time will be
  /// skipped
  void setStartSecond(long startSeconds) { start_seconds = startSeconds; }

  /// Defines (an optional) the end time in seconds. After the end time no audio
  /// is played and available() will return 0
  void setEndSecond(long endSeconds) { end_seconds = endSeconds; }

  /// Returns true if we are in a valid time range and are still playing sound
  bool isPlaying() {
    if (current_bytes < start_bytes)
      return false;
    if (end_bytes > 0 && current_bytes > end_bytes)
      return false;
    return true;
  }

  /// Returns true if we are not past the end time;
  bool isActive() {
    return (current_bytes < end_bytes && current_bytes > start_bytes);
  }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  bool begin() override {
    calculateByteLimits();
    current_bytes = 0;
    return true;
  }

  operator bool() { return isActive(); }

  /// Provides only data for the indicated start and end time. Only supported
  /// for data which does not contain any heder information: so PCM, mp3 should
  /// work!
  size_t readBytes(uint8_t *buffer, size_t length) override {
    // if reading is not supported we stop
    if (p_stream == nullptr)
      return 0;
    // if we are past the end we stop
    if (!isActive())
      return 0;
    // read the data now
    size_t result = 0;
    do {
      result = p_stream->readBytes(buffer, length);
      current_bytes += length;
      // ignore data before start time
    } while (result > 0 && current_bytes < start_bytes);
    return isPlaying() ? result : 0;
  }

  /// Plays only data for the indiated start and end time
  size_t write(const uint8_t *buffer, size_t length) override {
    current_bytes += length;
    return isPlaying() ? p_print->write(buffer, length) : length;
  }

  /// Provides the available bytes until the end time has reached
  int available() override {
    if (p_stream == nullptr)
      return 0;
    return isActive() ? p_stream->available() : 0;
  }

  /// Updates the AudioInfo in the current object and in the source or target
  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    p_info->setAudioInfo(info);
    calculateByteLimits();
  }

  int availableForWrite() override { return p_print->availableForWrite(); }

  /// Experimental: if used on mp3 you can set the compression ratio e.g. to 11
  /// which will be used to approximate the time
  void setCompressionRatio(float ratio) { compression_ratio = ratio; }

  /// Calculates the bytes per second from the AudioInfo
  int bytesPerSecond() {
    return info.sample_rate * info.channels * info.bits_per_sample / 8;
  }

protected:
  Stream *p_stream = nullptr;
  Print *p_print = nullptr;
  AudioInfoSupport *p_info = nullptr;
  uint32_t start_seconds = 0;
  uint32_t end_seconds = UINT32_MAX;
  uint32_t start_bytes = 0;
  uint32_t end_bytes = UINT32_MAX;
  uint32_t current_bytes = 0;
  float compression_ratio = 1.0;

  void calculateByteLimits() {
    int bytes_per_second = bytesPerSecond();
    if (bytes_per_second > 0) {
      start_bytes = bytes_per_second * start_seconds / compression_ratio;
      end_bytes = bytes_per_second * end_seconds / compression_ratio;
    } else {
      LOGE("AudioInfo not defined");
    }
  }
};

/**
 * Measures the effective output sample rate that flows thru the input or
 * output chain: We specify the n.th io time at which we calculate the rate with
 * the help of the setReportAt() method.
 *
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 */
class RateMeasuringStream : public AudioStream {
public:
  RateMeasuringStream() = default;
  RateMeasuringStream(Print &out) { setOutput(out); }
  RateMeasuringStream(Stream &out) { setStream(out); }

  /// Defines the output
  void setOutput(Print &out) { setStream(out); }

  /// Defines the output
  void setStream(Print &out) { p_out = &out; }
  void setStream(Stream &stream) {
    p_out = &stream;
    p_stream = &stream;
  }

  /// We need to warm up to find a stable value: here we define the count at
  /// which we measure
  void setReportAt(int at) { count_at = at; }

  /// Defines an alternative method to determine millis()
  void setTimeCallback(uint32_t (*cb_ms)(void)) { millis_cb = cb_ms; }

  int availableForWrite() override {
    if (p_out == nullptr)
      return 0;
    return p_out->availableForWrite();
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (p_out == nullptr)
      return 0;
    if (counter == 0 && len > 0) {
      start_time = ms();
    }
    size_t result = p_out->write(data, len);
    if (isActvie()) {
      last_time = ms();
      total_bytes += result;
    }
    counter++;
    return result;
  }

  int available() override {
    if (p_stream == nullptr)
      return 0;
    return p_stream->available();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    if (p_stream == nullptr)
      return 0;
    if (counter == 0 && len > 0) {
      start_time = ms();
    }
    size_t result = p_stream->readBytes(data, len);
    if (isActvie()) {
      last_time = ms();
      total_bytes += result;
    }
    counter++;
    return result;
  }

  /// provides the effective sample rate in server time
  float sampleRate() {
    // if we did not reach the limit we report the to be rate
    if (counter < count_at)
      return info.sample_rate;
    // report the effective rate
    float time_ms = last_time - start_time;
    float bytes_per_second = static_cast<float>(total_bytes) / time_ms / 1000.0;
    float frame_size = info.bits_per_sample * info.channels / 8;
    // calculate frames (=samples) per second
    return bytes_per_second / frame_size;
  }

  /// Calculates the correction factor to adjust the sample rate
  float correctionFactor() {
    // e.g. to be 44100 , eff 44110 -> 0.99977329403 (we need to slow down a
    // bit)
    return static_cast<float>(info.sample_rate) / sampleRate();
  }

  /// Provides true if we can provide a measured value
  bool isResultValid() { return counter >= count_at; }

  /// Provides true if we reached the update count
  bool isUpdate() { return counter == count_at; }

  void logResult() {
    LOGI("Sample rate: %d, effective: %f -> correction %f", info.sample_rate, sampleRate(), correctionFactor());
  }

  bool isActvie() { return  counter <= count_at; }

protected:
  const char *TAG = "SNAP";
  Stream *p_stream = nullptr;
  Print *p_out = nullptr;
  uint32_t counter = 0;
  uint32_t count_at = -1;
  uint32_t start_time = 0;
  uint32_t last_time = 0;
  uint32_t total_bytes = 0;
  uint32_t (*millis_cb)(void) = nullptr;

  inline uint32_t ms() {
    if (millis_cb)
      return millis_cb();
    return millis();
  }
};

} // namespace audio_tools
