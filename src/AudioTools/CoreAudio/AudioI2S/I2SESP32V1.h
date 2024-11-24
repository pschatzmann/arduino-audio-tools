#pragma once

#include "AudioConfig.h"
#if defined(ESP32) && defined(USE_I2S) &&                  \
        ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) || \
    defined(DOXYGEN)

#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_system.h"

#define IS_I2S_IMPLEMENTED

namespace audio_tools {

/**
 * @brief Basic I2S API for the ESP32 (using the new API).
 * https://docs.espressif.com/projects/esp-idf/en/v5.0.1/esp32/api-reference/peripherals/i2s.html#i2s-communication-mode
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriverESP32V1 {
 public:
  /// Provides the default configuration
  I2SConfigESP32V1 defaultConfig(RxTxMode mode) {
    I2SConfigESP32V1 c(mode);
    return c;
  }
  /// Potentially updates the sample rate (if supported)
  bool setAudioInfo(AudioInfo info) {
    // nothing to do
    if (is_started) {
      if (info.equals(cfg)) return true;
      if (info.equalsExSampleRate(cfg)) {
        cfg.sample_rate = info.sample_rate;
        LOGI("i2s_set_sample_rates: %d", (int)info.sample_rate);
        return getDriver(cfg).changeSampleRate(cfg, rx_chan, tx_chan);
      }
    } else {
      LOGE("not started");
    }
    return false;
  }

  /// starts the DAC with the default config
  bool begin(RxTxMode mode) { return begin(defaultConfig(mode)); }

  /// starts the DAC with the current config - if not started yet. If I2S has
  /// been started there is no action and we return true
  bool begin() { return (!is_started) ? begin(cfg) : true; }

  /// starts the DAC
  bool begin(I2SConfigESP32V1 cfg) {
    TRACED();
    this->cfg = cfg;

    // stop if it is already open
    if (is_started) end();

    switch (cfg.rx_tx_mode) {
      case TX_MODE:
        return begin(cfg, cfg.pin_data, I2S_GPIO_UNUSED);
      case RX_MODE:
        // usually we expet cfg.pin_data but if the used assinged rx we might
        // consider this one
        return begin(cfg, I2S_GPIO_UNUSED,
                     cfg.pin_data_rx != I2S_GPIO_UNUSED ? cfg.pin_data_rx
                                                        : cfg.pin_data);
      default:
        return begin(cfg, cfg.pin_data, cfg.pin_data_rx);
    }
    LOGE("Did not expect go get here");
  }

  /// we assume the data is already available in the buffer
  int available() { return I2S_BUFFER_COUNT * I2S_BUFFER_SIZE; }

  /// We limit the write size to the buffer size
  int availableForWrite() { return I2S_BUFFER_COUNT * I2S_BUFFER_SIZE; }

  /// stops the I2C and unistalls the driver
  void end() {
    TRACED();
    if (rx_chan != nullptr) {
      i2s_channel_disable(rx_chan);
      i2s_del_channel(rx_chan);
      rx_chan = nullptr;
    }
    if (tx_chan != nullptr) {
      i2s_channel_disable(tx_chan);
      i2s_del_channel(tx_chan);
      tx_chan = nullptr;
    }

    is_started = false;
  }

  /// provides the actual configuration
  I2SConfigESP32V1 config() { return cfg; }

  /// writes the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    TRACED();
    size_t result;
    assert(tx_chan != nullptr);
    if (i2s_channel_write(tx_chan, src, size_bytes, &result,
                          ticks_to_wait_write) != ESP_OK) {
      TRACEE();
    }
    return result;
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    size_t result = 0;
    if (i2s_channel_read(rx_chan, dest, size_bytes, &result,
                         ticks_to_wait_read) != ESP_OK) {
      TRACEE();
    }
    return result;
  }

  void setWaitTimeReadMs(TickType_t ms) {
    ticks_to_wait_read = pdMS_TO_TICKS(ms);
  }
  void setWaitTimeWriteMs(TickType_t ms) {
    ticks_to_wait_write = pdMS_TO_TICKS(ms);
  }

 protected:
  I2SConfigESP32V1 cfg = defaultConfig(RXTX_MODE);
  i2s_std_config_t i2s_config;
  i2s_chan_handle_t tx_chan = nullptr;  // I2S tx channel handler
  i2s_chan_handle_t rx_chan = nullptr;  // I2S rx channel handler
  bool is_started = false;
  TickType_t ticks_to_wait_read = portMAX_DELAY;
  TickType_t ticks_to_wait_write = portMAX_DELAY;

  struct DriverCommon {
    virtual bool startChannels(I2SConfigESP32V1 &cfg,
                               i2s_chan_handle_t &tx_chan,
                               i2s_chan_handle_t &rx_chan, int txPin,
                               int rxPin) = 0;
  
    virtual i2s_chan_config_t getChannelConfig(I2SConfigESP32V1 &cfg) = 0;
    // changes the sample rate
    virtual bool changeSampleRate(I2SConfigESP32V1 &cfg,
                                  i2s_chan_handle_t &tx_chan,
                                  i2s_chan_handle_t &rx_chan) {
      return false;
    }

    protected:
    /// 24 bits are stored in a 32 bit integer
    int get_bits_eff(int bits) { return ( bits == 24 )? 32 : bits; }

  };

  struct DriverI2S : public DriverCommon {
    bool startChannels(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan,
                       i2s_chan_handle_t &rx_chan, int txPin, int rxPin) {
      TRACED();
      LOGI("tx: %d, rx: %d", txPin, rxPin);
      i2s_std_config_t std_cfg = {
          .clk_cfg = getClockConfig(cfg),
          .slot_cfg = getSlotConfig(cfg),
          .gpio_cfg =
              {
                  .mclk = (gpio_num_t)cfg.pin_mck,
                  .bclk = (gpio_num_t)cfg.pin_bck,
                  .ws = (gpio_num_t)cfg.pin_ws,
                  .dout = (gpio_num_t)txPin,
                  .din = (gpio_num_t)rxPin,
                  .invert_flags =
                      {
                          .mclk_inv = false,
                          .bclk_inv = false,
                          .ws_inv = false,
                      },
              },
      };

      if (cfg.rx_tx_mode == RXTX_MODE || cfg.rx_tx_mode == TX_MODE) {
        if (i2s_channel_init_std_mode(tx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_std_mode %s", "tx");
          return false;
        }
        if (i2s_channel_enable(tx_chan) != ESP_OK) {
          LOGE("i2s_channel_enable %s", "tx");
          return false;
        }
      }

      if (cfg.rx_tx_mode == RXTX_MODE || cfg.rx_tx_mode == RX_MODE) {
        if (i2s_channel_init_std_mode(rx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_std_mode %s", "rx");
          return false;
        }
        if (i2s_channel_enable(rx_chan) != ESP_OK) {
          LOGE("i2s_channel_enable %s", "rx");
          return false;
        }
      }

      LOGD("%s - %s", __func__, "started");
      return true;
    }

   protected:
    i2s_std_slot_config_t getSlotConfig(I2SConfigESP32V1 &cfg) {
      TRACED();
      i2s_std_slot_config_t result;
      switch (cfg.i2s_format) {
        case I2S_LEFT_JUSTIFIED_FORMAT:
        case I2S_MSB_FORMAT:
          result = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
              (i2s_data_bit_width_t)cfg.bits_per_sample,
              (i2s_slot_mode_t)cfg.channels);
          break;
        case I2S_PCM:
          result = I2S_STD_PCM_SLOT_DEFAULT_CONFIG(
              (i2s_data_bit_width_t)cfg.bits_per_sample,
              (i2s_slot_mode_t)cfg.channels);
          break;
        default:
          result = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
              (i2s_data_bit_width_t)cfg.bits_per_sample,
              (i2s_slot_mode_t)cfg.channels);
      }

      // Update slot_mask if only one channel
      if (cfg.channels == 1) {
        switch (cfg.channel_format) {
          case I2SChannelSelect::Left:
            result.slot_mask = I2S_STD_SLOT_LEFT;
            break;
          case I2SChannelSelect::Right:
            result.slot_mask = I2S_STD_SLOT_RIGHT;
            break;
          default:
            LOGW("Using channel_format: I2SChannelSelect::Left for mono");
            result.slot_mask = I2S_STD_SLOT_LEFT;
            break;
        }
      }

      return result;
    }

    i2s_chan_config_t getChannelConfig(I2SConfigESP32V1 &cfg) {
      TRACED();
      i2s_chan_config_t result = I2S_CHANNEL_DEFAULT_CONFIG(
          (i2s_port_t)cfg.port_no,
          cfg.is_master ? I2S_ROLE_MASTER : I2S_ROLE_SLAVE);
      // use the legicy size parameters for frame num
      int size = cfg.buffer_size * cfg.buffer_count;
      int frame_size = get_bits_eff(cfg.bits_per_sample) * cfg.channels / 8;
      if (size > 0) result.dma_frame_num = size / frame_size;
      LOGI("dma_frame_num: %d", (int)result.dma_frame_num);
      result.auto_clear = cfg.auto_clear;
      return result;
    }

    i2s_std_clk_config_t getClockConfig(I2SConfigESP32V1 &cfg) {
      TRACED();
      i2s_std_clk_config_t clk_cfg =
          I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)cfg.sample_rate);
      if (cfg.mclk_multiple > 0) {
        clk_cfg.mclk_multiple = (i2s_mclk_multiple_t)cfg.mclk_multiple;
      } else {
        if (cfg.bits_per_sample == 24) {
          // mclk_multiple' should be the multiple of 3 while using 24-bit
          clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
          LOGI("mclk_multiple=384");
        }
      }
      return clk_cfg;
    }

    bool changeSampleRate(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan,
                          i2s_chan_handle_t &rx_chan) override {
      bool rc = false;
      auto clock_cfg = getClockConfig(cfg);
      if (tx_chan != nullptr) {
        i2s_channel_disable(tx_chan);
        rc = i2s_channel_reconfig_std_clock(tx_chan, &clock_cfg) == ESP_OK;
        i2s_channel_enable(tx_chan);
      }
      if (rx_chan != nullptr) {
        i2s_channel_disable(rx_chan);
        rc = i2s_channel_reconfig_std_clock(rx_chan, &clock_cfg) == ESP_OK;
        i2s_channel_enable(rx_chan);
      }
      return rc;
    }

  } i2s;

#ifdef USE_PDM

  struct DriverPDM : public DriverCommon {
    bool startChannels(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan,
                       i2s_chan_handle_t &rx_chan, int txPin, int rxPin) {
      if (cfg.rx_tx_mode == TX_MODE) {
        return startTX(cfg, tx_chan, txPin);
      } else if (cfg.rx_tx_mode == RX_MODE) {
        return startRX(cfg, rx_chan, rxPin);
      }
      LOGE("Only RX and TX is supported for PDM")
      return false;
    }

   protected:
    i2s_pdm_tx_slot_config_t getTxSlotConfig(I2SConfigESP32V1 &cfg) {
      return I2S_PDM_TX_SLOT_DEFAULT_CONFIG(
          (i2s_data_bit_width_t)cfg.bits_per_sample,
          (i2s_slot_mode_t)cfg.channels);
    }

    i2s_chan_config_t getChannelConfig(I2SConfigESP32V1 &cfg) {
      return I2S_CHANNEL_DEFAULT_CONFIG(
          (i2s_port_t)cfg.port_no,
          cfg.is_master ? I2S_ROLE_MASTER : I2S_ROLE_SLAVE);
    }

    i2s_pdm_tx_clk_config_t getTxClockConfig(I2SConfigESP32V1 &cfg) {
      return I2S_PDM_TX_CLK_DEFAULT_CONFIG((uint32_t)cfg.sample_rate);
    }

    bool startTX(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan, int txPin) {
      i2s_pdm_tx_config_t pdm_tx_cfg = {
          .clk_cfg = getTxClockConfig(cfg),
          .slot_cfg = getTxSlotConfig(cfg),
          .gpio_cfg =
              {
                  .clk = (gpio_num_t)cfg.pin_bck,
                  .dout = (gpio_num_t)txPin,
                  .invert_flags =
                      {
                          .clk_inv = false,
                      },
              },
      };

      if (i2s_channel_init_pdm_tx_mode(tx_chan, &pdm_tx_cfg) != ESP_OK) {
        LOGE("i2s_channel_init_pdm_tx_mode %s", "tx");
        return false;
      }
      if (i2s_channel_enable(tx_chan) != ESP_OK) {
        LOGE("i2s_channel_enable %s", "tx");
        return false;
      }
      return true;
    }

#if defined(USE_PDM_RX)
    i2s_pdm_rx_slot_config_t getRxSlotConfig(I2SConfigESP32V1 &cfg) {
      return I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
          (i2s_data_bit_width_t)cfg.bits_per_sample,
          (i2s_slot_mode_t)cfg.channels);
    }
    i2s_pdm_rx_clk_config_t getRxClockConfig(I2SConfigESP32V1 &cfg) {
      return I2S_PDM_RX_CLK_DEFAULT_CONFIG((uint32_t)cfg.sample_rate);
    }
    bool startRX(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &rx_chan, int rxPin) {
      i2s_pdm_rx_config_t pdm_rx_cfg = {
          .clk_cfg = getRxClockConfig(cfg),
          .slot_cfg = getRxSlotConfig(cfg),
          .gpio_cfg =
              {
                  .clk = (gpio_num_t)cfg.pin_bck,
                  .din = (gpio_num_t)rxPin,
                  .invert_flags =
                      {
                          .clk_inv = false,
                      },
              },
      };

      if (i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_rx_cfg) != ESP_OK) {
        LOGE("i2s_channel_init_pdm_rx_mode %s", "rx");
        return false;
      }
      if (i2s_channel_enable(rx_chan) != ESP_OK) {
        LOGE("i2s_channel_enable %s", "tx");
        return false;
      }
      return true;
    }
#else
    bool startRX(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &rx_chan, int rxPin) {
      LOGE("PDM RX not supported");
      return false;
    }
#endif
  } pdm;

#endif

#ifdef USE_TDM
  // example at
  // https://github.com/espressif/esp-idf/blob/v5.3-dev/examples/peripherals/i2s/i2s_basic/i2s_tdm/main/i2s_tdm_example_main.c
  struct DriverTDM : public DriverCommon {
    bool startChannels(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan,
                       i2s_chan_handle_t &rx_chan, int txPin, int rxPin) {
      i2s_tdm_config_t tdm_cfg = {
          .clk_cfg = getClockConfig(cfg),
          .slot_cfg = getSlotConfig(cfg),
          .gpio_cfg =
              {
                  .mclk = (gpio_num_t)cfg.pin_mck,
                  .bclk = (gpio_num_t)cfg.pin_bck,
                  .ws = (gpio_num_t)cfg.pin_ws,
                  .dout = (gpio_num_t)txPin,
                  .din = (gpio_num_t)rxPin,
                  .invert_flags =
                      {
                          .mclk_inv = false,
                          .bclk_inv = false,
                          .ws_inv = false,
                      },
              },
      };

      if (cfg.rx_tx_mode == TX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
        if (i2s_channel_init_tdm_mode(tx_chan, &tdm_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_tdm_tx_mode %s", "tx");
          return false;
        }
      }
      if (cfg.rx_tx_mode == RX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
        if (i2s_channel_init_tdm_mode(rx_chan, &tdm_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_tdm_tx_mode %s", "rx");
          return false;
        }
      }
      return true;
    }

   protected:
    i2s_tdm_slot_config_t getSlotConfig(I2SConfigESP32V1 &cfg) {
      int slots = 0;
      for (int j = 0; j < cfg.channels; j++) {
        slots |= 1 << j;
      }
      // setup default format
      i2s_tdm_slot_config_t slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(
          (i2s_data_bit_width_t)cfg.bits_per_sample, I2S_SLOT_MODE_STEREO,
          (i2s_tdm_slot_mask_t)slots);

      switch (cfg.i2s_format) {
        case I2S_RIGHT_JUSTIFIED_FORMAT:
        case I2S_LSB_FORMAT:
        case I2S_PHILIPS_FORMAT:
        case I2S_STD_FORMAT:
          slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(
              (i2s_data_bit_width_t)cfg.bits_per_sample, I2S_SLOT_MODE_STEREO,
              (i2s_tdm_slot_mask_t)slots);
          break;
        case I2S_LEFT_JUSTIFIED_FORMAT:
        case I2S_MSB_FORMAT:
          slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(
              (i2s_data_bit_width_t)cfg.bits_per_sample, I2S_SLOT_MODE_STEREO,
              (i2s_tdm_slot_mask_t)slots);
          break;
        case I2S_PCM:
          slot_cfg = I2S_TDM_PCM_LONG_SLOT_DEFAULT_CONFIG(
              (i2s_data_bit_width_t)cfg.bits_per_sample, I2S_SLOT_MODE_STEREO,
              (i2s_tdm_slot_mask_t)slots);
          break;
        default:
          LOGE("TDM: Unsupported format");
      }

      return slot_cfg;
    }

    i2s_chan_config_t getChannelConfig(I2SConfigESP32V1 &cfg) {
      return I2S_CHANNEL_DEFAULT_CONFIG(
          (i2s_port_t)cfg.port_no,
          cfg.is_master ? I2S_ROLE_MASTER : I2S_ROLE_SLAVE);
    }

    i2s_tdm_clk_config_t getClockConfig(I2SConfigESP32V1 &cfg) {
      return I2S_TDM_CLK_DEFAULT_CONFIG((uint32_t)cfg.sample_rate);
    }

  } tdm;

#endif

  /// -> protected methods from I2SDriverESP32V1

  /// starts I2S
  bool begin(I2SConfigESP32V1 cfg, int txPin, int rxPin) {
    TRACED();
    cfg.logInfo();
    this->cfg = cfg;
    if (cfg.channels <= 0 || cfg.channels > 2) {
      LOGE("invalid channels: %d", cfg.channels);
      return false;
    }

    DriverCommon &driver = getDriver(cfg);
    if (!newChannels(cfg, driver)) {
      end();
      return false;
    }

    is_started = driver.startChannels(cfg, tx_chan, rx_chan, txPin, rxPin);
    if (!is_started) {
      end();
      LOGE("Channels not started");
    }
    return is_started;
  }

  bool newChannels(I2SConfigESP32V1 &cfg, DriverCommon &driver) {
    i2s_chan_config_t chan_cfg = driver.getChannelConfig(cfg);
    switch (cfg.rx_tx_mode) {
      case RX_MODE:
        if (i2s_new_channel(&chan_cfg, NULL, &rx_chan) != ESP_OK) {
          LOGE("i2s_channel");
          return false;
        }
        break;
      case TX_MODE:
        if (i2s_new_channel(&chan_cfg, &tx_chan, NULL) != ESP_OK) {
          LOGE("i2s_channel");
          return false;
        }
        break;
      default:
        if (i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan) != ESP_OK) {
          LOGE("i2s_channel");
          return false;
        }
    }
    return true;
  }

  DriverCommon &getDriver(I2SConfigESP32V1 &cfg) {
    switch (cfg.signal_type) {
      case Digital:
        return i2s;
#ifdef USE_PDM
      case Analog:
      case PDM:
        return pdm;
#endif
#ifdef USE_TDM
      case TDM:
        return tdm;
#endif
      default:
        break;
    }
    LOGE("Unsupported singal_type");
    return i2s;
  }
};

using I2SDriver = I2SDriverESP32V1;

}  // namespace audio_tools

#endif