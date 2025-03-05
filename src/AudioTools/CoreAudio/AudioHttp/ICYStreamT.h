#pragma once
#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioHttp/AbstractURLStream.h"
#include "AudioTools/CoreAudio/AudioMetaData/MetaDataICY.h"

namespace audio_tools {

/**
 * @brief Icecast/Shoutcast Audio Stream which splits the data into metadata and
 * audio data. The Audio data is provided via the regular stream functions. The
 * metadata is handled with the help of the MetaDataICY state machine and
 * provided via a callback method.
 *
 * This is basically just a URLStream with the metadata turned on.
 *
 * If you run into performance issues, check if the data is provided chunked.
 * In this chase you can check if setting the protocol to "HTTP/1.0" improves
 * the situation.
 *
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

 template<class T>
 class ICYStreamT : public AbstractURLStream {
 public:
 ICYStreamT(int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACEI();
    setReadBufferSize(readBufferSize);
  }

  /// Default constructor
  ICYStreamT(const char* ssid, const char* password,
            int readBufferSize = DEFAULT_BUFFER_SIZE) : ICYStreamT(readBufferSize) {
    TRACEI();
    setSSID(ssid);
    setPassword(password);
  }

  ICYStreamT(Client& clientPar, int readBufferSize = DEFAULT_BUFFER_SIZE) : ICYStreamT(readBufferSize) {
    TRACEI();
    setClient(clientPar);
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

  void setReadBufferSize(int readBufferSize) {
    url.setReadBufferSize(readBufferSize);
  }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char* ssid) override { url.setSSID(ssid); }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char* password) override { url.setPassword(password); }

  /// if set to true, it activates the power save mode which comes at the cost
  /// of performance! - By default this is deactivated. ESP32 Only!
  void setPowerSave(bool active) { url.setPowerSave(active);}

  /// Define the Root PEM Certificate for SSL:
  void setCACert(const char* cert){
    url.setCACert(cert);
  }
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

 protected:
  T url;
  MetaDataICY icy;  // icy state machine
  void (*callback)(MetaDataType info, const char* str, int len) = nullptr;
};

}

