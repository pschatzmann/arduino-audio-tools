#pragma once

#if defined(ESP32) && defined(I2S_NEW)

#include "AudioConfig.h"
#include "AudioI2S/I2SConfig.h"
#include "driver/i2s_std.h"
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"
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
class I2SDriverESP32New {

public:
  /// Provides the default configuration
  I2SConfig defaultConfig(RxTxMode mode) {
    I2SConfig c(mode);
    return c;
  }

  /// starts the DAC with the default config
  bool begin(RxTxMode mode) { return begin(defaultConfig(mode)); }

  /// starts the DAC with the current config - if not started yet. If I2S has
  /// been started there is no action and we return true
  bool begin() { return !is_started ? begin(cfg) : true; }

  /// starts the DAC
  bool begin(I2SConfig cfg) {
    TRACED();
    this->cfg = cfg;
    switch (cfg.rx_tx_mode) {
    case TX_MODE:
      return begin(cfg, cfg.pin_data, I2S_PIN_NO_CHANGE);
    case RX_MODE:
      // usually we expet cfg.pin_data but if the used assinged rx we might
      // consider this one
      return begin(cfg, I2S_PIN_NO_CHANGE,
                   cfg.pin_data != I2S_PIN_NO_CHANGE ? cfg.pin_data
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
    if (cfg.mode == RXTX_MODE || cfg.mode == TX_MODE) {
      i2s_channel_disable(rx_chan);
      i2s_del_channel(rx_chan)
    }
    if (cfg.mode == RXTX_MODE || cfg.mode == TX_MODE) {
      i2s_channel_disable(tx_chan);
      i2s_del_channel(tx_chan);
    }

    is_started = false;
  }

  /// provides the actual configuration
  I2SConfig config() { return cfg; }

  /// writes the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    TRACED();
    if (i2s_channel_write(i2s_num, src, size_bytes, &result, portMAX_DELAY) !=
        ESP_OK) {
      TRACEE();
    }
    return result;
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    size_t result = 0;
    if (i2s_channel_read(i2s_num, dest, size_bytes, &result, portMAX_DELAY) !=
        ESP_OK) {
      TRACEE();
    }
    return result;
  }

protected:
  I2SConfig cfg = defaultConfig(RX_MODE);
  i2s_port_t i2s_num;
  i2s_config_t i2s_config;
  i2s_chan_handle_t tx_chan; // I2S tx channel handler
  i2s_chan_handle_t rx_chan; // I2S rx channel handler
  bool is_started = false;

  /// starts the DAC
  bool begin(I2SConfig cfg, int txPin, int rxPin) {
    TRACED();
    cfg.logInfo();
    this->cfg = cfg;
    this->i2s_num = (i2s_port_t)cfg.port_no;

    i2s_chan_config_t chan_cfg = getChannelConfig(cfg);
    if (i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan) != ESP_OK) {
      LOGE("i2s_new_channel");
      return false;
    }

    if (cfg.channels <= 0 || channels > 2) {
      LOGE("invalid channels: %d", cfg.channels);
      return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = getClockConfig(cfg).slot_cfg = getSlotConfig(cfg),
        .gpio_cfg =
            {
                .mclk = cfg.pin_mck,
                .bclk = cfg.pin_bck,
                .ws = cfg.pin_ws,
                .dout = txPin,
                .din = rxPin,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    return initChannels(std_cfg);
  }

  i2s_std_slot_config_t getSlotConfig(I2SConfig &cfg) {
    switch (cfg.signal_type) {
    case Digital: {
      switch (cfg.format) {
      case I2S_RIGHT_JUSTIFIED_FORMAT:
      case I2S_LSB_FORMAT:
      case I2S_PHILIPS_FORMAT:
      case I2S_STD_FORMAT:
        return I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(cfg.bits_per_sample,
                                                   cfg.channels);
      case I2S_LEFT_JUSTIFIED_FORMAT:
      case I2S_MSB_FORMAT:
        return I2S_STD_MSB_SLOT_DEFAULT_CONFIG(cfg.bits_per_sample,
                                               cfg.channels);
      case I2S_PCM:
        return I2S_STD_PCM_SLOT_DEFAULT_CONFIG(cfg.bits_per_sample,
                                               cfg.channels);
      }
    }
    case PDM:
      return I2S_PDM_RX_SLOT_DEFAULT_CONFIG(cfg.bits_per_sample, cfg.channels);
    case TDM:
      return I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(cfg.bits_per_sample, cfg.channels,
                                             I2S_TDM_SLOT0 | I2S_TDM_SLOT1 |
                                                 I2S_TDM_SLOT2 | I2S_TDM_SLOT3);
    }
  }

  i2s_chan_config_t getChannelConfig(I2SConfig &cfg) {
      return I2S_CHANNEL_DEFAULT_CONFIG(i2s_port_t)i2s_num, cfg.is_master ? I2S_ROLE_MASTER : I2S_ROLE_CLIENT)
  }

  i2s_clock_config_t getClockConfig(I2SConfig &cfg) {
    switch (cfg.signal_type) {
    case Digital:
      return I2S_STD_CLK_DEFAULT_CONFIG(cfg.sample_rate);
    case Analog:
    case PDM:
      return I2S_PDM_CLK_DEFAULT_CONFIG(cfg.sample_rate);
    case TDM:
      return I2S_TDM_CLK_DEFAULT_CONFIG(cfg.sample_rate);
    }
    assert(false);
  }

  bool initChannels(i2s_std_config_t init_channel) {
    switch (cfg.signal_type) {
    case Digital: {
      /* Initialize the channels */
      if (cfg.mode == RXTX_MODE || cfg.mode == TX_MODE) {
        if (i2s_channel_init_std_mode(tx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_std_mode %s", "tx");
          return false;
        }
      }

      if (cfg.mode == RXTX_MODE || cfg.mode == RX_MODE) {
        if (i2s_channel_init_std_mode(rx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_std_mode %s", "rx");
          return false;
        }
      }
    } break;
    case Analog:
    case PDM: {
      if (cfg.mode == RXTX_MODE || cfg.mode == TX_MODE) {
        if (i2s_channel_init_pdm_tx_mode(tx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_pdm_tx_mode %s", "tx");
          return false;
        }
      }
      if (cfg.mode == RXTX_MODE || cfg.mode == RX_MODE) {
        if (i2s_channel_init_pdm_tx_mode(rx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_pdm_tx_mode %s", "rx");
          return false;
        }
      }
    } break;

    case TDM: {
      if (cfg.mode == RXTX_MODE) {
        LOGE("RXTX_MODE not supported");
        return false;
      }

      if (cfg.mode == TX_MODE) {
        if (i2s_channel_init_tdm_mode(tx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_tdm_tx_mode %s", "tx");
          return false;
        }
      }

      if (cfg.mode == RX_MODE) {
        if (i2s_channel_init_tdm_tx_mode(rx_chan, &std_cfg) != ESP_OK) {
          LOGE("i2s_channel_init_tdm_tx_mode %s", "rx");
          return false;
        }
      }
    } break;

    default:
      LOGE("Unsupported signal_type");
      return false;
    }

    is_started = true;
    LOGD("%s - %s", __func__, "started");
    return true;
  }
};

using I2SDriver = I2SDriverESP32New;

} // namespace audio_tools

#endif