#pragma once

#include "AudioConfig.h"
#ifdef USE_URL_ARDUINO

#if defined(ESP32)
# include <Client.h>
# include <WiFi.h>
# include <WiFiClientSecure.h>
# include <esp_wifi.h>
#endif

#include "AbstractURLStream.h"
#include "HttpRequest.h"
#include "AudioTools/CoreAudio/AudioBasic/Str.h"

namespace audio_tools {

/**
 * @brief Represents the content of a URL as Stream. We use the WiFi.h API
 * @author Phil Schatzmann
 * @ingroup http
 * @copyright GPLv3
 *
 */
class URLStream : public AbstractURLStream {
 public:
  URLStream(int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACED();
    setReadBufferSize(readBufferSize);
  }

  URLStream(Client& clientPar, int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACED();
    setReadBufferSize(readBufferSize);
    setClient(clientPar);
  }

  URLStream(const char* network, const char* password,
            int readBufferSize = DEFAULT_BUFFER_SIZE) {
    TRACED();
    setReadBufferSize(readBufferSize);
    setSSID(network);
    setPassword(password);
  }

  URLStream(const URLStream&) = delete;

  ~URLStream() {
    TRACED();
    end();
#ifdef USE_WIFI_CLIENT_SECURE
    if (clientSecure != nullptr) {
      delete clientSecure;
      clientSecure = nullptr;
    }
#endif
#ifdef USE_WIFI
    if (clientInsecure != nullptr) {
      delete clientInsecure;
      clientInsecure = nullptr;
    }
#endif
  }

  /// (Re-)defines the client
  void setClient(Client& clientPar) override { client = &clientPar; }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char* ssid) override { this->network = ssid; }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char* password) override { this->password = password; }

  void setReadBufferSize(int readBufferSize) {
    read_buffer_size = readBufferSize;
  }

  /// Execute http request: by default we use a GET request
  virtual bool begin(const char* urlStr, const char* acceptMime = nullptr,
                     MethodID action = GET, const char* reqMime = "",
                     const char* reqData = "") override {
    LOGI("%s: %s", LOG_METHOD, urlStr);
    if (!preProcess(urlStr, acceptMime)) {
      LOGE("preProcess failed");
      return false;
    }
    int result = process<const char*>(action, url, reqMime, reqData);
    if (result > 0) {
      size = request.contentLength();
      LOGI("size: %d", (int)size);
      if (size >= 0 && wait_for_data) {
        waitForData(clientTimeout);
      }
    }
    total_read = 0;
    active = result == 200;
    LOGI("==> http status: %d", result);
#if USE_AUDIO_LOGGING
    custom_log_level.reset();
#endif
    return active;
  }

  /// Execute e.g. http POST request which submits the content as a stream
  virtual bool begin(const char* urlStr, const char* acceptMime,
                     MethodID action, const char* reqMime, Stream& reqData,
                     int len = -1) {
    LOGI("%s: %s", LOG_METHOD, urlStr);
    if (!preProcess(urlStr, acceptMime)) {
      LOGE("preProcess failed");
      return false;
    }
    int result = process<Stream&>(action, url, reqMime, reqData, len);
    if (result > 0) {
      size = request.contentLength();
      LOGI("size: %d", (int)size);
      if (size >= 0 && wait_for_data) {
        waitForData(clientTimeout);
      }
    }
    total_read = 0;
    active = result == 200;
    LOGI("==> http status: %d", result);
#if USE_AUDIO_LOGGING
    custom_log_level.reset();
#endif
    return active;
  }

  virtual void end() override {
    if (active) request.stop();
    active = false;
    clear();
  }

  virtual int available() override {
    if (!active || !request) return 0;

    int result = request.available();
    LOGD("available: %d", result);
    return result;
  }

  virtual size_t readBytes(uint8_t* data, size_t len) override {
    if (!active || !request) return 0;

    int read = request.read((uint8_t*)&data[0], len);
    if (read < 0) {
      read = 0;
    }
    total_read += read;
    LOGD("readBytes %d -> %d", (int)len, read);
    return read;
  }

  virtual int read() override {
    if (!active) return -1;
    // lazy allocation since this is rarely used
    read_buffer.resize(read_buffer_size);

    fillBuffer();
    total_read++;
    return isEOS() ? -1 : read_buffer[read_pos++];
  }

  virtual int peek() override {
    if (!active) return -1;
    // lazy allocation since this is rarely used
    read_buffer.resize(read_buffer_size);

    fillBuffer();
    return isEOS() ? -1 : read_buffer[read_pos];
  }

  virtual void flush() override {}

  virtual size_t write(uint8_t) override { return not_supported(0); }

  virtual size_t write(const uint8_t*, size_t len) override {
    return not_supported(0);
  }

  /// provides access to the HttpRequest
  virtual HttpRequest& httpRequest() override { return request; }

  operator bool() { return active && request.isReady(); }

  /// Defines the client timeout
  virtual void setTimeout(int ms) { clientTimeout = ms; }

  /// if set to true, it activates the power save mode which comes at the cost
  /// of performance! - By default this is deactivated. ESP32 Only!
  void setPowerSave(bool ps) { is_power_save = ps; }

  /// If set to true, new undefined reply parameters will be stored
  void setAutoCreateLines(bool flag) {
    httpRequest().reply().setAutoCreateLines(flag);
  }

  /// Sets if the connection should be close automatically
  void setConnectionClose(bool close) {
    httpRequest().setConnection(close ? CON_CLOSE : CON_KEEP_ALIVE);
  }

  /// Releases the memory from the request and reply
  void clear() {
    httpRequest().reply().clear();
    httpRequest().header().clear();
    read_buffer.resize(0);
    read_pos = 0;
    read_size = 0;
  }

  /// Adds/Updates a request header
  void addRequestHeader(const char* header, const char* value) {
    request.header().put(header, value);
  }

  /// Callback which allows you to add additional paramters dynamically
  void setOnConnectCallback(void (*callback)(
      HttpRequest& request, Url& url, HttpRequestHeader& request_header)) {
    request.setOnConnectCallback(callback);
  }

  void setWaitForData(bool flag) { wait_for_data = flag; }

  int contentLength() { return size; }

  size_t totalRead() { return total_read; }

  /// waits for some data - returns false if the request has failed
  virtual bool waitForData(int timeout) {
    TRACED();
    uint32_t end = millis() + timeout;
    if (request.available() == 0) {
      LOGI("Request written ... waiting for reply");
      while (request.available() == 0) {
        if (millis() > end) break;
        // stop waiting if we got an error
        if (request.reply().statusCode() >= 300) {
          LOGE("Error code recieved ... stop waiting for reply");
          break;
        }
        delay(500);
      }
    }
    LOGD("available: %d", request.available());
    return request.available() > 0;
  }

#if USE_AUDIO_LOGGING
  /// Defines the class specific custom log level
  void setLogLevel(AudioLogger::LogLevel level) { custom_log_level.set(level); }
#endif
  const char* urlStr() { return url_str.c_str(); }

 protected:
  HttpRequest request;
#if USE_AUDIO_LOGGING
  CustomLogLevel custom_log_level;
#endif
  Str url_str;
  Url url;
  long size;
  long total_read;
  // buffered single byte read
  Vector<uint8_t> read_buffer{0};
  uint16_t read_buffer_size = DEFAULT_BUFFER_SIZE;
  uint16_t read_pos;
  uint16_t read_size;
  bool active = false;
  bool wait_for_data = true;
  // optional
  const char* network = nullptr;
  const char* password = nullptr;
  Client* client = nullptr; // client defined via setClient
#ifdef USE_WIFI
  WiFiClient* clientInsecure = nullptr; // wifi client for http
#endif
#ifdef USE_WIFI_CLIENT_SECURE
  WiFiClientSecure* clientSecure = nullptr; // wifi client for https
#endif
  int clientTimeout = URL_CLIENT_TIMEOUT;                  // 60000;
  unsigned long handshakeTimeout = URL_HANDSHAKE_TIMEOUT;  // 120000
  bool is_power_save = false;

  bool preProcess(const char* urlStr, const char* acceptMime) {
    TRACED();
#if USE_AUDIO_LOGGING
    custom_log_level.set();
#endif
    url_str = urlStr;
    url.setUrl(url_str.c_str());
    int result = -1;

    // close it - if we have an active connection
    if (active) end();

#ifdef USE_WIFI
    // optional: login if necessary if no external client is defined
    if (client == nullptr){
      if (!login()){
        LOGE("Not connected");
        return false;
      }
    }
#endif

    // request.reply().setAutoCreateLines(false);
    if (acceptMime != nullptr) {
      request.setAcceptMime(acceptMime);
    }

    // setup client
    Client& client = getClient(url.isSecure());
    request.setClient(client);

    // set timeout
    client.setTimeout(clientTimeout / 1000);
    request.setTimeout(clientTimeout);

#if defined(ESP32) && defined(USE_WIFI_CLIENT_SECURE)
    // There is a bug in IDF 4!
    if (clientSecure != nullptr) {
      clientSecure->setHandshakeTimeout(handshakeTimeout);
    }

    // Performance optimization for ESP32
    if (!is_power_save) {
      esp_wifi_set_ps(WIFI_PS_NONE);
    }
#endif

    return true;
  }

  /// Process the Http request and handle redirects
  template <typename T>
  int process(MethodID action, Url& url, const char* reqMime, T reqData,
              int len = -1) {
    TRACED();
    // keep icy across redirect requests ?
    const char* icy = request.header().get("Icy-MetaData");

    int status_code = request.process(action, url, reqMime, reqData, len);
    // redirect
    while (request.reply().isRedirectStatus()) {
      const char* redirect_url = request.reply().get(LOCATION);
      if (redirect_url != nullptr) {
        LOGW("Redirected to: %s", redirect_url);
        url.setUrl(redirect_url);
        Client* p_client = &getClient(url.isSecure());
        p_client->stop();
        request.setClient(*p_client);
        if (icy) {
          request.header().put("Icy-MetaData", icy);
        }
        status_code = request.process(action, url, reqMime, reqData, len);
      } else {
        LOGE("Location is null");
        break;
      }
    }
    return status_code;
  }

  /// Determines the client
  Client& getClient(bool isSecure) {
#ifdef USE_WIFI_CLIENT_SECURE
    if (isSecure) {
      if (clientSecure == nullptr) {
        clientSecure = new WiFiClientSecure();
        clientSecure->setInsecure();
      }
      LOGI("WiFiClientSecure");
      return *clientSecure;
    }
#endif
#ifdef USE_WIFI
    if (clientInsecure == nullptr) {
      clientInsecure = new WiFiClient();
      LOGI("WiFiClient");
    }
    return *clientInsecure;
#else
    if (client == nullptr){ 
      LOGE("Client not set");
      stop();
    }
    return *client;  // to avoid compiler warning
#endif
  }

  inline void fillBuffer() {
    if (isEOS()) {
      // if we consumed all bytes we refill the buffer
      read_size = readBytes(&read_buffer[0], read_buffer_size);
      read_pos = 0;
    }
  }

  inline bool isEOS() { return read_pos >= read_size; }

  bool login() {
#ifdef USE_WIFI
    if (network != nullptr && password != nullptr &&
        WiFi.status() != WL_CONNECTED) {
      TRACEI();
      WiFi.begin(network, password);
      while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
      }
      Serial.println();
      delay(10);
      return WiFi.status() == WL_CONNECTED;
    }
    return WiFi.status() == WL_CONNECTED;
#else 
    return false;
#endif
  }
};

}  // namespace audio_tools

#endif
