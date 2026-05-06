#pragma once

#include "AudioToolsConfig.h"

#if defined(USE_WIFI)
# include "WiFiInclude.h"
#endif

#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioTools/Communication/HTTP/AbstractURLStream.h"
#include "AudioTools/Communication/HTTP/HttpRequest.h"
#include "AudioTools/Communication/HTTP/URLStreamBufferedT.h"

namespace audio_tools {

/**
 * @brief Represents the content of a URL as Stream. We use the WiFi.h API.
 * If you run into performance issues, check if the data is provided chunked.
 * In this chase you can check if setting the protocol to "HTTP/1.0" improves
 * the situation.
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
    if (client_secure != nullptr) {
      delete client_secure;
      client_secure = nullptr;
    }
    if (client_insecure != nullptr) {
      delete client_insecure;
      client_insecure = nullptr;
    }
  }

  /// (Re-)defines the client
  void setClient(Client& clientPar) override { client = &clientPar; }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char* ssid) override { this->network = ssid; }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char* password) override { this->password = password; }

  /// Defines the buffer that is used by individual read() or peek() calls
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
      content_length = request.contentLength();
      LOGI("contentLength: %d", (int)content_length);
      if (content_length >= 0 && wait_for_data) {
        waitForData(client_timeout);
      }
    }
    total_read = 0;
    active = result == 200;
    LOGI("==> http status: %d", result);
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
      content_length = request.contentLength();
      LOGI("size: %d", (int)content_length);
      if (content_length >= 0 && wait_for_data) {
        waitForData(client_timeout);
      }
    }
    total_read = 0;
    active = result == 200;
    LOGI("==> http status: %d", result);
    return active;
  }

  /// Ends the request and releases the memory
  virtual void end() override {
    if (active) request.stop();
    active = false;
    clear();
  }

  virtual int available() override {
    if (!active) return 0;

    int result = request.available();
    LOGD("available: %d", result);
    return result;
  }

  virtual size_t readBytes(uint8_t* data, size_t len) override {
    if (!active) return 0;

    int read = request.read(data, len);
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

  operator bool() override { return active && request.isReady(); }

  /// Defines the client timeout in ms
  virtual void setTimeout(int ms) { client_timeout = ms; }

  /// if set to true, it activates the power save mode which comes at the cost
  /// of performance! - By default this is deactivated. ESP32 Only!
  void setPowerSave(bool ps) override { is_power_save = ps; }

  /// If set to true, new undefined reply parameters will be stored
  void setAutoCreateLines(bool flag) {
    httpRequest().reply().setAutoCreateLines(flag);
  }

  /// Sets if the connection should be close automatically
  void setConnectionClose(bool close) override {
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
  void addRequestHeader(const char* key, const char* value) override {
    request.header().put(key, value);
  }

  /// Provides the value of the reply header for the given key
  const char* getReplyHeader(const char* key) override {
    return request.reply().get(key);
  }

  /// Callback which allows you to add additional paramters dynamically
  void setOnConnectCallback(void (*callback)(
      HttpRequest& request, Url& url, HttpRequestHeader& request_header)) {
    request.setOnConnectCallback(callback);
  }

  /// Defines if the stream should wait for data after the request has been sent
  void setWaitForData(bool flag) { wait_for_data = flag; }

  /// returns the content length 
  int contentLength() override { return content_length; }

  /// returns the total number of bytes read from the stream
  size_t totalRead() override { return total_read; }

  /// Waits for some data - returns false if the request has failed
  bool waitForData (int timeout) override{
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
    int avail = request.available();
    LOGD("available: %d", avail);
    return avail > 0;
  }

  /// returns the url as string
  const char* urlStr() override { return url_str.c_str(); }

  /// Define the Root PEM Certificate for SSL
  void setCACert(const char* cert) override{
    if (client_secure!=nullptr) client_secure->setCACert(cert);
  }

  /// Returns the content length of the request
  size_t size() { return content_length; }

 protected:
  HttpRequest request;
  Str url_str;
  Url url;
  long content_length = 0;
  long total_read = 0;
  // buffered single byte read
  Vector<uint8_t> read_buffer{0};
  uint16_t read_buffer_size = DEFAULT_BUFFER_SIZE;
  uint16_t read_pos = 0;
  uint16_t read_size = 0;
  bool active = false;
  bool wait_for_data = true;
  Client* client = nullptr; // client defined via setClient
  WiFiClient* client_insecure = nullptr; // wifi client for http
  WiFiClientSecure* client_secure = nullptr; // wifi client for https
  int client_timeout = URL_CLIENT_TIMEOUT;                  // 60000;
  unsigned long handshake_timeout = URL_HANDSHAKE_TIMEOUT;  // 120000
  bool is_power_save = false;
  // optional
  const char* network = nullptr;
  const char* password = nullptr;

  bool preProcess(const char* urlStr, const char* acceptMime) {
    TRACED();
    url_str = urlStr;
    url.setUrl(url_str.c_str());
    int result = -1;

    // close it - if we have an active connection
    if (active) end();

    // optional: login if necessary if no external client is defined
    if (client == nullptr){
      if (!login()){
        LOGE("Not connected");
        return false;
      }
    }

    // request.reply().setAutoCreateLines(false);
    if (acceptMime != nullptr) {
      request.setAcceptMime(acceptMime);
    }

    // setup client
    Client& client = getClient(url.isSecure());
    request.setClient(client);
    request.setTimeout(client_timeout);

#if defined(ESP32)
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
    if (isSecure) {
      if (client_secure == nullptr) {
        client_secure = new WiFiClientSecure();
        client_secure->setInsecure();
        client_secure->setTimeout(client_timeout);
#ifdef ESP32
        client_secure->setConnectionTimeout(client_timeout);
        client_secure->setHandshakeTimeout(handshake_timeout);
#endif
#ifdef RP2040_HOWER
        client_secure->setTLSConnectTimeout(client_timeout);
#endif
      }
      LOGI("WiFiClientSecure");
      return *client_secure;
    }
    if (client_insecure == nullptr) {
      client_insecure = new WiFiClient();
      client_insecure->setTimeout(client_timeout);
#ifdef ESP32
      client_insecure->setConnectionTimeout(client_timeout);
#endif
      LOGI("WiFiClient");
    }
    return *client_insecure;
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
  }
};


#if defined(USE_CONCURRENCY)
/// Type alias for buffered URLStream
using URLStreamBuffered = URLStreamBufferedT<URLStream>;

#endif

}  // namespace audio_tools
