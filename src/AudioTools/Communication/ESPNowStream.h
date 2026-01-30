#pragma once
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "AudioTools/Concurrency/RTOS.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/CoreAudio/BaseStream.h"

namespace audio_tools {

// forward declarations
class ESPNowStream;
static ESPNowStream* ESPNowStreamSelf = nullptr;
static const char* BROADCAST_MAC_STR = "FF:FF:FF:FF:FF:FF";
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/**
 * @brief Configuration for ESP-NOW protocolö.W
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct ESPNowStreamConfig {
  wifi_mode_t wifi_mode = WIFI_STA;
  const char* mac_address = nullptr;
  int channel = 0;
  const char* ssid = nullptr;
  const char* password = nullptr;
  bool use_send_ack = true;  // we wait for
  uint16_t delay_after_failed_write_ms = 2000;
  uint16_t buffer_size = ESP_NOW_MAX_DATA_LEN;
  uint16_t buffer_count = 400;
  int write_retry_count = -1;  // -1 endless
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  void (*recveive_cb)(const esp_now_recv_info* info, const uint8_t* data,
                      int data_len) = nullptr;
#else
  void (*recveive_cb)(const uint8_t* mac_addr, const uint8_t* data,
                      int data_len) = nullptr;
#endif
  /// to encrypt set primary_master_key and local_master_key to 16 byte strings
  const char* primary_master_key = nullptr;
  const char* local_master_key = nullptr;
  /// esp-now bit rate
  wifi_phy_rate_t rate = WIFI_PHY_RATE_2M_S;
  /// threshold in percent to start reading from buffer
  uint8_t start_read_threshold_percent = 0;
};

/**
 * @brief ESPNow as Arduino Stream. When use_send_ack is true we prevent any
 * buffer overflows by blocking writes until the previous packet has been
 * confirmed. When no acknowledgments are used, you might need to throttle the
 * send speed to prevent any buffer overflow on the receiver side.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ESPNowStream : public BaseStream {
 public:
  ESPNowStream() { ESPNowStreamSelf = this; };

  ~ESPNowStream() {
    if (xSemaphore != nullptr) vSemaphoreDelete(xSemaphore);
  }

  ESPNowStreamConfig defaultConfig() {
    ESPNowStreamConfig result;
    return result;
  }

  /// Returns the mac address of the current ESP32
  const char* macAddress() {
    static String mac_str = WiFi.macAddress();
    return mac_str.c_str();
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
      delay(500);  // On some boards calling macAddress to early leads to a race
                   // condition.
      // checking if address has been updated
      const char* addr = macAddress();
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
    if (esp_wifi_config_espnow_rate(getInterface(), cfg.rate) != ESP_OK) {
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
    if (is_init) {
      if (esp_now_deinit() != ESP_OK) {
        LOGE("esp_now_deinit");
      }
      if (buffer.size() > 0) buffer.resize(0);
      is_init = false;
      has_peers = false;
      read_ready = false;
      is_broadcast = false;
    }
  }

  /// Adds a peer to which we can send info or from which we can receive info
  bool addPeer(esp_now_peer_info_t& peer) {
    if (!is_init) {
      LOGE("addPeer before begin");
      return false;
    }
    if (memcmp(BROADCAST_MAC,peer.peer_addr, 6)==0){
      LOGI("Using broadcast");
      is_broadcast = true;
    }
    esp_err_t result = esp_now_add_peer(&peer);
    if (result == ESP_OK) {
      LOGI("addPeer: %s", mac2str(peer.peer_addr));
      has_peers = true;
    } else {
      LOGE("addPeer: %d", result);
    }
    return result == ESP_OK;
  }

  /// Adds an array of
  template <size_t size>
  bool addPeers(const char* (&array)[size]) {
    bool result = true;
    for (int j = 0; j < size; j++) {
      const char* peer = array[j];
      if (peer != nullptr) {
        if (!addPeer(peer)) {
          result = false;
        }
      }
    }
    return result;
  }

  /// Adds a peer to which we can send info or from which we can receive info
  bool addPeer(const char* address) {
    esp_now_peer_info_t peer;
    peer.channel = cfg.channel;
    
    peer.ifidx = getInterface();
    peer.encrypt = false;

    if (StrView(address).equals(cfg.mac_address)) {
      LOGW("Did not add own address as peer");
      return true;
    }

    if (isEncrypted()) {
      peer.encrypt = true;
      strncpy((char*)peer.lmk, cfg.local_master_key, 16);
    }

    if (!str2mac(address, peer.peer_addr)) {
      LOGE("addPeer - Invalid address: %s", address);
      return false;
    }
    return addPeer(peer);
  }

  /// Adds the broadcast peer (FF:FF:FF:FF:FF:FF) to send to all devices in
  /// range. Note: Broadcast does not support acknowledgments
  bool addBroadcastPeer() {
    if (cfg.use_send_ack) {
      LOGW("Broadcast peer does not support use_send_ack");
      cfg.use_send_ack = false;
    }
    return addPeer(BROADCAST_MAC_STR);
  }

  /// Writes the data - sends it to all registered peers
  size_t write(const uint8_t* data, size_t len) override {
    // nullptr means send to all registered peers
    return write((const uint8_t*)nullptr, data, len);
  }

  /// Writes the data - sends it to all the indicated peer mac address string
  size_t write(const char* peer, const uint8_t* data, size_t len) {
    uint8_t mac[6];
    if (!str2mac(peer, mac)) {
      LOGE("write: invalid mac address %s", peer);
      return 0;
    }
    return write(mac, data, len);
  }

  /// Writes the data - sends it to all the peers
  size_t write(const uint8_t* peer, const uint8_t* data, size_t len) {
    // initialization: setup semaphore
    setupSemaphore();
    
    // initialization: if no peers registered and peer is nullptr, add broadcast
    if (!has_peers && peer == nullptr) {
      addBroadcastPeer();
    }
    
    int open = len;
    size_t result = 0;
    int retry_count = 0;

    while (open > 0) {
      size_t send_len = min(open, ESP_NOW_MAX_DATA_LEN);
      bool success = sendPacket(data + result, send_len, retry_count, peer);

      if (success) {
        // Move to next chunk
        open -= send_len;
        result += send_len;
        retry_count = 0;
      } else {
        // Max retries exceeded
        return result;
      }
    }
    return result;
  }

  /// Reeds the data from the peers
  size_t readBytes(uint8_t* data, size_t len) override {
    if (!read_ready) return 0;
    if (buffer.size() == 0) return 0;
    return buffer.readArray(data, len);
  }

  int available() override {
    if (!buffer) return 0;
    if (!read_ready) return 0;
    return buffer.size() == 0 ? 0 : buffer.available();
  }

  int availableForWrite() override {
    if (!buffer) return 0;
    return cfg.use_send_ack ? available_to_write : cfg.buffer_size;
  }

  /// provides how much the receive buffer is filled (in percent)
  float getBufferPercent() {
    int size = buffer.size();
    // prevent div by 0
    if (size == 0) return 0.0;
    // calculate percent
    return 100.0 * buffer.available() / size;
  }

  /// provides access to the receive buffer
  BufferRTOS<uint8_t>& getBuffer() { return buffer; }

 protected:
  ESPNowStreamConfig cfg;
  BufferRTOS<uint8_t> buffer{0};
  esp_now_recv_cb_t receive = default_recv_cb;
  esp_now_send_cb_t send = default_send_cb;
  volatile size_t available_to_write = 0;
  volatile bool last_send_success = true;
  bool is_init = false;
  SemaphoreHandle_t xSemaphore = nullptr;
  bool has_peers = false;
  bool read_ready = false;
  bool is_broadcast = false;

  inline void setupSemaphore() {
    // use semaphore for confirmations
    if (cfg.use_send_ack && xSemaphore == nullptr) {
      xSemaphore = xSemaphoreCreateBinary();
      xSemaphoreGive(xSemaphore);
    }
  }

  inline void setupReceiveBuffer() {
    // setup receive buffer
    if (!buffer) {
      LOGI("setupReceiveBuffer: %d", cfg.buffer_size * cfg.buffer_count);
      buffer.resize(cfg.buffer_size * cfg.buffer_count);
    }
  }

  inline void resetAvailableToWrite() {
    if (cfg.use_send_ack) {
      available_to_write = 0;
    }
  }

  /// Sends a single packet with retry logic
  bool sendPacket(const uint8_t* data, size_t len, int& retry_count,
                  const uint8_t* destination = nullptr) {
    // ESP-NOW requires explicit destination MAC when peers are registered
    // nullptr only works if NO peers are registered at all
    const uint8_t* target = destination;
    
    // If destination is nullptr and we have peers, use broadcast MAC
    if (target == nullptr && is_broadcast) {
      target = BROADCAST_MAC;
    }
    
    while (true) {
      resetAvailableToWrite();

      // Wait for confirmation from previous send
      if (cfg.use_send_ack) {
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
      }

      // Try to queue the packet
      esp_err_t rc = esp_now_send(target, data, len);

      if (rc == ESP_OK) {
        // Packet queued successfully, now wait for transmission confirmation
        if (handleTransmissionResult(retry_count)) {
          return true;  // Success
        }
        // Transmission failed, retry will happen in next loop iteration
      } else {
        // Failed to queue packet
        if (!handleQueueError(rc, retry_count)) {
          return false;  // Max retries exceeded
        }
      }
    }
  }

  /// Handles the result of packet transmission (after queuing)
  bool handleTransmissionResult(int& retry_count) {
    if (cfg.use_send_ack) {
      // Wait for callback to report transmission status
      xSemaphoreTake(xSemaphore, portMAX_DELAY);

      if (last_send_success) {
        // Success! Give back semaphore for next iteration
        xSemaphoreGive(xSemaphore);
        return true;
      } else {
        // Transmission failed - give back semaphore and retry
        xSemaphoreGive(xSemaphore);
        retry_count++;
        LOGI("Transmission failed - retrying same packet (attempt %d)",
             retry_count);

        if (cfg.write_retry_count >= 0 && retry_count > cfg.write_retry_count) {
          LOGE("Transmission error after %d retries - data lost!", retry_count);
          return false;
        }
        delay(cfg.delay_after_failed_write_ms);
        return false;  // Signal to retry
      }
    } else {
      // No ACK mode - assume success
      return true;
    }
  }

  /// Handles errors when queuing packets
  bool handleQueueError(esp_err_t rc, int& retry_count) {
    // esp_now_send failed to queue - callback will NOT be called
    // Give back the semaphore we took earlier
    if (cfg.use_send_ack) {
      xSemaphoreGive(xSemaphore);
    }

    retry_count++;
    LOGW("esp_now_send failed to queue (rc=%d/0x%04X) - retrying (attempt %d)", 
         rc, rc, retry_count);
    

    if (cfg.write_retry_count >= 0 && retry_count > cfg.write_retry_count) {
      LOGE("Send queue error after %d retries", retry_count);
      return false;
    }

    delay(cfg.delay_after_failed_write_ms);
    return true;  // Continue retrying
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
      esp_now_set_pmk((uint8_t*)cfg.primary_master_key);
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

  bool str2mac(const char* mac, uint8_t* values) {
    sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &values[0], &values[1],
           &values[2], &values[3], &values[4], &values[5]);
    return strlen(mac) == 17;
  }

  const char* mac2str(const uint8_t* array) {
    static char macStr[18];
    memset(macStr, 0, 18);
    snprintf(macStr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", array[0], array[1],
             array[2], array[3], array[4], array[5]);
    return (const char*)macStr;
  }

  static int bufferAvailableForWrite() {
    return ESPNowStreamSelf->buffer.availableForWrite();
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  static void default_recv_cb(const esp_now_recv_info* info,
                              const uint8_t* data, int data_len)
#else
  static void default_recv_cb(const uint8_t* mac_addr, const uint8_t* data,
                              int data_len)
#endif
  {
    LOGD("rec_cb: %d", data_len);
    // make sure that the receive buffer is available - moved from begin to make
    // sure that it is only allocated when needed
    ESPNowStreamSelf->setupReceiveBuffer();
    // blocking write
    size_t result = ESPNowStreamSelf->buffer.writeArray(data, data_len);
    if (result != data_len) {
      LOGE("writeArray %d -> %d", data_len, result);
    }
    // manage ready state
    if (ESPNowStreamSelf->read_ready == false) {
      if (ESPNowStreamSelf->cfg.start_read_threshold_percent == 0) {
        ESPNowStreamSelf->read_ready = true;
      } else {
        float percent = ESPNowStreamSelf->getBufferPercent();
        ESPNowStreamSelf->read_ready =
            percent >= ESPNowStreamSelf->cfg.start_read_threshold_percent;
      }
    }
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

  static void default_send_cb(const wifi_tx_info_t* tx_info,
                              esp_now_send_status_t status) {
    const uint8_t* mac_addr = tx_info->des_addr;
#else
  static void default_send_cb(const uint8_t* mac_addr,
                              esp_now_send_status_t status) {
#endif
    static uint8_t first_mac[ESP_NOW_KEY_LEN] = {0};
    // we use the first confirming mac_addr for further confirmations and
    // ignore others
    if (first_mac[0] == 0) {
      strncpy((char*)first_mac, (char*)mac_addr, ESP_NOW_KEY_LEN);
    }
    LOGD("default_send_cb - %s -> %s", ESPNowStreamSelf->mac2str(mac_addr),
         status == ESP_NOW_SEND_SUCCESS ? "+" : "-");

    // ignore others
    if (strncmp((char*)mac_addr, (char*)first_mac, ESP_NOW_KEY_LEN) == 0) {
      ESPNowStreamSelf->available_to_write = ESPNowStreamSelf->cfg.buffer_size;

      // Track send success/failure
      ESPNowStreamSelf->last_send_success = (status == ESP_NOW_SEND_SUCCESS);

      if (status != ESP_NOW_SEND_SUCCESS) {
        LOGI(
            "Send Error to %s! Status: %d (Possible causes: out of range, "
            "receiver busy/offline, channel mismatch, or buffer full)",
            ESPNowStreamSelf->mac2str(mac_addr), status);
      }

      // Release semaphore to allow write to check status and retry if needed
      xSemaphoreGive(ESPNowStreamSelf->xSemaphore);
    }
  }
};

}  // namespace audio_tools
