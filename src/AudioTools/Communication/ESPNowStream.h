#pragma once
#include <WiFiUdp.h>
#include <esp_now.h>

#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/CoreAudio/Buffers.h"


namespace audio_tools {


// forward declarations
class ESPNowStream;
ESPNowStream *ESPNowStreamSelf = nullptr;

/**
 * @brief A simple RIA locking class for the ESP32 using _lock_t
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Lock {
 public:
  Lock(_lock_t &lock) {
    this->p_lock = &lock;
    _lock_acquire(p_lock);
  }
  ~Lock() { _lock_release(p_lock); }

 protected:
  _lock_t *p_lock = nullptr;
};



/**
 * @brief Configuration for ESP-NOW protocolö.W
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct ESPNowStreamConfig {
  wifi_mode_t wifi_mode = WIFI_STA;
  const char *mac_address = nullptr;
  int channel = 0;
  const char *ssid = nullptr;
  const char *password = nullptr;
  bool use_send_ack = true;  // we wait for
  uint16_t delay_after_write_ms = 2;
  uint16_t delay_after_failed_write_ms = 2000;
  uint16_t buffer_size = ESP_NOW_MAX_DATA_LEN;
  uint16_t buffer_count = 400;
  int write_retry_count = -1; // -1 endless
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  void (*recveive_cb)(const esp_now_recv_info *info, const uint8_t *data,
                      int data_len) = nullptr;
#else
  void (*recveive_cb)(const uint8_t *mac_addr, const uint8_t *data,
                      int data_len) = nullptr;
#endif
  /// to encrypt set primary_master_key and local_master_key to 16 byte strings
  const char *primary_master_key = nullptr;
  const char *local_master_key = nullptr;
  /// esp-now bit rate
  wifi_phy_rate_t rate = WIFI_PHY_RATE_2M_S;
};

/**
 * @brief ESPNow as Arduino Stream
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ESPNowStream : public BaseStream {
 public:
  ESPNowStream() { ESPNowStreamSelf = this; };

  ~ESPNowStream() {
    if (p_buffer != nullptr) delete p_buffer;
  }

  ESPNowStreamConfig defaultConfig() {
    ESPNowStreamConfig result;
    return result;
  }

  /// Returns the mac address of the current ESP32
  const char *macAddress() { 
    static const char* result = WiFi.macAddress().c_str(); 
    return result; 
  }

  /// Defines an alternative send callback
  void setSendCallback(esp_now_send_cb_t cb) { send = cb; }

  /// Defines the Receive Callback - Deactivates the readBytes and available()
  /// methods!
  void setReceiveCallback(esp_now_recv_cb_t cb) { receive = cb; }

  /// Initialization of ESPNow
  bool begin() { return begin(cfg); }

  /// Initialization of ESPNow incl WIFI
  bool begin(ESPNowStreamConfig cfg) {
    this->cfg = cfg;
    WiFi.mode(cfg.wifi_mode);
    // set mac address
    if (cfg.mac_address != nullptr) {
      LOGI("setting mac %s", cfg.mac_address);
      byte mac[ESP_NOW_KEY_LEN];
      str2mac(cfg.mac_address, mac);
      if (esp_wifi_set_mac((wifi_interface_t)getInterface(), mac) != ESP_OK) {
        LOGE("Could not set mac address");
        return false;
      }
      delay(500);  // On some boards calling macAddress to early leads to a race condition.
      // checking if address has been updated
      const char *addr = macAddress();
      if (strcmp(addr, cfg.mac_address) != 0) {
        LOGE("Wrong mac address: %s", addr);
        return false;
      }
    }

    if (WiFi.status() != WL_CONNECTED && cfg.ssid != nullptr &&
        cfg.password != nullptr) {
      WiFi.begin(cfg.ssid, cfg.password);
      while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
      }
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    LOGI("Setting ESP-NEW rate");
    if (esp_wifi_config_espnow_rate(getInterface(), cfg.rate) !=
        ESP_OK) {
      LOGW("Could not set rate");
    }
#endif

    Serial.println();
    Serial.print("mac: ");
    Serial.println(WiFi.macAddress());
    return setup();
  }

  /// DeInitialization
  void end() {
    if (esp_now_deinit() != ESP_OK) {
      LOGE("esp_now_deinit");
    }
    is_init = false;
  }

  /// Adds a peer to which we can send info or from which we can receive info
  bool addPeer(esp_now_peer_info_t &peer) {
    if (!is_init) {
      LOGE("addPeer before begin");
      return false;
    }
    esp_err_t result = esp_now_add_peer(&peer);
    if (result == ESP_OK) {
      LOGI("addPeer: %s", mac2str(peer.peer_addr));
    } else {
      LOGE("addPeer: %d", result);
    }
    return result == ESP_OK;
  }

  /// Adds an array of
  template <size_t size>
  bool addPeers(const char *(&array)[size]) {
    bool result = true;
    for (int j = 0; j < size; j++) {
      const char *peer = array[j];
      if (peer != nullptr) {
        if (!addPeer(peer)) {
          result = false;
        }
      }
    }
    return result;
  }

  /// Adds a peer to which we can send info or from which we can receive info
  bool addPeer(const char *address) {
    esp_now_peer_info_t peer;
    peer.channel = cfg.channel;
    peer.ifidx = getInterface();
    peer.encrypt = false;

    if (StrView(address).equals(cfg.mac_address)){
      LOGW("Did not add own address as peer");
      return true;
    }

    if (isEncrypted()) {
      peer.encrypt = true;
      strncpy((char *)peer.lmk, cfg.local_master_key, 16);
    }

    if (!str2mac(address, peer.peer_addr)) {
      LOGE("addPeer - Invalid address: %s", address);
      return false;
    }
    return addPeer(peer);
  }

  /// Writes the data - sends it to all the peers
  size_t write(const uint8_t *data, size_t len) override {
    int open = len;
    size_t result = 0;
    int retry_count = 0;
    while (open > 0) {
      if (available_to_write > 0) {
        resetAvailableToWrite();
        size_t send_len = min(open, ESP_NOW_MAX_DATA_LEN);
        esp_err_t rc = esp_now_send(nullptr, data + result, send_len);
        // wait for confirmation
        if (cfg.use_send_ack) {
          while (available_to_write == 0) {
            delay(1);
          }
        } else {
          is_write_ok = true;
        }
        // check status
        if (rc == ESP_OK && is_write_ok) {
          open -= send_len;
          result += send_len;
        } else {
          LOGW("Write failed - retrying again");
          retry_count++;
          if (cfg.write_retry_count>0 && retry_count>=cfg.write_retry_count){
            LOGE("Write error after %d retries", cfg.write_retry_count);
            // break loop
            return 0;
          }
        }
        // if we do have no partner to write we stall and retry later
      } else {
        delay(cfg.delay_after_write_ms);
      }

      // Wait some time before we retry
      if (!is_write_ok) {
        delay(cfg.delay_after_failed_write_ms);
      }
    }
    return result;
  }

  /// Reeds the data from the peers
  size_t readBytes(uint8_t *data, size_t len) override {
    if (p_buffer == nullptr) return 0;
    Lock lock(write_lock);
    return p_buffer->readArray(data, len);
  }

  int available() override {
    return p_buffer == nullptr ? 0 : p_buffer->available();
  }

  int availableForWrite() override {
    return cfg.use_send_ack ? available_to_write : cfg.buffer_size;
  }

 protected:
  ESPNowStreamConfig cfg;
  BaseBuffer<uint8_t> *p_buffer = nullptr;
  esp_now_recv_cb_t receive = default_recv_cb;
  esp_now_send_cb_t send = default_send_cb;
  volatile size_t available_to_write;
  bool is_init = false;
  bool is_write_ok = false;
  _lock_t write_lock;

  inline void setupReceiveBuffer(){
    // setup receive buffer
    if (p_buffer == nullptr && cfg.buffer_count > 0) {
      // p_buffer = new NBuffer<uint8_t>(cfg.buffer_size , cfg.buffer_count);
      p_buffer = new RingBuffer<uint8_t>(cfg.buffer_size * cfg.buffer_count);
    }
  }

  inline void resetAvailableToWrite() {
    if (cfg.use_send_ack) {
      available_to_write = 0;
    }
  }

  bool isEncrypted() {
    return cfg.primary_master_key != nullptr && cfg.local_master_key != nullptr;
  }

  wifi_interface_t getInterface() {
    // define wifi_interface_t
    wifi_interface_t result;
    switch (cfg.wifi_mode) {
      case WIFI_STA:
        result = (wifi_interface_t)ESP_IF_WIFI_STA;
        break;
      case WIFI_AP:
        result = (wifi_interface_t)ESP_IF_WIFI_AP;
        break;
      default:
        result = (wifi_interface_t)0;
        break;
    }
    return result;
  }

  /// Initialization
  bool setup() {
    esp_err_t result = esp_now_init();
    if (result == ESP_OK) {
      LOGI("esp_now_init: %s", macAddress());
    } else {
      LOGE("esp_now_init: %d", result);
    }

    // encryption is optional
    if (isEncrypted()) {
      esp_now_set_pmk((uint8_t *)cfg.primary_master_key);
    }

    if (cfg.recveive_cb != nullptr) {
      esp_now_register_recv_cb(cfg.recveive_cb);
    } else {
      esp_now_register_recv_cb(receive);
    }
    if (cfg.use_send_ack) {
      esp_now_register_send_cb(send);
    }
    available_to_write = cfg.buffer_size;
    is_init = result == ESP_OK;
    return is_init;
  }

  bool str2mac(const char *mac, uint8_t *values) {
    sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &values[0], &values[1],
           &values[2], &values[3], &values[4], &values[5]);
    return strlen(mac) == 17;
  }

  const char *mac2str(const uint8_t *array) {
    static char macStr[18];
    memset(macStr, 0, 18);
    snprintf(macStr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", array[0], array[1],
             array[2], array[3], array[4], array[5]);
    return (const char *)macStr;
  }

  static int bufferAvailableForWrite() {
    Lock lock(ESPNowStreamSelf->write_lock);
    return ESPNowStreamSelf->p_buffer->availableForWrite();
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  static void default_recv_cb(const esp_now_recv_info *info, const uint8_t *data, int data_len)
#else
  static void default_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len)
#endif
{
    LOGD("rec_cb: %d", data_len);
    // make sure that the receive buffer is available - moved from begin to make sure that it is only allocated when needed
    ESPNowStreamSelf->setupReceiveBuffer();
    // blocking write
    while (bufferAvailableForWrite() < data_len) {
      delay(2);
    }
    Lock lock(ESPNowStreamSelf->write_lock);
    size_t result = ESPNowStreamSelf->p_buffer->writeArray(data, data_len);
    if (result!=data_len){
      LOGE("writeArray %d -> %d", data_len, result);
    }
  }

  static void default_send_cb(const uint8_t *mac_addr,
                              esp_now_send_status_t status) {
    static uint8_t first_mac[ESP_NOW_KEY_LEN] = {0};
    // we use the first confirming mac_addr for further confirmations and ignore
    // others
    if (first_mac[0] == 0) {
      strncpy((char *)first_mac, (char *)mac_addr, ESP_NOW_KEY_LEN);
    }
    LOGD("default_send_cb - %s -> %s", ESPNowStreamSelf->mac2str(mac_addr),
         status == ESP_NOW_SEND_SUCCESS ? "+" : "-");

    // ignore others
    if (strncmp((char *)mac_addr, (char *)first_mac, ESP_NOW_KEY_LEN) == 0) {
      ESPNowStreamSelf->available_to_write = ESPNowStreamSelf->cfg.buffer_size;
      if (status == ESP_NOW_SEND_SUCCESS) {
        ESPNowStreamSelf->is_write_ok = true;
      } else {
        ESPNowStreamSelf->is_write_ok = false;
      }
    }
  }
};

}
