#pragma once
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "AudioTools/Concurrency/RTOS.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/CoreAudio/BaseStream.h"

// propose max data length based on esp-idf version
#ifdef ESP_NOW_MAX_DATA_LEN_V2
// 1470
#define MY_ESP_NOW_MAX_LEN ESP_NOW_MAX_DATA_LEN_V2
#else
// 240
#define MY_ESP_NOW_MAX_LEN ESP_NOW_MAX_DATA_LEN
#endif

// calculate buffer count: 96000 bytes should be enough for most use cases and
// should not cause memory issues on the receiver side. With 240 bytes per
// packet, this results in 400 packets.
#define MY_ESP_NOW_BUFFER_SIZE (240 * 400)
#define MY_ESP_NOW_BUFFER_COUNT (MY_ESP_NOW_BUFFER_SIZE / MY_ESP_NOW_MAX_LEN)

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
  /// WiFi mode (station or access point). Default: WIFI_STA
  wifi_mode_t wifi_mode = WIFI_STA;
  /// MAC address to use for the ESP-NOW interface (nullptr for default).
  /// Default: nullptr
  const char* mac_address = nullptr;
  /// Size of each ESP-NOW packet buffer (bytes). Default: 1470 or 240 depending
  /// on esp-idf version
  uint16_t buffer_size = MY_ESP_NOW_MAX_LEN;
  /// Number of packet buffers allocated. Default: 65 or 400 depending on
  /// esp-idf version
  uint16_t buffer_count = MY_ESP_NOW_BUFFER_COUNT;
  /// WiFi channel to use (0 for auto). Default: 0
  int channel = 0;
  /// WiFi SSID for connection (optional). Default: nullptr
  const char* ssid = nullptr;
  /// WiFi password for connection (optional). Default: nullptr
  const char* password = nullptr;
  /// Use send acknowledgments to prevent buffer overflow. Default: true
  bool use_send_ack = true;  // we wait for
  /// Delay after failed write (ms). Default: 2000
  uint16_t delay_after_failed_write_ms = 2000;
  // enable long_range mode: increases range but reduces throughput. Default:
  // false
  bool use_long_range = false;
  /// Number of write retries (-1 for endless). Default: 1
  int write_retry_count = 1;  // -1 endless
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  /// Receive callback for ESP-NOW (esp-idf >= 5.0.0). Default: nullptr
  void (*recveive_cb)(const esp_now_recv_info* info, const uint8_t* data,
                      int data_len) = nullptr;
#else
  /// Receive callback for ESP-NOW (esp-idf < 5.0.0). Default: nullptr
  void (*recveive_cb)(const uint8_t* mac_addr, const uint8_t* data,
                      int data_len) = nullptr;
#endif
  /// Primary master key for encryption (16 bytes, optional). Default: nullptr
  const char* primary_master_key = nullptr;
  /// Local master key for encryption (16 bytes, optional). Default: nullptr
  const char* local_master_key = nullptr;
  /// ESP-NOW PHY mode. Default: WIFI_PHY_MODE_11G
  wifi_phy_mode_t phymode = WIFI_PHY_MODE_11G;
  /// ESP-NOW bit rate. Default: WIFI_PHY_RATE_6M
  wifi_phy_rate_t rate = WIFI_PHY_RATE_6M;
  /// Buffer fill threshold (percent) to start reading. Default: 0
  uint8_t start_read_threshold_percent = 0;
  /// Timeout for ACK semaphore (ms). Default: portMAX_DELAY
  uint32_t ack_semaphore_timeout_ms = portMAX_DELAY;
  /// Delay after updating mac
  uint16_t delay_after_updating_mac_ms = 500;
};

/**
 * @brief ESPNow as Arduino Stream. When use_send_ack is true we prevent any
 * buffer overflows by blocking writes until the previous packet has been
 * confirmed. When no acknowledgments are used, you might need to throttle the
 * send speed to prevent any buffer overflow on the receiver side.
 *
 * @note If multiple receivers are in range, only the first one which sends an
 * acknowledgment will be used as coordinator.
 *
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

    if (!setupMAC()) return false;

    if (!setupWiFi()) return false;

    WiFi.enableLongRange(cfg.use_long_range);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    LOGI("Setting ESP-NEW rate");
    if (esp_wifi_config_espnow_rate(getInterface(), cfg.rate) != ESP_OK) {
      LOGW("Could not set rate");
    }
#endif

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
    if (memcmp(BROADCAST_MAC, peer.peer_addr, 6) == 0) {
      LOGI("Using broadcast");
      is_broadcast = true;
    }
    esp_err_t result = esp_now_add_peer(&peer);
    if (result == ESP_OK) {
      LOGI("addPeer: %s", mac2str(peer.peer_addr));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
      esp_now_rate_config_t rate_config = {.phymode = cfg.phymode,
                                           .rate = cfg.rate,
                                           .ersu = false,
                                           .dcm = false};
      result = esp_now_set_peer_rate_config(peer.peer_addr, &rate_config);
      if (result != ESP_OK) {
        LOGW("Could not set the ESP-NOW PHY rate (%d) %s.", result,
             esp_err_to_name(result));
      }
#endif
      has_peers = result == ESP_OK;
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

    size_t total_sent = 0;
    size_t remaining = len;

    while (remaining > 0) {
      size_t chunk_size = min(remaining, (size_t)MY_ESP_NOW_MAX_LEN);
      int retry_count = 0;

      bool success =
          sendPacket(data + total_sent, chunk_size, retry_count, peer);

      if (success) {
        // Chunk sent successfully
        total_sent += chunk_size;
        remaining -= chunk_size;
      } else {
        // Max retries exceeded for this chunk
        LOGE(
            "write: failed to send chunk after %d attempts (sent %zu/%zu "
            "bytes)",
            retry_count, total_sent, len);
        // Return bytes successfully sent so far (may be 0 if first chunk
        // failed)
        return total_sent;
      }
    }
    return total_sent;
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

  /// time when we were able to send or receive the last packet successfully
  uint32_t getLastIoSuccessTime() const { return last_io_success_time; }

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
  uint32_t last_io_success_time = 0;

  bool setupMAC() {
    // set mac address
    if (cfg.mac_address != nullptr) {
      LOGI("setting mac %s", cfg.mac_address);
      byte mac[ESP_NOW_KEY_LEN];
      str2mac(cfg.mac_address, mac);
      if (esp_wifi_set_mac((wifi_interface_t)getInterface(), mac) != ESP_OK) {
        LOGE("Could not set mac address");
        return false;
      }

      // On some boards calling macAddress to early leads to a race condition.
      delay(cfg.delay_after_updating_mac_ms);

      // checking if address has been updated
      const char* addr = macAddress();
      if (strcmp(addr, cfg.mac_address) != 0) {
        LOGE("Wrong mac address: %s", addr);
        return false;
      }
    }
    return true;
  }

  bool setupWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
      // start only when not connected and we have ssid and password
      if (cfg.ssid != nullptr && cfg.password != nullptr) {
        LOGI("Logging into WiFi: %s", cfg.ssid);
        WiFi.begin(cfg.ssid, cfg.password);
        while (WiFi.status() != WL_CONNECTED) {
          Serial.print('.');
          delay(1000);
        }
      }
      Serial.println();
    }

    // in AP mode we neeed to be logged in!
    if (WiFi.getMode() == WIFI_AP && WiFi.status() != WL_CONNECTED) {
      LOGE("You did not start Wifi or did not provide ssid and password");
      return false;
    }

    return true;
  }

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
    TRACED();
    const uint8_t* target = destination;
    if (target == nullptr && is_broadcast) {
      target = BROADCAST_MAC;
    }

    while (true) {
      resetAvailableToWrite();

      // Wait for previous send to complete (if using ACKs)
      if (cfg.use_send_ack) {
        TickType_t ticks = (cfg.ack_semaphore_timeout_ms == portMAX_DELAY)
                               ? portMAX_DELAY
                               : pdMS_TO_TICKS(cfg.ack_semaphore_timeout_ms);
        if (xSemaphoreTake(xSemaphore, ticks) != pdTRUE) {
          // Timeout waiting for previous send - check retry limit BEFORE
          // incrementing
          if (cfg.write_retry_count >= 0 &&
              retry_count >= cfg.write_retry_count) {
            LOGE("Timeout waiting for ACK semaphore after %d retries",
                 retry_count);
            return false;
          }
          retry_count++;
          LOGW("ACK semaphore timeout (attempt %d)", retry_count);
          delay(cfg.delay_after_failed_write_ms);
          continue;
        }
      }

      // Try to queue the packet
      esp_err_t rc = esp_now_send(target, data, len);

      if (rc == ESP_OK) {
        // Packet queued - wait for transmission result
        if (handleTransmissionResult(retry_count)) {
          return true;  // Success
        }
        // Transmission failed - check if we've exceeded the limit
        // handleTransmissionResult returns false both when limit is reached
        // and when we should retry, so check the limit here
        if (cfg.write_retry_count >= 0 &&
            retry_count >= cfg.write_retry_count) {
          return false;  // Give up - limit reached
        }
        // Continue to retry
      } else {
        // Failed to queue - callback will NOT be called
        if (cfg.use_send_ack) {
          xSemaphoreGive(xSemaphore);  // Give back semaphore
        }

        // Check limit BEFORE incrementing
        if (cfg.write_retry_count >= 0 &&
            retry_count >= cfg.write_retry_count) {
          LOGE("esp_now_send queue error (rc=%d/0x%04X) after %d retries", rc,
               rc, retry_count);
          return false;
        }

        retry_count++;
        LOGW("esp_now_send failed (rc=%d/0x%04X) - retrying (attempt %d)", rc,
             rc, retry_count);
        delay(cfg.delay_after_failed_write_ms);
      }
    }
  }

  /// Handles the result of packet transmission (after queuing)
  bool handleTransmissionResult(int& retry_count) {
    TRACED();
    if (cfg.use_send_ack) {
      // Wait for callback to signal result
      TickType_t ticks = (cfg.ack_semaphore_timeout_ms == portMAX_DELAY)
                             ? portMAX_DELAY
                             : pdMS_TO_TICKS(cfg.ack_semaphore_timeout_ms);
      if (xSemaphoreTake(xSemaphore, ticks) != pdTRUE) {
        // Callback never came - check limit BEFORE incrementing
        if (cfg.write_retry_count >= 0 &&
            retry_count >= cfg.write_retry_count) {
          LOGE("Transmission callback timeout after %d retries", retry_count);
          return false;
        }
        retry_count++;
        LOGW("Transmission callback timeout (attempt %d)", retry_count);
        delay(cfg.delay_after_failed_write_ms);
        return false;  // Retry
      }

      // Got callback - check result
      if (last_send_success) {
        xSemaphoreGive(xSemaphore);  // Release for next send
        return true;
      } else {
        xSemaphoreGive(xSemaphore);  // Release for retry

        // Check limit BEFORE incrementing
        if (cfg.write_retry_count >= 0 &&
            retry_count >= cfg.write_retry_count) {
          LOGE("Transmission failed after %d retries", retry_count);
          return false;
        }

        retry_count++;
        LOGI("Transmission failed - retrying (attempt %d)", retry_count);
        delay(cfg.delay_after_failed_write_ms);
        return false;  // Retry
      }
    }
    return true;  // No ACK mode - assume success
  }

  /// Handles errors when queuing packets
  bool handleQueueError(esp_err_t rc, int& retry_count) {
    TRACED();
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
    // make sure that the receive buffer is available - moved from begin to
    // make sure that it is only allocated when needed
    ESPNowStreamSelf->setupReceiveBuffer();

    // update last io time
    ESPNowStreamSelf->last_io_success_time = millis();

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

      if (status == ESP_NOW_SEND_SUCCESS) {
        ESPNowStreamSelf->last_io_success_time = millis();
      } else {
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
