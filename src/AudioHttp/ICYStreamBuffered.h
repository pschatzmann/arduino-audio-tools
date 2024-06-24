#pragma once
#include "AudioConfig.h" 
#if defined(USE_CONCURRENCY) && defined(USE_URL_ARDUINO)
#include "AudioHttp/ICYStream.h"

namespace audio_tools {

/**
 * @brief ICYStream implementation for the ESP32 based on a FreeRTOS task
 * This is a Icecast/Shoutcast Audio Stream which splits the data into metadata
 * and audio data. The Audio data is provided via the regular stream functions.
 * The metadata is handled with the help of the MetaDataICY state machine and
 * provided via a callback method.
 *
 * This is basically just a URLStream with the metadata turned on.
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ICYStreamBuffered : public AbstractURLStream {
 public:
  ICYStreamBuffered(int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACEI();
    urlStream.setReadBufferSize(readBufferSize);
    taskStream.setInput(urlStream);
  }

  ICYStreamBuffered(Client& clientPar,
                    int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACEI();
    urlStream.setReadBufferSize(readBufferSize);
    setClient(clientPar);
    taskStream.setInput(urlStream);
  }

  ICYStreamBuffered(const char* network, const char* password,
                    int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACEI();
    urlStream.setReadBufferSize(readBufferSize);
    setSSID(network);
    setPassword(password);
    taskStream.setInput(urlStream);
  }

  virtual bool setMetadataCallback(void (*fn)(MetaDataType info,
                                              const char* str,
                                              int len)) override {
    TRACED();
    return urlStream.setMetadataCallback(fn);
  }

  virtual bool begin(const char* urlStr, const char* acceptMime = nullptr,
                     MethodID action = GET, const char* reqMime = "",
                     const char* reqData = "") override {
    TRACED();
    // start real stream
    bool result = urlStream.begin(urlStr, acceptMime, action, reqMime, reqData);
    // start reader task
    taskStream.begin();
    return result;
  }

  virtual void end() override {
    TRACED();
    taskStream.end();
    urlStream.end();
  }

  virtual int available() override { return taskStream.available(); }

  virtual size_t readBytes(uint8_t* data, size_t len) override {
    size_t result = taskStream.readBytes(data, len);
    LOGD("%s: %zu -> %zu", LOG_METHOD, len, result);
    return result;
  }

  virtual int read() override { return taskStream.read(); }

  virtual int peek() override { return taskStream.peek(); }

  /// Not implemented
  virtual void flush() override {}

  /// provides access to the HttpRequest
  virtual HttpRequest& httpRequest() override {
    return urlStream.httpRequest();
  }

  /// (Re-)defines the client
  void setClient(Client& client) override { urlStream.setClient(client); }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char* ssid) override { urlStream.setSSID(ssid); }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char* password) override {
    urlStream.setPassword(password);
  }

 protected:
  BufferedTaskStream taskStream;
  ICYStream urlStream;
};

}  // namespace audio_tools

#endif  // USE_TASK
