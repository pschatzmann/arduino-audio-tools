#pragma once

#include "esp_err.h"
#include "esp_netif.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"


#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/Concurrency/RTOS.h"

namespace audio_tools
{

  // clang-format off
#define DEFAULT_ESPNOW_RATE_CONFIG { \
  .phymode = WIFI_PHY_MODE_11G,      \
  .rate = WIFI_PHY_RATE_1M_L,        \
  .ersu = false,                     \
  .dcm = false                       \
}
  // clang-format on

  using ESPNowAddress = std::array<uint8_t, ESP_NOW_ETH_ALEN>;

  // forward declarations
  class ESPNowStream;
  static ESPNowStream *ESPNowStreamSelf = nullptr;
  static ESPNowAddress BROADCAST_ADDR = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  /**
   * @brief Configuration for ESP-NOW protocolö.W
   * @author Phil Schatzmann
   * @copyright GPLv3
   */
  struct ESPNowStreamConfig
  {
    wifi_mode_t wifi_mode = WIFI_MODE_STA;
    ESPNowAddress mac_address;
    int channel = 6;
    const char *ssid = nullptr;
    const char *password = nullptr;
    bool use_send_ack = true; // we wait for
    bool use_broadcast = false;
    bool use_long_range = false;
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
    wifi_phy_rate_t rate = WIFI_PHY_RATE_1M_L;
  };

  /**
   * @brief ESPNow as Arduino Stream.
   * @ingroup communications
   * @author Phil Schatzmann
   * @copyright GPLv3
   */
  class ESPNowStream : public BaseStream
  {
  public:
    ESPNowStream() { ESPNowStreamSelf = this; };

    ~ESPNowStream()
    {
      if (xSemaphore != nullptr)
        vSemaphoreDelete(xSemaphore);
    }

    ESPNowStreamConfig defaultConfig()
    {
      ESPNowStreamConfig result;
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
    bool begin(ESPNowStreamConfig cfg)
    {
      esp_err_t err = ESP_OK;
      this->cfg = cfg;
      LOGD("esp_netif_init");
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());
      LOGD("esp_event_loop_create_default");
      wifi_init_config_t cfg_wifi = WIFI_INIT_CONFIG_DEFAULT();
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_init(&cfg_wifi));
      LOGD("esp_wifi_set_storage");
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_RAM));
      LOGD("esp_wifi_set_mode");
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(cfg.wifi_mode));
      if (cfg.mac_address[0] != 0)
      {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mac_address(cfg.mac_address));
      }

      LOGD("esp_wifi_start");
      ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
      if (cfg.channel != 0)
      {
        LOGD("esp_wifi_set_channel");
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_channel(cfg.channel, WIFI_SECOND_CHAN_NONE));
      }
      if (cfg.use_long_range)
      {
        ESP_ERROR_CHECK(esp_wifi_set_protocol(getInterface(), WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
      }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
      LOGI("Setting ESP-NEW rate");
      if (esp_wifi_config_espnow_rate(getInterface(), cfg.rate) != ESP_OK)
      {
        LOGW("Could not set rate");
      }
#endif

      err = esp_now_init();
      if (err != ESP_OK)
      {
        LOGE("esp_now_init: (%d) %s", err, esp_err_to_name(err));
        return false;
      }

      // encryption is optional
      if (isEncrypted())
      {
        esp_now_set_pmk((uint8_t *)cfg.primary_master_key);
      }

      if (cfg.recveive_cb != nullptr)
      {
        esp_now_register_recv_cb(cfg.recveive_cb);
      }
      else
      {
        esp_now_register_recv_cb(receive);
      }

      if (cfg.use_send_ack)
      {
        esp_now_register_send_cb(send);
      }
      available_to_write = cfg.buffer_size;

      is_init = true;

      if (cfg.use_broadcast)
      {
        addPeer(BROADCAST_ADDR.data());
      }

      uint8_t addr[6];
      esp_wifi_get_mac(getInterface(), (uint8_t *) &addr);
      LOGI("Current Mac address: " MACSTR, MAC2STR(addr));

      return true;
    }

    /// DeInitialization
    void end()
    {
      if (is_init)
      {
        if (esp_now_deinit() != ESP_OK)
        {
          LOGE("esp_now_deinit");
        }
        if (buffer.size() > 0)
          buffer.resize(0);
        is_init = false;
      }
    }

    esp_err_t esp_wifi_set_mac_address(ESPNowAddress mac)
    {

      LOGI("Setting main mac to " MACSTR, MAC2STR(mac.data()));
      esp_err_t err = esp_wifi_set_mac(getInterface(), mac.data());
      if (err != ESP_OK)
      {
        return err;
      }
      delay(500); // On some boards calling macAddress to early leads to a race
                  // condition.
      // checking if address has been updated
      uint8_t addr[6];
      esp_wifi_get_mac(getInterface(), (uint8_t *) &addr);
      if (memcmp(addr, mac.data(), 6) != 0)
      {
        LOGE("Wrong mac address: " MACSTR, MAC2STR(addr));
        return !ESP_OK;
      }
      return ESP_OK;
    }

    /// Adds an array of
    template <size_t size>
    bool addPeers(ESPNowAddress (&array)[size])
    {
      bool result = true;
      for (int j = 0; j < size; j++)
      {
        ESPNowAddress peer = array[j];
        if (!peer.empty())
        {
          if (!addPeer(peer.data()))
          {
            result = false;
          }
        }
      }
      return result;
    }

    /// Adds a peer to which we can send info or from which we can receive info
    bool addPeer(const uint8_t *address)
    {
      esp_now_peer_info_t peer;
      peer.channel = cfg.channel;
      peer.ifidx = getInterface();
      peer.encrypt = false;

      if (memcmp(address, cfg.mac_address.data(), 6) == 0)
      {
        LOGW("Do not add own address as peer");
        return true;
      }

      if (isEncrypted())
      {
        peer.encrypt = true;
        strncpy((char *)peer.lmk, cfg.local_master_key, 16);
      }

      memcpy(peer.peer_addr, address, 6);
      return addPeer(peer);
    }

    /// Adds a peer to which we can send info or from which we can receive info
    bool addPeer(esp_now_peer_info_t &peer)
    {
      if (!is_init)
      {
        LOGE("addPeer before begin");
        return false;
      }
      esp_err_t err = esp_now_add_peer(&peer);
      if (err == ESP_OK)
      {
        LOGI("addPeer: " MACSTR, MAC2STR(peer.peer_addr));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        esp_now_rate_config_t rate_config = DEFAULT_ESPNOW_RATE_CONFIG;
        if (rate_config.rate != cfg.rate)
        {
          rate_config.rate = cfg.rate;
          LOGI("Setting PHY rate");
          esp_err_t rate_err = esp_now_set_peer_rate_config(peer.peer_addr, &rate_config);
          if (rate_err != ESP_OK)
          {
            log_w("Could not set the ESP-NOW PHY rate (%d) %s.", err, esp_err_to_name(err));
          }
        }
#endif
      }
      else
      {
        LOGE("addPeer: %d: %s.", err, esp_err_to_name(err));
      }
      return err == ESP_OK;
    }

    /// Writes the data - sends it to all the peers
    size_t write(const uint8_t *data, size_t len) override
    {
      setupSemaphore();
      int open = len;
      size_t result = 0;
      int retry_count = cfg.write_retry_count;
      while (open > 0)
      {
        resetAvailableToWrite();
        // wait for confirmation
        if (cfg.use_send_ack)
        {
          xSemaphoreTake(xSemaphore, portMAX_DELAY);
        }
        size_t send_len = min(open, ESP_NOW_MAX_DATA_LEN);
        esp_err_t err = 0;
        if (cfg.use_broadcast)
        {
          err = esp_now_send(BROADCAST_ADDR.data(), data + result, send_len);
        }
        else
        {
          err = esp_now_send(nullptr, data + result, send_len);
        } // check status
        if (err == ESP_OK)
        {
          open -= send_len;
          result += send_len;
          retry_count = 0;
        }
        else if (cfg.write_retry_count == 0)
        {
          LOGW("Write failed with %d: %s.", err, esp_err_to_name(err));
        }
        else
        {
          LOGW("Write failed with %d: %s - retrying again", err, esp_err_to_name(err));
          retry_count++;
          if (retry_count-- < 0)
          {
            LOGE("Write error after %d retries", cfg.write_retry_count);
            // break loop
            xSemaphoreGive(xSemaphore);
            return 0;
          }
          delay(cfg.delay_after_failed_write_ms);
        }
      }
      return result;
    }

    /// Reeds the data from the peers
    size_t readBytes(uint8_t *data, size_t len) override
    {
      if (buffer.size() == 0)
        return 0;
      return buffer.readArray(data, len);
    }

    int available() override
    {
      if (!buffer)
        return 0;
      return buffer.size() == 0 ? 0 : buffer.available();
    }

    int availableForWrite() override
    {
      if (!buffer)
        return 0;
      return cfg.use_send_ack ? available_to_write : cfg.buffer_size;
    }

    /// provides how much the receive buffer is filled (in percent)
    float getBufferPercent()
    {
      int size = buffer.size();
      // prevent div by 0
      if (size == 0)
        return 0.0;
      // calculate percent
      return 100.0 * buffer.available() / size;
    }

  protected:
    ESPNowStreamConfig cfg;
    BufferRTOS<uint8_t> buffer{0};
    esp_now_recv_cb_t receive = default_recv_cb;
    esp_now_send_cb_t send = default_send_cb;
    volatile size_t available_to_write = 0;
    bool is_init = false;
    SemaphoreHandle_t xSemaphore = nullptr;

    inline void setupSemaphore()
    {
      // use semaphore for confirmations
      if (cfg.use_send_ack && xSemaphore == nullptr)
      {
        xSemaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(xSemaphore);
      }
    }

    inline void setupReceiveBuffer()
    {
      // setup receive buffer
      if (!buffer)
      {
        LOGI("setupReceiveBuffer: %d", cfg.buffer_size * cfg.buffer_count);
        buffer.resize(cfg.buffer_size * cfg.buffer_count);
      }
    }

    inline void resetAvailableToWrite()
    {
      if (cfg.use_send_ack)
      {
        available_to_write = 0;
      }
    }

    bool isEncrypted()
    {
      return cfg.primary_master_key != nullptr && cfg.local_master_key != nullptr;
    }

    wifi_interface_t getInterface()
    {
      // define wifi_interface_t
      wifi_interface_t result;
      switch (cfg.wifi_mode)
      {
      case WIFI_MODE_STA:
        result = (wifi_interface_t)ESP_IF_WIFI_STA;
        break;
      case WIFI_MODE_AP:
        result = (wifi_interface_t)ESP_IF_WIFI_AP;
        break;
      default:
        result = (wifi_interface_t)0;
        break;
      }
      return result;
    }

    static int bufferAvailableForWrite()
    {
      return ESPNowStreamSelf->buffer.availableForWrite();
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void default_recv_cb(const esp_now_recv_info *info,
                                const uint8_t *data, int data_len)
#else
    static void default_recv_cb(const uint8_t *mac_addr, const uint8_t *data,
                                int data_len)
#endif
    {
      LOGD("rec_cb: %d", data_len);
      // make sure that the receive buffer is available - moved from begin to make
      // sure that it is only allocated when needed
      ESPNowStreamSelf->setupReceiveBuffer();
      // blocking write
      size_t result = ESPNowStreamSelf->buffer.writeArray(data, data_len);
      if (result != data_len)
      {
        LOGE("writeArray %d -> %d", data_len, result);
      }
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

    static void default_send_cb(const wifi_tx_info_t *tx_info,
                                esp_now_send_status_t status)
    {
      const uint8_t *mac_addr = tx_info->des_addr;
#else
    static void default_send_cb(const uint8_t *mac_addr,
                                esp_now_send_status_t status)
    {
      const uint8_t *mac_addr = tx_info->src_addr;

#endif
      static uint8_t first_mac[ESP_NOW_KEY_LEN] = {0};
      // we use the first confirming mac_addr for further confirmations and ignore
      // others
      if (first_mac[0] == 0)
      {
        strncpy((char *)first_mac, (char *)mac_addr, ESP_NOW_KEY_LEN);
      }
      LOGD("default_send_cb - " MACSTR " -> %s", MAC2STR(mac_addr),
           status == ESP_NOW_SEND_SUCCESS ? "+" : "-");

      // ignore others
      if (strncmp((char *)mac_addr, (char *)first_mac, ESP_NOW_KEY_LEN) == 0)
      {
        ESPNowStreamSelf->available_to_write = ESPNowStreamSelf->cfg.buffer_size;
        xSemaphoreGive(ESPNowStreamSelf->xSemaphore);
        if (status != ESP_NOW_SEND_SUCCESS)
        {
          LOGE("Send Error!");
        }
      }
    }
  };

} // namespace audio_tools
