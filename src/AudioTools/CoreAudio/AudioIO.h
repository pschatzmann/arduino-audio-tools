#pragma once
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

#ifndef MAX_ZERO_READ_COUNT
#  define MAX_ZERO_READ_COUNT 3
#endif

#ifndef CHANNEL_SELECT_BUFFER_SIZE
#  define CHANNEL_SELECT_BUFFER_SIZE 256
#endif


namespace audio_tools {
/**
 * @brief ConverterStream Helper class which implements the
 *  readBytes with the help of write.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T class name of the original transformer stream
 */
template <class T>
class TransformationReader {
 public:
  /// @brief setup of the TransformationReader class
  /// @param transform The original transformer stream
  /// @param source The data source of the data to be converted
  /// the requested converted bytes
  void begin(T *transform, Stream *source) {
    TRACED();
    active = true;
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

  /// Defines the read buffer size for individual reads
  void resizeReadBuffer(int size) {
    buffer.resize(size);
  }
  /// Defines the queue size for result
  void resizeResultQueue(int size) {
    result_queue_buffer.resize(size);
    result_queue.begin();
  }


  size_t readBytes(uint8_t *data, size_t len) {
    LOGD("TransformationReader::readBytes: %d", (int)len);
    if (!active) {
      LOGE("inactive");
      return 0;
    }
    if (p_stream == nullptr) {
      LOGE("p_stream is NULL");
      return 0;
    }

    // we read half the necessary bytes
    if (buffer.size() == 0) {
      int size = (0.5f / p_transform->getByteFactor() * len);
      // process full samples/frames
      size = size / 4 * 4;
      LOGI("read size: %d", size);
      buffer.resize(size);
    }

    if (result_queue_buffer.size() == 0) {
      // make sure that the ring buffer is big enough
      int rb_size = len * result_queue_factor;
      LOGI("buffer size: %d", rb_size);
      result_queue_buffer.resize(rb_size);
      result_queue.begin();
    }

    if (result_queue.available() < len) {
      Print *tmp = setupOutput();
      int zero_count = 0;
      while (result_queue.available() < len) {
        int read_eff = p_stream->readBytes(buffer.data(), buffer.size());
        if (read_eff > 0) {
          zero_count  = 0; // reset 0 count
          if (read_eff != buffer.size()){
            LOGD("readBytes %d -> %d", buffer.size(), read_eff);
          }
          int write_eff = p_transform->write(buffer.data(), read_eff);
          if (write_eff != read_eff){
            LOGE("write %d -> %d", read_eff, write_eff);
          }
        } else {
          // limit the number of reads which provide 0;
          if (++zero_count > MAX_ZERO_READ_COUNT){
            break;
          }
          // wait for some more data
          delay(5);
        }
      }
      restoreOutput(tmp);
    }

    int result_len = min((int)len, result_queue.available());
    result_len = result_queue.readBytes(data, result_len);
    LOGD("TransformationReader::readBytes: %d -> %d", (int)len,
         result_len);

    return result_len;
  }

  void end() {
    result_queue_buffer.resize(0);
    buffer.resize(0);
  }

  /// Defines the queue size dependent on the read size
  void setResultQueueFactor(int factor) { result_queue_factor = factor; }

 protected:
  RingBuffer<uint8_t> result_queue_buffer{0};
  QueueStream<uint8_t> result_queue{result_queue_buffer};  //
  Stream *p_stream = nullptr;
  Vector<uint8_t> buffer{0};  // we allocate memory only when needed
  T *p_transform = nullptr;
  bool active = false;
  int result_queue_factor = 5;

  /// Makes sure that the data  is written to the array
  /// @param data
  /// @return original output of the converter class
  Print *setupOutput() {
    Print *result = p_transform->getPrint();
    p_transform->setOutput((Print &)result_queue);

    return result;
  }
  /// @brief  restores the original output in the converter class
  /// @param out
  void restoreOutput(Print *out) {
    if (out) p_transform->setOutput(*out);
  }
};

/**
 * @brief Base class for chained converting streams
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class ReformatBaseStream : public ModifyingStream {
 public:
  virtual void setStream(Stream &stream) override {
    TRACED();
    p_stream = &stream;
    p_print = &stream;
  }

  virtual void setStream(AudioStream &stream) {
    TRACED();
    p_stream = &stream;
    p_print = &stream;
    //setNotifyOnOutput(stream);
    addNotifyAudioChange(stream);
  }

  virtual void setOutput(AudioOutput &print) {
    TRACED();
    p_print = &print;
    addNotifyAudioChange(print);
  }

  virtual void setOutput(Print &print) override {
    TRACED();
    p_print = &print;
  }

  virtual Print *getPrint() { return p_print; }

  virtual Stream *getStream() { return p_stream; }

  size_t readBytes(uint8_t *data, size_t len) override {
    LOGD("ReformatBaseStream::readBytes: %d", (int)len);
    return reader.readBytes(data, len);
  }

  int available() override {
    return DEFAULT_BUFFER_SIZE;  // reader.availableForWrite();
  }

  int availableForWrite() override {
    return DEFAULT_BUFFER_SIZE;  // reader.availableForWrite();
  }

  virtual float getByteFactor() = 0;

  void end() override {
    TRACED();
    AudioStream::end();
    reader.end();
  }

  /// Provides access to the TransformationReader
  virtual TransformationReader<ReformatBaseStream> &transformationReader() {return reader;}

 protected:
  TransformationReader<ReformatBaseStream> reader;
  Stream *p_stream = nullptr;
  Print *p_print = nullptr;

  void setupReader() {
    if (getStream() != nullptr) {
      reader.begin(this, getStream());
    }
  }
};

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
  AdapterPrintToAudioOutput() = default;
  AdapterPrintToAudioOutput(Print &print) { setStream(print); }
  void setStream(Print& out) { p_print = &out; }
  void setAudioInfo(AudioInfo info) { cfg = info;}
  size_t write(const uint8_t *data, size_t len) {
    return p_print->write(data, len);
  }
  /// If true we need to release the related memory in the destructor
  virtual bool isDeletable() { return true; }

  AudioInfo audioInfo() {return cfg; }

 protected:
  Print *p_print = nullptr;
  AudioInfo cfg;
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

  void setAudioInfo(AudioInfo info) override { p_stream->setAudioInfo(info); }

  AudioInfo audioInfo() override { return p_stream->audioInfo(); }

  size_t write(const uint8_t *data, size_t len) override {
    return p_stream->write(data, len);
  }

  int availableForWrite() override { return p_stream->availableForWrite(); }

  bool begin() override { return p_stream->begin(); }

  void end() override { p_stream->end(); }

  /// If true we need to release the related memory in the destructor
  virtual bool isDeletable() { return true; }

  operator bool() override { return *p_stream; }

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

  void setAudioInfo(AudioInfo info) override { p_stream->setAudioInfo(info); }

  AudioInfo audioInfo() override { return p_stream->audioInfo(); }

  size_t write(const uint8_t *data, size_t len) override {
    return p_stream->write(data, len);
  }

  bool begin() override { return p_stream->begin(); }

  void end() override { p_stream->end(); }

  /// If true we need to release the related memory in the destructor
  virtual bool isDeletable() { return true; }

  operator bool() override { return *p_stream; }

 protected:
  AudioOutput *p_stream = nullptr;
};

/**
 * @brief Replicates the output to multiple destinations.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiOutput : public ModifyingOutput {
 public:
  /// Defines a MultiOutput with no final output: Define your outputs with add()
  MultiOutput() = default;

  MultiOutput(Print &out) { add(out); }

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

  /// Defines a MultiOutput with 2 final outputs: Warning no support for
  /// AudioInfo notifications. It is recommended to use individual add calls.
  MultiOutput(Print &out1, Print &out2) {
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

  size_t write(const uint8_t *data, size_t len) {
    for (int j = 0; j < vector.size(); j++) {
      int open = len;
      int start = 0;
      while (open > 0) {
        int written = vector[j]->write(data + start, open);
        open -= written;
        start += written;
      }
    }
    return len;
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
  /// support for Pipleline
  void setOutput(Print &out) { add(out); }
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
class TimedStream : public ModifyingStream {
 public:
  TimedStream() = default;

  TimedStream(AudioStream &io, long startSeconds = 0, long endSeconds = -1) {
    p_stream = &io;
    p_print = &io;
    p_info = &io;
    setStartSec(startSeconds);
    setEndSec(endSeconds);
  }

  TimedStream(AudioOutput &o, long startSeconds = 0, long endSeconds = -1) {
    p_print = &o;
    p_info = &o;
    setStartSec(startSeconds);
    setEndSec(endSeconds);
  }

  /// Defines the start time in seconds. The audio before the start time will be
  /// skipped
  void setStartSec(uint32_t startSeconds) {
    start_ms = startSeconds * 1000;
    calculateByteLimits();
  }

  /// Defines the start time in milliseconds
  void setStartMs(uint32_t ms) {
    start_ms = ms;
    calculateByteLimits();
  }

  /// Defines (an optional) the end time in seconds. After the end time no audio
  /// is played and available() will return 0
  void setEndSec(uint32_t endSeconds) {
    end_ms = endSeconds * 1000;
    calculateByteLimits();
  }

  /// Defines the (optional) end time in milliseconds
  void setEndMs(uint32_t ms) {
    end_ms = ms;
    calculateByteLimits();
  }

  /// Returns true if we are in a valid time range and are still playing sound
  bool isPlaying() {
    if (current_bytes < start_bytes) return false;
    if (end_bytes > 0 && current_bytes > end_bytes) return false;
    return true;
  }

  /// Returns true if we are not past the end time;
  bool isActive() {
    return (current_bytes < end_bytes && current_bytes >= start_bytes);
  }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  bool begin() override {
    calculateByteLimits();
    current_bytes = 0;
    LOGI("byte range %u - %u",(unsigned) start_bytes,(unsigned) end_bytes);
    return true;
  }

  operator bool() { return isActive(); }

  /// Provides only data for the indicated start and end time. Only supported
  /// for data which does not contain any heder information: so PCM, mp3 should
  /// work!
  size_t readBytes(uint8_t *data, size_t len) override {
    // if reading is not supported we stop
    if (p_stream == nullptr) return 0;
    // Positioin to start
    if (start_bytes > current_bytes){
      consumeBytes(start_bytes - current_bytes);
    }
    // if we are past the end we stop
    if (!isActive()) return 0;
    // read the data now
    size_t result = 0;
    do {
      result = p_stream->readBytes(data, len);
      current_bytes += len;
      // ignore data before start time
    } while (result > 0 && current_bytes < start_bytes);
    return isPlaying() ? result : 0;
  }

  /// Plays only data for the indiated start and end time
  size_t write(const uint8_t *data, size_t len) override {
    if (current_bytes >= end_bytes) return 0; 
    current_bytes += len;
    if (current_bytes < start_bytes) return len;
    return p_print->write(data, len);
  }

  /// Provides the available bytes until the end time has reached
  int available() override {
    if (p_stream == nullptr) return 0;
    return current_bytes < end_bytes  ? p_stream->available() : 0;
  }

  /// Updates the AudioInfo in the current object and in the source or target
  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    if (p_info) p_info->setAudioInfo(info);
    calculateByteLimits();
  }

  int availableForWrite() override {
    return current_bytes < end_bytes  ? p_print->availableForWrite() : 0;
  }

  /// Experimental: if used on mp3 you can set the compression ratio e.g. to 11
  /// which will be used to approximate the time
  void setCompressionRatio(float ratio) { compression_ratio = ratio; }

  /// Calculates the bytes per second from the AudioInfo
  int bytesPerSecond() {
    return info.sample_rate * info.channels * info.bits_per_sample / 8;
  }

  void setOutput(Print &out) { p_print = &out; }

  void setStream(Stream &stream) {
    p_print = &stream;
    p_stream = &stream;
  }

  void setOutput(AudioOutput &out) {
    p_print = &out;
    p_info = &out;
  }

  void setStream(AudioOutput &out) {
    p_print = &out;
    p_info = &out;
  }

  void setStream(AudioStream &stream) {
    p_print = &stream;
    p_stream = &stream;
    p_info = &stream;
  }

  size_t size() {
    return end_bytes - start_bytes;
  }

 protected:
  Stream *p_stream = nullptr;
  Print *p_print = nullptr;
  AudioInfoSupport *p_info = nullptr;
  uint32_t start_ms = 0;
  uint32_t end_ms = UINT32_MAX;
  uint32_t start_bytes = 0;
  uint32_t end_bytes = UINT32_MAX;
  uint32_t current_bytes = 0;
  float compression_ratio = 1.0;

  void consumeBytes(uint32_t len){
    int open = len;
    uint8_t buffer[1024];
    while (open > 0){
      int toread = min(1024, open);
      p_stream->readBytes(buffer, toread);
      open -= toread;
    }
    current_bytes += len;
    LOGD("consumed %u -> %u",(unsigned) len, (unsigned)current_bytes);
  }

  void calculateByteLimits() {
    float bytes_per_second = bytesPerSecond();
    if (bytes_per_second > 0) {
      start_bytes = bytes_per_second * start_ms / compression_ratio / 1000;
      end_bytes = bytes_per_second * end_ms / compression_ratio / 1000;
    } else {
      LOGE("AudioInfo not defined");
    }
  }
};

/**
 * @brief Flexible functionality to extract one or more channels from a
 * multichannel signal. Warning: the destinatios added with addOutput
 * are not automatically notified about audio changes.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
class ChannelsSelectOutput : public AudioOutput {
 public:
  ChannelsSelectOutput() = default;

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  bool begin() {
    AudioOutput::begin();
    // make sure that selected channels are valid
    for (auto &out : out_channels) {
      for (auto &ch : out.channels) {
        if (ch > cfg.channels - 1) {
          LOGE("Channel '%d' not valid for max %d channels", ch, cfg.channels);
          return false;
        }
      }
    }
    return true;
  }

  /// Define the channel to be selected to the specified output. 0: first
  /// (=left) channel, 1: second (=right) channel
  void addOutput(AudioOutput &out, uint16_t channel) {
    Vector<uint16_t> channels;
    channels.push_back(channel);
    ChannelSelectionOutputDef def;
    def.channels = channels;
    def.p_out = &out;
    def.p_audio_info = &out;
    out_channels.push_back(def);
  }

  /// Define the channel to be selected to the specified output. 0: first
  /// (=left) channel, 1: second (=right) channel
  void addOutput(AudioStream &out, uint16_t channel) {
    Vector<uint16_t> channels;
    channels.push_back(channel);
    ChannelSelectionOutputDef def;
    def.channels = channels;
    def.p_out = &out;
    def.p_audio_info = &out;
    out_channels.push_back(def);
  }

  /// Define the channel to be selected to the specified output. 0: first
  /// (=left) channel, 1: second (=right) channel
  void addOutput(Print &out, uint16_t channel) {
    Vector<uint16_t> channels;
    channels.push_back(channel);
    ChannelSelectionOutputDef def;
    def.channels = channels;
    def.p_out = &out;
    out_channels.push_back(def);
  }

  /// Define the stereo channels to be selected to the specified output. 0:
  /// first (=left) channel, 1: second (=right) channel
  void addOutput(Print &out, uint16_t left, uint16_t right) {
    Vector<uint16_t> channels;
    channels.push_back(left);
    channels.push_back(right);
    ChannelSelectionOutputDef def;
    def.channels = channels;
    def.p_out = &out;
    out_channels.push_back(def);
  }

  /// Define the stereo channels to be selected to the specified output. 0:
  /// first (=left) channel, 1: second (=right) channel
  void addOutput(AudioOutput &out, uint16_t left, uint16_t right) {
    Vector<uint16_t> channels;
    channels.push_back(left);
    channels.push_back(right);
    ChannelSelectionOutputDef def;
    def.channels = channels;
    def.p_out = &out;
    def.p_audio_info = &out;
    out_channels.push_back(def);
  }

  /// Define the stereo channels to be selected to the specified output. 0:
  /// first (=left) channel, 1: second (=right) channel
  void addOutput(AudioStream &out, uint16_t left, uint16_t right) {
    Vector<uint16_t> channels;
    channels.push_back(left);
    channels.push_back(right);
    ChannelSelectionOutputDef def;
    def.channels = channels;
    def.p_out = &out;
    def.p_audio_info = &out;
    out_channels.push_back(def);
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (!is_active) return false;
    LOGD("write %d", (int)len);
    switch (cfg.bits_per_sample) {
      case 16:
        return writeT<int16_t>(data, len);
      case 24:
        return writeT<int24_t>(data, len);
      case 32:
        return writeT<int32_t>(data, len);
      default:
        return 0;
    }
  }

  void setAudioInfo(AudioInfo ai) override {
    this->cfg = ai;
    //notifyAudioChange(ai);
    for (auto &info : out_channels) {
      auto p_notify = info.p_audio_info;
      if (p_notify != nullptr) {
        AudioInfo result{ai};
        result.channels = info.channels.size();
        p_notify->setAudioInfo(result);
      }
    }
  }

 protected:
  struct ChannelSelectionOutputDef {
    Print *p_out = nullptr;
    AudioInfoSupport *p_audio_info = nullptr;
    SingleBuffer<uint8_t> buffer{CHANNEL_SELECT_BUFFER_SIZE};
    Vector<uint16_t> channels{0};
  };
  Vector<ChannelSelectionOutputDef> out_channels{0};

  template <typename T>
  size_t writeT(const uint8_t *buffer, size_t size) {
    if (!is_active) return 0;
    int sample_count = size / sizeof(T);
    //int result_size = sample_count / cfg.channels;
    T *data = (T *)buffer;

    for (int i = 0; i < sample_count; i += cfg.channels) {
      T *frame = data + i;
      for (auto &out : out_channels) {
        T out_frame[out.channels.size()] = {0};
        int ch_out = 0;
        for (auto &ch : out.channels) {
          // make sure we have a valid channel
          int channel = (ch < cfg.channels) ? ch : cfg.channels - 1;
          out_frame[ch_out++] = frame[channel];
        }
        // write to buffer
        size_t written = out.buffer.writeArray((const uint8_t *)&out_frame, sizeof(out_frame));
        // write buffer to final output
        if (out.buffer.availableForWrite()<sizeof(out_frame)){
          out.p_out->write(out.buffer.data(), out.buffer.available());
          out.buffer.reset();
        }
        // if (written != sizeof(out_frame)) {
        //   LOGW("Could not write all samples %d -> %d", sizeof(out_frame), written);
        // }
      }
    }
    return size;
  }

  /// Determine number of channels for destination
  int getChannels(Print *out, int defaultChannels) {
    for (auto &channels_select : out_channels) {
      if (channels_select.p_out == out) return channels_select.channels.size();
    }
    return defaultChannels;
  }
};

}  // namespace audio_tools
