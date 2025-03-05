#pragma once
#include "AudioConfig.h"
#if defined(USE_CONCURRENCY)
#include "AudioTools/AudioLibs/Concurrency.h"
#include "AudioTools/CoreAudio/AudioHttp/AbstractURLStream.h"
#include "AudioTools/CoreAudio/BaseStream.h"

#ifndef URL_STREAM_CORE
#define URL_STREAM_CORE 0
#endif

#ifndef URL_STREAM_PRIORITY
#define URL_STREAM_PRIORITY 2
#endif

#ifndef URL_STREAM_BUFFER_COUNT
#define URL_STREAM_BUFFER_COUNT 10
#endif

#ifndef STACK_SIZE
#define STACK_SIZE 30000
#endif

namespace audio_tools {

/**
 * @brief A FreeRTOS task is filling the buffer from the indicated stream. Only
 * to be used on the ESP32
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BufferedTaskStream : public AudioStream {
 public:
  BufferedTaskStream() { TRACEI(); };

  BufferedTaskStream(AudioStream &input) {
    TRACEI();
    setInput(input);
  }

  ~BufferedTaskStream() {
    TRACEI();
    stop();
  }

  /// Define an explicit the buffer size in bytes
  void setBufferSize(int bufferSize, int bufferCount) {
    buffers.resize(bufferSize, bufferCount);
  }

  virtual void begin(bool wait = true) {
    TRACED();
    active = true;
    ready = false;
    task.begin(std::bind(&BufferedTaskStream::processTask, this));
    if (!wait) ready = true;
  }

  virtual void end() {
    TRACED();
    task.end();
    active = false;
    ready = false;
  }

  virtual void setInput(AudioStream &input) {
    TRACED();
    p_stream = &input;
  }

  /// writes a byte to the buffer
  virtual size_t write(uint8_t c) override { return 0; }

  /// Use this method: write an array
  virtual size_t write(const uint8_t *data, size_t len) override { return 0; }

  /// empties the buffer
  virtual void flush() override {}

  /// reads a byte - to be avoided
  virtual int read() override {
    // if (!ready) return -1;
    int result = -1;
    result = buffers.read();
    return result;
  }

  /// peeks a byte - to be avoided
  virtual int peek() override {
    // if (!ready) return -1;
    int result = -1;
    result = buffers.peek();
    return result;
  };

  /// Use this method !!
  virtual size_t readBytes(uint8_t *data, size_t len) override {
    // if (!ready) return 0;
    size_t result = 0;
    result = buffers.readArray(data, len);
    LOGD("%s: %zu -> %zu", LOG_METHOD, len, result);
    return result;
  }

  /// Returns the available bytes in the buffer: to be avoided
  virtual int available() override {
    // if (!ready) return 0;
    int result = 0;
    result = buffers.available();
    return result;
  }

 protected:
  AudioStream *p_stream = nullptr;
  bool active = false;
  Task task{"BufferedTaskStream", STACK_SIZE, URL_STREAM_PRIORITY,
            URL_STREAM_CORE};
  SynchronizedNBuffer buffers{DEFAULT_BUFFER_SIZE, URL_STREAM_BUFFER_COUNT};
  bool ready = false;

  void processTask() {
    size_t available_to_write = this->buffers.availableForWrite();
    if (*(this->p_stream) && available_to_write > 0) {
      size_t to_read = min(available_to_write, (size_t)512);
      uint8_t buffer[to_read];
      size_t avail_read = this->p_stream->readBytes((uint8_t *)buffer, to_read);
      size_t written = this->buffers.writeArray(buffer, avail_read);

      if (written != avail_read) {
        LOGE("DATA Lost! %zu reqested, %zu written!", avail_read, written);
      }

    } else {
      // 3ms at 44100 stereo is about 529.2 bytes
      delay(3);
    }
    // buffer is full we start to provide data
    if (available_to_write == 0) {
      this->ready = true;
    }
  }
};

/**
 * @brief URLStream implementation for the ESP32 based on a separate FreeRTOS
 * task: the
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class T>
class URLStreamBufferedT : public AbstractURLStream {
 public:
  URLStreamBufferedT(int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACED();
    urlStream.setReadBufferSize(readBufferSize);
    taskStream.setInput(urlStream);
  }

  URLStreamBufferedT(const char *network, const char *password,
                     int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACED();
    urlStream.setReadBufferSize(readBufferSize);
    setSSID(network);
    setPassword(password);
    taskStream.setInput(urlStream);
  }

#ifdef ARDUINO

  URLStreamBufferedT(Client &clientPar,
                     int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACED();
    urlStream.setReadBufferSize(readBufferSize);
    setClient(clientPar);
    taskStream.setInput(urlStream);
  }
#endif

  /// Defines the buffer that holds the with encoded data
  void setBufferSize(int bufferSize, int bufferCount) {
    taskStream.setBufferSize(bufferSize, bufferCount);
  }

  bool begin(const char *urlStr, const char *acceptMime = nullptr,
             MethodID action = GET, const char *reqMime = "",
             const char *reqData = "") {
    TRACED();
    // start real stream
    bool result = urlStream.begin(urlStr, acceptMime, action, reqMime, reqData);
    // start buffer task
    taskStream.begin();
    return result;
  }

  virtual int available() { return taskStream.available(); }

  virtual size_t readBytes(uint8_t *data, size_t len) {
    size_t result = taskStream.readBytes(data, len);
    LOGD("%s: %zu -> %zu", LOG_METHOD, len, result);
    return result;
  }

  virtual int read() { return taskStream.read(); }

  virtual int peek() { return taskStream.peek(); }

  virtual void flush() {}

  void end() {
    TRACED();
    taskStream.end();
    urlStream.end();
  }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char *ssid) override { urlStream.setSSID(ssid); }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char *password) override {
    urlStream.setPassword(password);
  }

  /// ESP32 only: PowerSave off (= default setting) is much faster
  void setPowerSave(bool ps) override { urlStream.setPowerSave(ps); }

  /// Define the Root PEM Certificate for SSL
  void setCACert(const char *cert) { urlStream.setCACert(cert); }

   /// Adds/Updates a request header
   void addRequestHeader(const char* key, const char* value) override {
    urlStream.addRequestHeader(key, value);
  }
  /// Provides reply header info
  const char* getReplyHeader(const char* key) override {
    return urlStream.getReplyHeader(key);
  }
 
  /// provides access to the HttpRequest
  HttpRequest &httpRequest() { return urlStream.httpRequest(); }

  /// (Re-)defines the client
  void setClient(Client &client) override { urlStream.setClient(client); }

 protected:
  BufferedTaskStream taskStream;
  T urlStream;
};

}  // namespace audio_tools

#endif
