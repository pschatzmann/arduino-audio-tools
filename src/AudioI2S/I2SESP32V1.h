#pragma once

#include "AudioConfig.h"
#if defined(ESP32) && defined(USE_I2S) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) || defined(DOXYGEN)

#include "AudioI2S/I2SConfig.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_system.h"

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

  /// starts the DAC with the default config
  bool begin(RxTxMode mode) { return begin(defaultConfig(mode)); }

  /// starts the DAC with the current config - if not started yet. If I2S has
  /// been started there is no action and we return true
  bool begin() { return (!is_started) ? begin(cfg) : true; }

  /// starts the DAC
  bool begin(I2SConfigESP32V1 cfg) {
    TRACED();
    this->cfg = cfg;
    switch (cfg.rx_tx_mode) {
    case TX_MODE:
      return begin(cfg, cfg.pin_data, I2S_GPIO_UNUSED);
    case RX_MODE:
      // usually we expet cfg.pin_data but if the used assinged rx we might
      // consider this one
      return begin(cfg, I2S_GPIO_UNUSED,
                   cfg.pin_data != I2S_GPIO_UNUSED ? cfg.pin_data
                                                     : cfg.pin_data_rx);
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
    if (cfg.rx_tx_mode == RXTX_MODE || cfg.rx_tx_mode == TX_MODE) {
      i2s_channel_disable(rx_chan);
      i2s_del_channel(rx_chan);
    }
    if (cfg.rx_tx_mode == RXTX_MODE || cfg.rx_tx_mode == TX_MODE) {
      i2s_channel_disable(tx_chan);
      i2s_del_channel(tx_chan);
    }

    is_started = false;
  }

  /// provides the actual configuration
  I2SConfigESP32V1 config() { return cfg; }

  /// writes the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    TRACED();
    size_t result;
    if (i2s_channel_write(tx_chan, src, size_bytes, &result, portMAX_DELAY) !=
        ESP_OK) {
      TRACEE();
    }
    return result;
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    size_t result = 0;
    if (i2s_channel_read(rx_chan, dest, size_bytes, &result, portMAX_DELAY) !=
        ESP_OK) {
      TRACEE();
    }
    return result;
  }

protected:
  I2SConfigESP32V1 cfg = defaultConfig(RX_MODE);
  i2s_std_config_t i2s_config;
  i2s_chan_handle_t tx_chan; // I2S tx channel handler
  i2s_chan_handle_t rx_chan; // I2S rx channel handler
  bool is_started = false;

  struct DriverCommon {
    virtual i2s_chan_config_t getChannelConfig(I2SConfigESP32V1 &cfg) = 0;
    virtual bool startChannels(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan,
                              i2s_chan_handle_t &rx_chan, int txPin, int rxPin) = 0;
  };

  struct DriverI2S : public DriverCommon {
    i2s_std_slot_config_t getSlotConfig(I2SConfigESP32V1 &cfg) {
      TRACED();
      switch (cfg.i2s_format) {
      case I2S_RIGHT_JUSTIFIED_FORMAT:
      case I2S_LSB_FORMAT:
      case I2S_PHILIPS_FORMAT:
      case I2S_STD_FORMAT:
        return I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            (i2s_data_bit_width_t)cfg.bits_per_sample,
            (i2s_slot_mode_t)cfg.channels);
      case I2S_LEFT_JUSTIFIED_FORMAT:
      case I2S_MSB_FORMAT:
        return I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            (i2s_data_bit_width_t)cfg.bits_per_sample,
            (i2s_slot_mode_t)cfg.channels);
      case I2S_PCM:
        return I2S_STD_PCM_SLOT_DEFAULT_CONFIG(
            (i2s_data_bit_width_t)cfg.bits_per_sample,
            (i2s_slot_mode_t)cfg.channels);
      }
      // use default config
      TRACEE();
      return I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
          (i2s_data_bit_width_t)cfg.bits_per_sample,
          (i2s_slot_mode_t)cfg.channels);
    }

    i2s_chan_config_t getChannelConfig(I2SConfigESP32V1 &cfg) {
      TRACED();
      return I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)cfg.port_no,
                                        cfg.is_master ? I2S_ROLE_MASTER
                                                      : I2S_ROLE_SLAVE);
    }

    i2s_std_clk_config_t getClockConfig(I2SConfigESP32V1 &cfg) {
      TRACED();
      return I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)cfg.sample_rate);
    }

    bool startChannels(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan,
                      i2s_chan_handle_t &rx_chan, int txPin, int rxPin) {
      TRACED();
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
        if (i2s_channel_enable(tx_chan)!=ESP_OK){
          LOGE("i2s_channel_enable %s", "tx");
          return false;
        }
      }

      if (cfg.rx_tx_mode == RXTX_MODE || cfg.rx_tx_mode == RX_MODE) {
        if (i2s_channel_init_std_mode(rx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_std_mode %s", "rx");
          return false;
        }
        if (i2s_channel_enable(rx_chan)!=ESP_OK){
          LOGE("i2s_channel_enable %s", "tx");
          return false;
        }
      }

      LOGD("%s - %s", __func__, "started");
      return true;
    }
  } i2s;

#ifdef USE_PDM

  struct DriverPDM : public DriverCommon {
    i2s_pdm_rx_slot_config_t getRxSlotConfig(I2SConfigESP32V1 &cfg) {
      return I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
          (i2s_data_bit_width_t)cfg.bits_per_sample,
          (i2s_slot_mode_t)cfg.channels);
    }

    i2s_pdm_tx_slot_config_t getTxSlotConfig(I2SConfigESP32V1 &cfg) {
      return I2S_PDM_TX_SLOT_DEFAULT_CONFIG(
          (i2s_data_bit_width_t)cfg.bits_per_sample,
          (i2s_slot_mode_t)cfg.channels);
    }

    i2s_chan_config_t getChannelConfig(I2SConfigESP32V1 &cfg) {
      return I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)cfg.port_no,
                                        cfg.is_master ? I2S_ROLE_MASTER
                                                      : I2S_ROLE_SLAVE);
    }

    i2s_pdm_tx_clk_config_t getTxClockConfig(I2SConfigESP32V1 &cfg) {
      return I2S_PDM_TX_CLK_DEFAULT_CONFIG((uint32_t)cfg.sample_rate);
    }

    i2s_pdm_rx_clk_config_t getRxClockConfig(I2SConfigESP32V1 &cfg) {
      return I2S_PDM_RX_CLK_DEFAULT_CONFIG((uint32_t)cfg.sample_rate);
    }

    bool startChannels(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan,
                      i2s_chan_handle_t &rx_chan, int txPin, int rxPin) {

      if (cfg.rx_tx_mode == TX_MODE) {
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
        if (i2s_channel_enable(tx_chan)!=ESP_OK){
          LOGE("i2s_channel_enable %s", "tx");
          return false;
        }

      }
      if (cfg.rx_tx_mode == RX_MODE) {
        i2s_pdm_rx_config_t pdm_rx_cfg = {.clk_cfg = getRxClockConfig(cfg),
                                          .slot_cfg = getRxSlotConfig(cfg),
                                          .gpio_cfg = {
                                              .clk = (gpio_num_t)cfg.pin_bck,
                                              .din = (gpio_num_t)rxPin,
                                              .invert_flags =
                                                  {
                                                      .clk_inv = false,
                                                  },
                                          }};
        if (i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_rx_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_pdm_tx_mode %s", "rx");
          return false;
        }
        if (i2s_channel_enable(rx_chan)!=ESP_OK){
          LOGE("i2s_channel_enable %s", "rx");
          return false;
        }
        return true;
      }
      TRACEE();
      return false;
    }
  } pdm;

#endif
  // struct DriverTDM : public DriverCommon {
  //   i2s_tdm_slot_config_t getSlotConfig(I2SConfigESP32V1 &cfg) {
  //     return I2S_TDM_SLOT_DEFAULT_CONFIG(
  //         (i2s_data_bit_width_t)cfg.bits_per_sample,
  //         (i2s_slot_mode_t)cfg.channels);
  //   }

  //   i2s_chan_config_t getChannelConfig(I2SConfigESP32V1 &cfg) {
  //     return I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)cfg.port_no,
  //                                       cfg.is_master ? I2S_ROLE_MASTER
  //                                                     : I2S_ROLE_SLAVE);
  //   }

  //   i2s_tdm_clk_config_t getClockConfig(I2SConfigESP32V1 &cfg) {
  //     return I2S_TDM_CLK_DEFAULT_CONFIG((uint32_t)cfg.sample_rate);
  //   }

  //   bool startChannels(I2SConfigESP32V1 &cfg, i2s_chan_handle_t &tx_chan,
  //                     i2s_chan_handle_t &rx_chan, int txPin, int rxPin) {

  //     i2s_tdm_config_t tdm_cfg = {
  //         .clk_cfg = getClockConfig(cfg),
  //         .slot_cfg = getSlotConfig(cfg),
  //         .gpio_cfg =
  //             {
  //                 .clk = (gpio_num_t)cfg.pin_bck,
  //                 .dout = (gpio_num_t)txPin,
  //                 .invert_flags =
  //                     {
  //                         .clk_inv = false,
  //                     },
  //             },
  //     };

  //     if (cfg.rx_tx_mode == TX_MODE) {

  //       if (i2s_channel_init_tdm_mode(tx_chan, &tdm_cfg) != ESP_OK) {
  //         LOGE("i2s_channel_init_tdm_tx_mode %s", "tx");
  //         return false;
  //       }
  //     }
  //     if (cfg.rx_tx_mode == RX_MODE) {
  //       if (i2s_channel_init_tdm_mode(rx_chan, &tdm_cfg) != ESP_OK) {
  //         LOGE("i2s_channel_init_tdm_tx_mode %s", "rx");
  //         return false;
  //       }
  //       return true;
  //     }
  //     return false;
  //   }
  // } tdm;

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

    i2s_chan_config_t chan_cfg = driver.getChannelConfig(cfg);
    if (i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan) != ESP_OK) {
      LOGE("i2s_channel");
      return false;
    }

    is_started = driver.startChannels(cfg, tx_chan, rx_chan, txPin, rxPin);
    if(!is_started){
      LOGE("Channels not started");
    }
    return is_started;
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
    case TDM:
      LOGW("TDM not supported");
    }

    return i2s;
  }
};

using I2SDriver = I2SDriverESP32V1;

} // namespace audio_tools

#endif