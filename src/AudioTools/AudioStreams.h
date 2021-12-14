#pragma once
#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTypes.h"
#include "Buffers.h"

namespace audio_tools {

static const char *UNDERFLOW_MSG = "data underflow";

/**
 * @brief Base class for all Audio Streams. It support the boolean operator to
 * test if the object is ready with data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioStream : public Stream, public AudioBaseInfoDependent {
 public:
  // overwrite to do something useful
  virtual void setAudioInfo(AudioBaseInfo info) {
    LOGD(LOG_METHOD);
    info.logInfo();
  }

  virtual size_t readBytes(char *buffer, size_t length) {
    return readBytes((uint8_t *)buffer, length);
  }

  virtual size_t readBytes(uint8_t *buffer, size_t length) = 0;

  virtual size_t write(const uint8_t *buffer, size_t size) = 0;

  operator bool() { return available() > 0; }
};

/**
 * @brief Same as AudioStream - but we do not have any abstract methods
 *
 */
class AudioStreamX : public Stream, public AudioBaseInfoDependent {
 public:
  virtual size_t readBytes(uint8_t *buffer, size_t length) { return 0; }
  virtual size_t write(const uint8_t *buffer, size_t size) { return 0; }
  virtual size_t write(uint8_t) { return 0; }
  virtual int available() { return 0; };
  virtual int read() { return -1; }
  virtual int peek() { return -1; }
  virtual void flush() {}
  virtual void setAudioInfo(audio_tools::AudioBaseInfo){}
};

/**
 * @brief To be used to support implementations where the readBytes is not
 * virtual
 *
 */
class AudioStreamWrapper : public AudioStream {
 public:
  AudioStreamWrapper(Stream &s) { p_stream = &s; }

  virtual size_t readBytes(uint8_t *buffer, size_t length) {
    return p_stream->readBytes(buffer, length);
  }

  int read() { return p_stream->read(); }

  int peek() { return p_stream->peek(); }

  int available() { return p_stream->available(); }

  virtual size_t write(uint8_t c) { return p_stream->write(c); }

  virtual size_t write(const uint8_t *buffer, size_t size) {
    return p_stream->write(buffer, size);
  }

  virtual int availableForWrite() { return p_stream->availableForWrite(); }

  virtual void flush() { p_stream->flush(); }

 protected:
  Stream *p_stream;
};

/**
 * @brief A simple Stream implementation which is backed by allocated memory
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class MemoryStream : public AudioStream {
 public:
  MemoryStream(int buffer_size = 512) {
    LOGD("MemoryStream: %d", buffer_size);
    this->buffer_size = buffer_size;
    this->buffer = new uint8_t[buffer_size];
    this->owns_buffer = true;
  }

  MemoryStream(const uint8_t *buffer, int buffer_size) {
    LOGD("MemoryStream: %d", buffer_size);
    this->buffer_size = buffer_size;
    this->write_pos = buffer_size;
    this->buffer = (uint8_t *)buffer;
    this->owns_buffer = false;
  }

  ~MemoryStream() {
    LOGD(LOG_METHOD);
    if (owns_buffer) delete[] buffer;
  }

  // resets the read pointer
  void begin() {
    LOGD(LOG_METHOD);
    write_pos = buffer_size;
    read_pos = 0;
  }

  virtual size_t write(uint8_t byte) {
    int result = 0;
    if (write_pos < buffer_size) {
      result = 1;
      buffer[write_pos] = byte;
      write_pos++;
    }
    return result;
  }

  virtual size_t write(const uint8_t *buffer, size_t size) {
    size_t result = 0;
    for (size_t j = 0; j < size; j++) {
      if (!write(buffer[j])) {
        break;
      }
      result = j;
    }
    return result;
  }

  virtual int available() { return write_pos - read_pos; }

  virtual int read() {
    int result = peek();
    if (result >= 0) {
      read_pos++;
    }
    return result;
  }

  virtual size_t readBytes(char *buffer, size_t length) {
    size_t count = 0;
    while (count < length) {
      int c = read();
      if (c < 0) break;
      *buffer++ = (char)c;
      count++;
    }
    return count;
  }

  virtual int peek() {
    int result = -1;
    if (available() > 0) {
      result = buffer[read_pos];
    }
    return result;
  }

  virtual void flush() {}

  virtual void clear(bool reset = false) {
    write_pos = 0;
    read_pos = 0;
    if (reset) {
      // we clear the buffer data
      memset(buffer, 0, buffer_size);
    }
  }

 protected:
  int write_pos = 0;
  int read_pos = 0;
  int buffer_size = 0;
  uint8_t *buffer = nullptr;
  bool owns_buffer = false;
};

/**
 * @brief Source for reading generated tones. Please note
 * - that the output is for one channel only!
 * - we do not support reading of individual characters!
 * - we do not support any write operations
 * @param generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class T>
class GeneratedSoundStream : public AudioStream, public AudioBaseInfoSource {
 public:
  GeneratedSoundStream(SoundGenerator<T> &generator) {
    LOGD(LOG_METHOD);
    this->generator_ptr = &generator;
  }

  AudioBaseInfo defaultConfig() { return this->generator_ptr->defaultConfig(); }

  /// start the processing
  void begin() {
    LOGD(LOG_METHOD);
    generator_ptr->begin();
    if (audioBaseInfoDependent != nullptr)
      audioBaseInfoDependent->setAudioInfo(generator_ptr->audioInfo());
    active = true;
  }

  /// start the processing
  void begin(AudioBaseInfo cfg) {
    LOGD(LOG_METHOD);
    generator_ptr->begin(cfg);
    if (audioBaseInfoDependent != nullptr)
      audioBaseInfoDependent->setAudioInfo(generator_ptr->audioInfo());
    active = true;
  }

  /// stop the processing
  void end() {
    LOGD(LOG_METHOD);
    generator_ptr->stop();
    active = false;
  }

  virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
    audioBaseInfoDependent = &bi;
  }

  /// unsupported operations
  virtual size_t write(uint8_t) { return not_supported(); };

  /// unsupported operations
  virtual int availableForWrite() { return not_supported(); }

  /// unsupported operations
  virtual size_t write(const uint8_t *buffer, size_t size) {
    return not_supported();
  }

  /// unsupported operations
  virtual int read() { return -1; }
  /// unsupported operations
  virtual int peek() { return -1; }
  /// This is unbounded so we just return the buffer size
  virtual int available() { return DEFAULT_BUFFER_SIZE; }

  /// privide the data as byte stream
  size_t readBytes(uint8_t *buffer, size_t length) override {
    LOGD("GeneratedSoundStream::readBytes: %zu", length);
    return generator_ptr->readBytes(buffer, length);
  }

  operator bool() { return active; }

  void flush() {}

 protected:
  SoundGenerator<T> *generator_ptr;
  bool active = false;
  AudioBaseInfoDependent *audioBaseInfoDependent = nullptr;

  int not_supported() {
    LOGE("GeneratedSoundStream-unsupported operation!");
    return 0;
  }
};

/**
 * @brief The Arduino Stream supports operations on single characters. This is
 * usually not the best way to push audio information, but we will support it
 * anyway - by using a buffer. On reads: if the buffer is empty it gets refilled
 * - for writes if it is full it gets flushed.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BufferedStream : public AudioStream {
 public:
  BufferedStream(size_t buffer_size) {
    LOGD(LOG_METHOD);
    buffer = new SingleBuffer<uint8_t>(buffer_size);
  }

  ~BufferedStream() {
    LOGD(LOG_METHOD);
    if (buffer != nullptr) {
      delete buffer;
    }
  }

  /// writes a byte to the buffer
  virtual size_t write(uint8_t c) {
    if (buffer->isFull()) {
      flush();
    }
    return buffer->write(c);
  }

  /// Use this method: write an array
  virtual size_t write(const uint8_t *data, size_t len) {
    LOGD("%s: %zu", LOG_METHOD, len);
    flush();
    return writeExt(data, len);
  }

  /// empties the buffer
  virtual void flush() {
    // just dump the memory of the buffer and clear it
    if (buffer->available() > 0) {
      writeExt(buffer->address(), buffer->available());
      buffer->reset();
    }
  }

  /// reads a byte - to be avoided
  virtual int read() {
    if (buffer->isEmpty()) {
      refill();
    }
    return buffer->read();
  }

  /// peeks a byte - to be avoided
  virtual int peek() {
    if (buffer->isEmpty()) {
      refill();
    }
    return buffer->peek();
  };

  /// Use this method !!
  size_t readBytes(uint8_t *data, size_t length) {
    if (buffer->isEmpty()) {
      return readExt(data, length);
    } else {
      return buffer->readArray(data, length);
    }
  }

  /// Returns the available bytes in the buffer: to be avoided
  virtual int available() {
    if (buffer->isEmpty()) {
      refill();
    }
    return buffer->available();
  }

  /// Clears all the data in the buffer
  void clear() { buffer->reset(); }

 protected:
  SingleBuffer<uint8_t> *buffer = nullptr;

  // refills the buffer with data from i2s
  void refill() {
    size_t result = readExt(buffer->address(), buffer->size());
    buffer->setAvailable(result);
  }

  virtual size_t writeExt(const uint8_t *data, size_t len) = 0;
  virtual size_t readExt(uint8_t *data, size_t length) = 0;
};

/**
 * @brief The Arduino Stream which provides silence and simulates a null device
 * when used as audio target
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NullStream : public BufferedStream {
 public:
  NullStream(bool measureWrite = false) : BufferedStream(100) {
    is_measure = measureWrite;
  }

  void begin(AudioBaseInfo info, int opt = 0) {}

  void begin() {}

  AudioBaseInfo defaultConfig(int opt = 0) {
    AudioBaseInfo info;
    return info;
  }

  /// Define object which need to be notified if the basinfo is changing
  void setNotifyAudioChange(AudioBaseInfoDependent &bi) {}

  void setAudioInfo(AudioBaseInfo info) {}

 protected:
  size_t total = 0;
  unsigned long timeout = 0;
  bool is_measure;

  virtual size_t writeExt(const uint8_t *data, size_t len) {
    if (is_measure) {
      if (millis() < timeout) {
        total += len;
      } else {
        LOGI("Thruput = %zu kBytes/sec", total / 1000);
        total = 0;
        timeout = millis() + 1000;
      }
    }
    return len;
  }
  virtual size_t readExt(uint8_t *data, size_t len) {
    memset(data, 0, len);
    return len;
  }
};

/**
 * @brief A Stream backed by a Ringbuffer. We can write to the end and read from
 * the beginning of the stream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RingBufferStream : public AudioStream {
 public:
  RingBufferStream(int size = DEFAULT_BUFFER_SIZE) {
    buffer = new RingBuffer<uint8_t>(size);
  }

  ~RingBufferStream() {
    if (buffer != nullptr) {
      delete buffer;
    }
  }

  virtual int available() {
    // LOGD("RingBufferStream::available: %zu",buffer->available());
    return buffer->available();
  }

  virtual void flush() {}

  virtual int peek() { return buffer->peek(); }
  virtual int read() { return buffer->read(); }

  virtual size_t readBytes(uint8_t *data, size_t length) {
    return buffer->readArray(data, length);
  }

  virtual size_t write(const uint8_t *data, size_t len) {
    // LOGD("RingBufferStream::write: %zu",len);
    return buffer->writeArray(data, len);
  }

  virtual size_t write(uint8_t c) { return buffer->write(c); }

 protected:
  RingBuffer<uint8_t> *buffer = nullptr;
};

/**
 * @brief A Stream backed by a SingleBufferStream. We assume that the memory is
 * externally allocated and that we can submit only full buffer records, which
 * are then available for reading.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ExternalBufferStream : public AudioStream {
 public:
  ExternalBufferStream() { LOGD(LOG_METHOD); }

  virtual int available() { return buffer.available(); }

  virtual void flush() {}

  virtual int peek() { return buffer.peek(); }

  virtual int read() { return buffer.read(); }

  virtual size_t readBytes(uint8_t *data, size_t length) {
    return buffer.readArray(data, length);
  }

  virtual size_t write(const uint8_t *data, size_t len) {
    buffer.onExternalBufferRefilled((void *)data, len);
    return len;
  }

  virtual size_t write(uint8_t c) {
    LOGE("not implemented: %s", LOG_METHOD);
    return 0;
  }

 protected:
  SingleBuffer<uint8_t> buffer;
};

/**
 * @brief AudioOutput class which stores the data in a temporary buffer.
 * The buffer can be consumed e.g. by a callback function by calling read();

 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T>
class CallbackBufferedStream : public BufferedStream {
 public:
  // Default constructor
  CallbackBufferedStream(int bufferSize, int bufferCount)
      : BufferedStream(bufferSize) {
    callback_buffer_ptr = new NBuffer<T>(bufferSize, bufferCount);
  }

  virtual ~CallbackBufferedStream() { delete callback_buffer_ptr; }

  /// Activates the output
  virtual bool begin() {
    active = true;
    return true;
  }

  /// stops the processing
  virtual bool stop() {
    active = false;
    return true;
  };

 protected:
  NBuffer<T> *callback_buffer_ptr;
  bool active;

  virtual size_t writeExt(const uint8_t *data, size_t len) {
    return callback_buffer_ptr->writeArray(data, len / sizeof(T));
  }

  virtual size_t readExt(uint8_t *data, size_t len) {
    return callback_buffer_ptr->readArray(data, len / sizeof(T));
    ;
  }
};

/**
 * @brief TimerCallbackAudioStream Configuration
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
struct TimerCallbackAudioStreamInfo : public AudioBaseInfo {
  RxTxMode rx_tx_mode = RX_MODE;
  uint16_t buffer_size = DEFAULT_BUFFER_SIZE;
  bool use_timer = true;
  int timer_id = 0;
  TimerFunction timer_function = DirectTimerCallback;
  bool adapt_sample_rate = false;
  uint16_t (*callback)(uint8_t *data, uint16_t len) = nullptr;
};

// forward declaration: relevant only if use_timer == true
// void IRAM_ATTR timerCallback(void* obj);

/**
 * @brief Callback driven Audio Source (rx_tx_mode==RX_MODE) or Audio Sink
 * (rx_tx_mode==TX_MODE). This class allows to to integrate external libraries
 * in order to consume or generate a data stream which is based on a timer
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class TimerCallbackAudioStream : public BufferedStream,
                                 public AudioBaseInfoSource {
  friend void IRAM_ATTR timerCallback(void *obj);

 public:
  TimerCallbackAudioStream() : BufferedStream(80) { LOGD(LOG_METHOD); }

  ~TimerCallbackAudioStream() {
    LOGD(LOG_METHOD);
    if (timer != nullptr) delete timer;
    if (buffer != nullptr) delete buffer;
    if (frame != nullptr) delete[] frame;
  }

  /// Provides the default configuration
  TimerCallbackAudioStreamInfo defaultConfig() {
    TimerCallbackAudioStreamInfo def;
    return def;
  }

  /// updates the audio information
  virtual void setAudioInfo(AudioBaseInfo info) {
    LOGD(LOG_METHOD);
    if (cfg.sample_rate != info.sample_rate || cfg.channels != info.channels ||
        cfg.bits_per_sample != info.bits_per_sample) {
      bool do_restart = active;
      if (do_restart) end();
      cfg.sample_rate = info.sample_rate;
      cfg.channels = info.channels;
      cfg.bits_per_sample = info.bits_per_sample;
      if (do_restart) begin(cfg);
    }
  }

  /// Defines the target that needs to be notified
  void setNotifyAudioChange(AudioBaseInfoDependent &bi) { notifyTarget = &bi; }

  /// Provides the current audio information
  TimerCallbackAudioStreamInfo audioInfo() { return cfg; }

  void begin(TimerCallbackAudioStreamInfo config) {
    LOGD("%s:  %s", LOG_METHOD,
         config.rx_tx_mode == RX_MODE ? "RX_MODE" : "TX_MODE");
    this->cfg = config;
    this->frameCallback = config.callback;
    if (cfg.use_timer) {
      frameSize = cfg.bits_per_sample * cfg.channels / 8;
      frame = new uint8_t[frameSize];
      buffer = new RingBuffer<uint8_t>(cfg.buffer_size);
      timer = new TimerAlarmRepeating(cfg.timer_function, cfg.timer_id);
      time = AudioUtils::toTimeUs(cfg.sample_rate);
      LOGI("sample_rate: %u -> time: %u milliseconds", cfg.sample_rate, time);
      timer->setCallbackParameter(this);
      timer->begin(timerCallback, time, TimeUnit::US);
    }

    notifyAudioChange();
    active = true;
  }

  /// Restart the processing
  void begin() {
    LOGD(LOG_METHOD);
    if (this->frameCallback != nullptr) {
      if (cfg.use_timer) {
        timer->begin(timerCallback, time, TimeUnit::US);
      }
      active = true;
    }
  }

  /// Stops the processing
  void end() {
    LOGD(LOG_METHOD);
    if (cfg.use_timer) {
      timer->end();
    }
    active = false;
  }

  /// Provides the effective sample rate
  uint16_t currentSampleRate() { return currentRateValue; }

 protected:
  TimerCallbackAudioStreamInfo cfg;
  AudioBaseInfoDependent *notifyTarget = nullptr;
  bool active = false;
  uint16_t (*frameCallback)(uint8_t *data, uint16_t len);
  // below only relevant with timer
  TimerAlarmRepeating *timer = nullptr;
  RingBuffer<uint8_t> *buffer = nullptr;
  uint8_t *frame = nullptr;
  uint16_t frameSize = 0;
  uint32_t time = 0;
  unsigned long lastTimestamp = 0u;
  uint32_t currentRateValue = 0;
  uint32_t printCount = 0;

  // used for audio sink
  virtual size_t writeExt(const uint8_t *data, size_t len) {
    if (!active) return 0;
    LOGD(LOG_METHOD);
    size_t result = 0;
    if (!cfg.use_timer) {
      result = frameCallback((uint8_t *)data, len);
    } else {
      result = buffer->writeArray((uint8_t *)data, len);
    }
    if (++printCount % 10000 == 0) printSampleRate();
    return result;
  }

  // used for audio source
  virtual size_t readExt(uint8_t *data, size_t len) {
    if (!active) return 0;
    LOGD(LOG_METHOD);

    size_t result = 0;
    if (!cfg.use_timer) {
      result = frameCallback(data, len);
    } else {
      result = buffer->readArray(data, len);
    }
    if (++printCount % 10000 == 0) printSampleRate();
    return result;
  }

  /// calculates the effective sample rate
  virtual void measureSampleRate() {
    unsigned long ms = millis();
    if (lastTimestamp > 0u) {
      uint32_t diff = ms - lastTimestamp;
      if (diff > 0) {
        uint16_t rate = 1 * 1000 / diff;

        if (currentRateValue == 0) {
          currentRateValue = rate;
        } else {
          currentRateValue = (currentRateValue + rate) / 2;
        }
      }
    }
    lastTimestamp = ms;
  }

  /// log and update effective sample rate
  virtual void printSampleRate() {
    LOGI("effective sample rate: %d", currentRateValue);
    if (cfg.adapt_sample_rate &&
        abs((int)currentRateValue - cfg.sample_rate) > 200) {
      cfg.sample_rate = currentRateValue;
      notifyAudioChange();
    }
  }

  /// Update Audio Information in target device
  virtual void notifyAudioChange() {
    if (notifyTarget != nullptr) {
      notifyTarget->setAudioInfo(cfg);
    }
  }

  static void IRAM_ATTR timerCallback(void *obj);
};

// relevant only if use_timer == true
void TimerCallbackAudioStream::timerCallback(void *obj) {
  TimerCallbackAudioStream *src = (TimerCallbackAudioStream *)obj;
  if (src != nullptr) {
    // LOGD("%s:  %s", LOG_METHOD, src->cfg.rx_tx_mode==RX_MODE ?
    // "RX_MODE":"TX_MODE");
    if (src->cfg.rx_tx_mode == RX_MODE) {
      // input
      uint16_t available_bytes = src->frameCallback(src->frame, src->frameSize);
      uint16_t buffer_available = src->buffer->availableForWrite();
      if (buffer_available < available_bytes) {
        // if buffer is full make space
        uint16_t to_clear = available_bytes - buffer_available;
        uint8_t tmp[to_clear];
        src->buffer->readArray(tmp, to_clear);
      }
      if (src->buffer->writeArray(src->frame, available_bytes) !=
          available_bytes) {
        // LOGE(UNDERFLOW_MSG);
        assert(false);
      }
    } else {
      // output
      if (src->buffer != nullptr && src->frame != nullptr &&
          src->frameSize > 0) {
        uint16_t available_bytes =
            src->buffer->readArray(src->frame, src->frameSize);
        if (available_bytes !=
            src->frameCallback(src->frame, available_bytes)) {
          LOGE(UNDERFLOW_MSG);
        }
      }
    }
    src->measureSampleRate();
  }
}

}  // namespace audio_tools
