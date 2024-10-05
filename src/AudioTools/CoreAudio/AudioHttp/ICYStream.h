#pragma once
#ifdef USE_URL_ARDUINO

#include "AudioConfig.h"
#include "URLStreamBuffered.h"
#include "AudioTools/CoreAudio/AudioMetaData/MetaDataICY.h"

namespace audio_tools {

/**
 * @brief Icecast/Shoutcast Audio Stream which splits the data into metadata and
 * audio data. The Audio data is provided via the regular stream functions. The
 * metadata is handled with the help of the MetaDataICY state machine and
 * provided via a callback method.
 *
 * This is basically just a URLStream with the metadata turned on.
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ICYStream : public AbstractURLStream {
 public:
  ICYStream(int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACEI();
    setReadBufferSize(readBufferSize);
  }

  ICYStream(Client& clientPar, int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACEI();
    setReadBufferSize(readBufferSize);
    setClient(clientPar);
  }

  /// Default constructor
  ICYStream(const char* network, const char* password,
            int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACEI();
    setReadBufferSize(readBufferSize);
    setSSID(network);
    setPassword(password);
  }

  /// Defines the meta data callback function
  virtual bool setMetadataCallback(void (*fn)(MetaDataType info,
                                              const char* str,
                                              int len)) override {
    TRACED();
    callback = fn;
    icy.setCallback(fn);
    return true;
  }

  // Icy http get request to the indicated url
  virtual bool begin(const char* urlStr, const char* acceptMime = nullptr,
                     MethodID action = GET, const char* reqMime = "",
                     const char* reqData = "") override {
    TRACED();
    // accept metadata
    url.httpRequest().header().put("Icy-MetaData", "1");
    bool result = url.begin(urlStr, acceptMime, action, reqMime, reqData);

    if (result) {
      // setup icy
      ICYUrlSetup icySetup;
      int iceMetaint = icySetup.setup(url.httpRequest());
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

  /// provides the available method from the URLStream
  virtual int available() override { return url.available(); }

  /// reads the audio bytes
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

  // Read character and processes using the MetaDataICY state engine
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

  operator bool() { return url; }

  /// provides access to the HttpRequest
  virtual HttpRequest& httpRequest() override { return url.httpRequest(); }

  void setReadBufferSize(int readBufferSize) {
    url.setReadBufferSize(readBufferSize);
  }

  /// (Re-)defines the client
  void setClient(Client& client) override { url.setClient(client); }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char* ssid) override { url.setSSID(ssid); }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char* password) override { url.setPassword(password); }

  /// if set to true, it activates the power save mode which comes at the cost
  /// of performance! - By default this is deactivated. ESP32 Only!
  void setPowerSave(bool active) { url.setPowerSave(active);}

 protected:
  URLStream url;
  MetaDataICY icy;  // icy state machine
  void (*callback)(MetaDataType info, const char* str, int len) = nullptr;
};

}  // namespace audio_tools
#endif