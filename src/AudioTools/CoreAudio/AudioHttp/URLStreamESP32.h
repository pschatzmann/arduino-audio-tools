#pragma once

#include "AudioTools/CoreAudio/AudioHttp/AbstractURLStream.h"
#include "AudioTools/CoreAudio/AudioHttp/HttpRequest.h"
#include "AudioTools/CoreAudio/AudioHttp/ICYStreamT.h"
#include "AudioTools/CoreAudio/AudioHttp/URLStreamBufferedT.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace audio_tools {

/**
 * @brief Login to Wifi using the ESP32 IDF functionality. This can be
 * accessed with the global object IDF_WIFI
 * @author Phil Schatzmann
 * @ingroup http
 * @copyright GPLv3
 */
class WiFiESP32 {
 public:
  bool begin(const char* ssid, const char* password) {
    TRACEI();
    if (is_open) return true;
    if (!setupWIFI(ssid, password)) {
      LOGE("setupWIFI failed");
      return false;
    }

    return true;
  }

  void end() {
    TRACED();
    if (is_open) {
      TRACEI();
      esp_wifi_stop();
      esp_wifi_deinit();
    }
    is_open = false;
  }

  void setPowerSave(wifi_ps_type_t powerSave) { power_save = powerSave; }

  bool isConnected() { return is_open; }

 protected:
  volatile bool is_open = false;
  esp_ip4_addr_t ip = {0};
  wifi_ps_type_t power_save = WIFI_PS_NONE;

  bool setupWIFI(const char* ssid, const char* password) {
    assert(ssid != nullptr);
    assert(password != nullptr);
    LOGI("setupWIFI: %s", ssid);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      ret = nvs_flash_init();
    }

    if (esp_netif_init() != ESP_OK) {
      LOGE("esp_netif_init");
      return false;
    };

    if (esp_event_loop_create_default() != ESP_OK) {
      LOGE("esp_event_loop_create_default");
      return false;
    };

    esp_netif_t* itf = esp_netif_create_default_wifi_sta();
    if (itf == nullptr) {
      LOGE("esp_netif_create_default_wifi_sta");
      return false;
    };

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_sta_event_handler, this, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_sta_event_handler, this, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
      LOGE("esp_wifi_init");
      return false;
    }

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_ps(power_save);

    wifi_config_t sta_config;
    memset(&sta_config, 0, sizeof(wifi_config_t));
    strncpy((char*)sta_config.sta.ssid, ssid, 32);
    strncpy((char*)sta_config.sta.password, password, 32);
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);

    // start wifi
    bool rc = esp_wifi_start() == ESP_OK;
    if (!rc) {
      LOGE("esp_wifi_start");
    }
    return rc;
  }

  static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
    WiFiESP32* self = (WiFiESP32*)arg;

    if (event_base == WIFI_EVENT) {
      switch (event_id) {
        case WIFI_EVENT_STA_START:
          LOGI("WIFI_EVENT_STA_START");
          esp_wifi_connect();
          break;
        case WIFI_EVENT_STA_DISCONNECTED:
          LOGI("WIFI_EVENT_STA_DISCONNECTED");
          esp_wifi_connect();
          break;
      }
    } else if (event_base == IP_EVENT) {
      switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
          ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
          self->ip = event->ip_info.ip;
          self->is_open = true;
          LOGI("==> Station connected with IP: " IPSTR ", GW: " IPSTR
               ", Mask: " IPSTR ".",
               IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.gw),
               IP2STR(&event->ip_info.netmask));
          break;
        }
      }
    }
  }

} static IDF_WIFI;

class URLStreamESP32;
static URLStreamESP32* actualURLStreamESP32 = nullptr;

/**
 * @brief URLStream using the ESP32 IDF API.
 *
 * For Https you need to provide the certificate.
 * Execute: openssl s_client -showcerts -connect www.howsmyssl.com:443
 * </dev/null
 *
 * To completely disable the certificate check, you will need to go to ESP-TLS
 * in menuconfig, enable "Allow potentially insecure options" and then enable
 * "Skip server certificate verification by default" (accepting risks).
 *
 * This is unfortunately not an option when using Arduino!
 *
 * @author Phil Schatzmann
 * @ingroup http
 * @copyright GPLv3
 */

class URLStreamESP32 : public AbstractURLStream {
 public:
  URLStreamESP32(const char* ssid, const char* pwd) {
    setSSID(ssid);
    setPassword(pwd);
    _timeout = 8000;
  }
  URLStreamESP32() : URLStreamESP32(nullptr, nullptr) {}
  ~URLStreamESP32() { end(); }
  // executes the URL request
  virtual bool begin(const char* urlStr, const char* acceptMime = "",
                     MethodID action = GET, const char* reqMime = "",
                     const char* reqData = "") {
    TRACED();
    // start wifi if necessary and possible
    if (ssid != nullptr) {
      if (!IDF_WIFI.begin(ssid, password)) {
        LOGE("Wifi failed");
        return false;
      }
      if (!IDF_WIFI.isConnected()) {
        // wait for connection
        Serial.print("Waiting for connection ");
        while (!IDF_WIFI.isConnected()) {
          delay(200);
          Serial.print(".");
        }
        Serial.println();
      }
    }

    // for headers
    actualURLStreamESP32 = this;

    // determine wifi country
    wifi_country_t cntry;
    memset(&cntry, 0, sizeof(wifi_country_t));
    esp_wifi_get_country(&cntry);
    char text[4] = {0};
    strncpy(text, cntry.cc, 3);
    LOGI("wifi country: %s", text);

    // fill http_config
    esp_http_client_config_t http_config;
    memset(&http_config, 0, sizeof(http_config));
    http_config.url = urlStr;
    http_config.user_agent = DEFAULT_AGENT;
    http_config.event_handler = http_event_handler;
    http_config.buffer_size = buffer_size;
    http_config.timeout_ms = _timeout;
    http_config.user_data = this;
    // for SSL certificate
    if (pem_cert != nullptr) {
      http_config.cert_pem = (const char*)pem_cert;
      http_config.cert_len = pem_cert_len;
    }
    // for SSL (use of a bundle for certificate verification)
    if (crt_bundle_attach != nullptr) {
      http_config.crt_bundle_attach = crt_bundle_attach;
    }

    switch (action) {
      case GET:
        http_config.method = HTTP_METHOD_GET;
        break;
      case POST:
        http_config.method = HTTP_METHOD_POST;
        break;
      case PUT:
        http_config.method = HTTP_METHOD_PUT;
        break;
      case DELETE:
        http_config.method = HTTP_METHOD_DELETE;
        break;
      default:
        LOGE("Unsupported action: %d", action);
        break;
    }

    // Init only the first time
    if (client_handle == nullptr) {
      client_handle = esp_http_client_init(&http_config);
    }

    // process header parameters
    if (!StrView(acceptMime).isEmpty()) addRequestHeader(ACCEPT, acceptMime);
    if (!StrView(reqMime).isEmpty()) addRequestHeader(CONTENT_TYPE, reqMime);
    List<HttpHeaderLine*>& lines = request.header().getHeaderLines();
    for (auto it = lines.begin(); it != lines.end(); ++it) {
      if ((*it)->active) {
        esp_http_client_set_header(client_handle, (*it)->key.c_str(),
                                   (*it)->value.c_str());
      }
    }

    // Open http
    if (esp_http_client_open(client_handle, 0) != ESP_OK) {
      LOGE("esp_http_client_open");
      return false;
    }

    // Determine the result
    int content_length = esp_http_client_fetch_headers(client_handle);
    int status_code = esp_http_client_get_status_code(client_handle);
    LOGI("status_code: %d / content_length: %d", status_code, content_length);

    // Process post/put data
    StrView data(reqData);
    if (!data.isEmpty()) {
      write((const uint8_t*)reqData, data.length());
    }

    return status_code == 200;
  }
  // ends the request
  virtual void end() override {
    esp_http_client_close(client_handle);
    esp_http_client_cleanup(client_handle);
  }

  /// Writes are not supported
  int availableForWrite() override { return 1024; }

  /// Sets the ssid that will be used for logging in (when calling begin)
  virtual void setSSID(const char* ssid) { this->ssid = ssid; }

  /// Sets the password that will be used for logging in (when calling begin)
  virtual void setPassword(const char* password) { this->password = password; }

  /// Sets the power save mode (default false)!
  virtual void setPowerSave(bool ps) {
    IDF_WIFI.setPowerSave(ps ? WIFI_PS_MAX_MODEM : WIFI_PS_NONE);
  }

  size_t write(const uint8_t* data, size_t len) override {
    TRACED();
    return esp_http_client_write(client_handle, (const char*)data, len);
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    TRACED();
    return esp_http_client_read(client_handle, (char*)data, len);
  }

  /// Adds/Updates a request header
  void addRequestHeader(const char* key, const char* value) override {
    TRACED();
    request.addRequestHeader(key, value);
  }
  /// Provides a header entry
  const char* getReplyHeader(const char* key) override {
    return request.getReplyHeader(key);
  }

  /// Define the Root PEM Certificate for SSL: Method compatible with Arduino
  /// WiFiClientSecure API
  void setCACert(const char* cert) override {
    int len = strlen(cert);
    setCACert((const uint8_t*)cert, len + 1);
  }

  /// Attach and enable use of a bundle for certificate verification  e.g.
  /// esp_crt_bundle_attach or arduino_esp_crt_bundle_attach
  void setCACert(esp_err_t (*cb)(void *conf)){
    crt_bundle_attach = cb;
  }

  /// Defines the read buffer size
  void setReadBufferSize(int size) {
    buffer_size = size; }

  /// Used for request and reply header parameters
  HttpRequest& httpRequest() override {
    return request; }

  /// Does nothing
  void setClient(Client& client) override {}

 protected:
  int id = 0;
  HttpRequest request;
  esp_http_client_handle_t client_handle = nullptr;
  bool is_power_save = false;
  const char* ssid = nullptr;
  const char* password = nullptr;
  int buffer_size = DEFAULT_BUFFER_SIZE;
  const uint8_t* pem_cert = nullptr;
  int pem_cert_len = 0;
  esp_err_t (*crt_bundle_attach)(void *conf) = nullptr;


  /// Define the Root PEM Certificate for SSL: the last byte must be null, the
  /// len is including the ending null
  void setCACert(const uint8_t* cert, int len) {
    pem_cert_len = len;
    pem_cert = cert;
    // certificate must end with traling null
    assert(cert[len - 1] == 0);
  }

  static esp_err_t http_event_handler(esp_http_client_event_t* evt) {
    switch (evt->event_id) {
      case HTTP_EVENT_ERROR:
        LOGI("HTTP_EVENT_ERROR");
        break;
      case HTTP_EVENT_ON_CONNECTED:
        LOGD("HTTP_EVENT_ON_CONNECTED");
        break;
      case HTTP_EVENT_HEADER_SENT:
        LOGD("HTTP_EVENT_HEADER_SENT");
        break;
      case HTTP_EVENT_ON_HEADER:
        LOGI("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
             evt->header_value);
        // store reply headers
        actualURLStreamESP32->request.reply().put(evt->header_key,
                                                  evt->header_value);
        break;
      case HTTP_EVENT_ON_DATA:
        LOGD("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
      case HTTP_EVENT_ON_FINISH:
        LOGI("HTTP_EVENT_ON_FINISH");
        break;
      case HTTP_EVENT_DISCONNECTED:
        LOGI("HTTP_EVENT_DISCONNECTED");
        break;
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 3, 7)
      case HTTP_EVENT_REDIRECT:
        LOGI("HTTP_EVENT_REDIRECT");
        break;
#endif
    }
    return ESP_OK;
  }
};

/// ICYStream
using ICYStreamESP32 = ICYStreamT<URLStreamESP32>;
using URLStreamBufferedESP32 = URLStreamBufferedT<URLStreamESP32>;
using ICYStreamBufferedESP32 = URLStreamBufferedT<ICYStreamESP32>;

/// Support URLStream w/o Arduino
#if !defined(ARDUINO)
using URLStream = URLStreamESP32;
using URLStreamBuffered = URLStreamBufferedESP32;
using ICYStreamBuffered = ICYStreamBufferedESP32;
#endif

}  // namespace audio_tools