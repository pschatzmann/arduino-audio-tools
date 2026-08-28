#pragma once

#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Class which measures the thruput
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
 */
class MeasuringStream : public ModifyingStream {
 public:
  MeasuringStream(int count = 10, Print *logOut = nullptr) {
    this->count = count;
    this->max_count = count;
    p_stream = &null;
    p_print = &null;
    start_time = millis();
    p_logout = logOut;
  }

  MeasuringStream(Print &print, int count = 10, Print *logOut = nullptr) {
    this->count = count;
    this->max_count = count;
    setOutput(print);
    start_time = millis();
    p_logout = logOut;
  }

  MeasuringStream(Stream &stream, int count = 10, Print *logOut = nullptr) {
    this->count = count;
    this->max_count = count;
    setStream(stream);
    start_time = millis();
    p_logout = logOut;
  }

  /// Defines the logging output
  void setLogOutput(Print &out) { p_logout = &out; }

  /// Defines/Changes the input & output
  void setStream(Stream &io) override {
    p_print = &io;
    p_stream = &io;
  };

  /// Defines/Changes the output target
  void setOutput(Print &out) override { p_print = &out; }

  /// Provides the data from all streams mixed together
  size_t readBytes(uint8_t *data, size_t len) override {
    total_bytes_since_begin += len;
    return measure(p_stream->readBytes(data, len));
  }

  int available() override { return p_stream->available(); }

  /// Writes raw PCM audio data, which will be the input for the volume control
  virtual size_t write(const uint8_t *data, size_t len) override {
    total_bytes_since_begin += len;
    return measure(p_print->write(data, len));
  }

  /// Provides the nubmer of bytes we can write
  virtual int availableForWrite() override {
    return p_print->availableForWrite();
  }

  /// Returns the actual thrughput in bytes per second
  int bytesPerSecond() { return bytes_per_second; }

  /// Returns the actual thrughput in frames (samples) per second
  int framesPerSecond() {
    if (frame_size == 0) return 0;
    return bytes_per_second / frame_size;
  }

  /// Provides the time when the last measurement was started
  uint32_t startTime() { return start_time; }

  void setAudioInfo(AudioInfo info) override {
    LOGI("MeasuringStream::setAudioInfo: %d bits, %d channels", info.bits_per_sample, info.channels);
    ModifyingStream::setAudioInfo(info);
    setFrameSize(info.bits_per_sample / 8 * info.channels);
  }

  bool begin() override {
    total_bytes_since_begin = 0;
    ms_at_begin = millis();
    return ModifyingStream::begin();
  }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  /// Trigger reporting in frames (=samples) per second
  void setFrameSize(int size) { frame_size = size; }

  /// Report in bytes instead of samples
  void setReportBytes(bool flag) { report_bytes = flag; }

  void setName(const char *name) { this->name = name; }

  /// Provides the time in ms since the last call of begin()
  uint32_t timeSinceBegin() { return millis() - ms_at_begin; }

  /// Provides the total processed bytes since the last call of begin()
  uint32_t bytesSinceBegin() { return total_bytes_since_begin; }

  /// Provides the estimated runtime in milliseconds for the indicated total
  uint32_t estimatedTotalTimeFor(uint32_t totalBytes) {
    if (bytesSinceBegin() == 0) return 0;
    return static_cast<float>(timeSinceBegin()) / bytesSinceBegin() *
           totalBytes;
  }

  /// Provides the estimated time from now to the end in ms
  uint32_t estimatedOpenTimeFor(uint32_t totalBytes) {
    if (bytesSinceBegin() == 0) return 0;
    return estimatedTotalTimeFor(totalBytes) - timeSinceBegin();
  }

  /// Alternative update method: e.g report actual file positon: returns true if
  /// the file position was increased
  bool setProcessedBytes(uint32_t pos) {
    bool is_regular_update = true;
    if (pos < total_bytes_since_begin) {
      begin();
      is_regular_update = false;
    }
    total_bytes_since_begin = pos;
    return is_regular_update;
  }

 protected:
  int max_count = 0;
  int count = 0;
  Stream *p_stream = nullptr;
  Print *p_print = nullptr;
  uint32_t start_time;
  int total_bytes = 0;
  int bytes_per_second = 0;
  int frame_size = 0;
  NullStream null;
  Print *p_logout = nullptr;
  bool report_bytes = false;
  const char *name = "";
  uint32_t ms_at_begin = 0;
  uint32_t total_bytes_since_begin = 0;

  size_t measure(size_t len) {
    count--;
    total_bytes += len;

    if (count <= 0) {
      uint32_t end_time = millis();
      int time_diff = end_time - start_time;  // in ms
      if (time_diff > 0) {
        bytes_per_second = total_bytes / time_diff * 1000;
        printResult();
        count = max_count;
        total_bytes = 0;
        start_time = end_time;
      }
    }
    return len;
  }

  void printResult() {
    char msg[70];
    if (report_bytes || frame_size == 0) {
      snprintf(msg, 70, "%s ==> Bytes per second: %d", name, bytes_per_second);
    } else {
      snprintf(msg, 70, "%s ==> Samples per second: %d", name,
               bytes_per_second / frame_size);
    }
    if (p_logout != nullptr) {
      p_logout->println(msg);
    } else {
      LOGI("%s", msg);
    }
  }
};

}  // namespace audio_tools
