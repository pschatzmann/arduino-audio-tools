#pragma once
#include "AudioTools/Communication/HTTP/AbstractURLStream.h"
#include "AudioTools/CoreAudio/AudioMetaData/MetaDataICY.h"
#include "AudioToolsConfig.h"

namespace audio_tools {

/**
 * @brief Icecast/Shoutcast audio stream that separates ICY metadata from audio bytes.
 *
 * ICY/Shoutcast servers interleave metadata blocks into the audio byte stream.
 * This wrapper enables ICY metadata handling while exposing a clean audio-only
 * stream via the standard read/available methods. Metadata is parsed by
 * MetaDataICY and delivered through a user-supplied callback.
 *
 * The class inherits from AbstractURLStream and delegates network/HTTP work to
 * an underlying URL-like stream instance of type T. ICY support is enabled by
 * adding the request header "Icy-MetaData: 1" and by removing metadata bytes
 * from the returned audio stream while forwarding metadata via the callback.
 *
 * Notes
 * - If you run into performance issues, check if the server delivers data with
 *   HTTP chunked transfer encoding. In that case, using protocol "HTTP/1.0"
 *   can sometimes improve performance.
 * - Audio bytes returned by read()/readBytes() contain no metadata; metadata is
 *   only available through the callback.
 * - Use the following predefined usings:
 *   - ICYStream = ICYStreamT<URLStream>;
 *   - BufferedICYStream = ICYStreamT<BufferedURLStream>;
 *   - ICYStreamESP32 = ICYStreamT<URLStreamESP32>;
 *
 * @tparam T Underlying URL stream type; must implement the AbstractURLStream
 *           interface (e.g., provide begin/end/read/available/httpRequest,
 *           credentials, headers, etc.).
 *
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class T>
class ICYStreamT : public AbstractURLStream {
 public:
  ICYStreamT(int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACEI();
    setReadBufferSize(readBufferSize);
  }

  /// Default constructor
  ICYStreamT(const char* ssid, const char* password,
             int readBufferSize = DEFAULT_BUFFER_SIZE)
      : ICYStreamT(readBufferSize) {
    TRACEI();
    setSSID(ssid);
    setPassword(password);
  }

  ICYStreamT(Client& clientPar, int readBufferSize = DEFAULT_BUFFER_SIZE)
      : ICYStreamT(readBufferSize) {
    TRACEI();
    setClient(clientPar);
  }

  /// Defines the metadata callback function
  virtual bool setMetadataCallback(void (*fn)(MetaDataType info,
                                              const char* str,
                                              int len)) override {
    TRACED();
    callback = fn;
    icy.setCallback(fn);
    return true;
  }

  // Performs an HTTP request to the indicated URL with ICY metadata enabled
  virtual bool begin(const char* urlStr, const char* acceptMime = nullptr,
                     MethodID action = GET, const char* reqMime = "",
                     const char* reqData = "") override {
    TRACED();
    // accept metadata
    addRequestHeader("Icy-MetaData", "1");
    bool result = url.begin(urlStr, acceptMime, action, reqMime, reqData);

    if (result) {
      // setup icy
      ICYUrlSetup icySetup;
      int iceMetaint = icySetup.setup(*this);
      // callbacks from http request
      icySetup.executeCallback(callback);
      icy.setIcyMetaInt(iceMetaint);
      icy.begin();

      if (!icy.hasMetaData()) {
        LOGW("url does not provide metadata");
      }
    }
    return result;
  }

  /// Ends the processing
  virtual void end() override {
    TRACED();
    url.end();
    icy.end();
  }

  /// Delegates available() from the underlying stream
  virtual int available() override { return url.available(); }

  /// Reads audio bytes, stripping out any interleaved ICY metadata
  virtual size_t readBytes(uint8_t* data, size_t len) override {
    size_t result = 0;
    if (icy.hasMetaData()) {
      // get data
      int read = url.readBytes(data, len);
      // remove metadata from data
      int pos = 0;
      for (int j = 0; j < read; j++) {
        icy.processChar(data[j]);
        if (icy.isData()) {
          data[pos++] = data[j];
        }
      }
      result = pos;
    } else {
      // fast access if there is no metadata
      result = url.readBytes(data, len);
    }
    LOGD("%s: %zu -> %zu", LOG_METHOD, len, result);
    return result;
  }

  // Reads a single audio byte, skipping metadata via the MetaDataICY state engine
  virtual int read() override {
    int ch = -1;

    // get next data byte
    do {
      ch = url.read();
      if (ch == -1) {
        break;
      }

      icy.processChar(ch);
    } while (!icy.isData());
    return ch;
  }

  /// True if the underlying URL stream is ready/connected
  operator bool() { return url; }

  void setReadBufferSize(int readBufferSize) {
    url.setReadBufferSize(readBufferSize);
  }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char* ssid) override { url.setSSID(ssid); }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char* password) override { url.setPassword(password); }

  /// If set to true, activates power save mode at the cost of performance (ESP32 only).
  void setPowerSave(bool active) { url.setPowerSave(active); }

  /// Define the Root PEM certificate for TLS/SSL
  void setCACert(const char* cert) { url.setCACert(cert); }
  /// Adds/Updates a request header
  void addRequestHeader(const char* key, const char* value) override {
    url.addRequestHeader(key, value);
  }
  /// Provides reply header info
  const char* getReplyHeader(const char* key) override {
    return url.getReplyHeader(key);
  }

  /// provides access to the HttpRequest
  virtual HttpRequest& httpRequest() override { return url.httpRequest(); }

  /// (Re-)defines the client
  void setClient(Client& client) override { url.setClient(client); }

  void setConnectionClose(bool flag) override { url.setConnectionClose(flag); }
  const char* urlStr() override { return url.urlStr(); }
  size_t totalRead() override { return url.totalRead(); };
  int contentLength() override { return url.contentLength(); };
  bool waitForData(int timeout) override { return url.waitForData(timeout); }

 protected:
  T url;
  MetaDataICY icy;  // icy state machine
  void (*callback)(MetaDataType info, const char* str, int len) = nullptr;
};

}  // namespace audio_tools
