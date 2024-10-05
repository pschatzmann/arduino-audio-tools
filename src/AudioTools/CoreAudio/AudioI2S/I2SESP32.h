#pragma once

#include "AudioConfig.h"
#if defined(ESP32) && defined(USE_I2S) &&                 \
        ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0) || \
    defined(DOXYGEN)

#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#include "driver/i2s.h"
#include "esp_system.h"

#ifndef I2S_MCLK_MULTIPLE_DEFAULT
#  define I2S_MCLK_MULTIPLE_DEFAULT ((i2s_mclk_multiple_t)0)
#endif

#define IS_I2S_IMPLEMENTED 

namespace audio_tools {

/**
 * @brief Basic I2S API - for the ESP32. If we receive 1 channel, we expand the
 * result to 2 channels.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriverESP32 {
  friend class AnalogAudio;
  friend class AudioKitStream;

 public:
  /// Provides the default configuration
  I2SConfigESP32 defaultConfig(RxTxMode mode) {
    I2SConfigESP32 c(mode);
    return c;
  }

  /// Potentially updates the sample rate (if supported)
  bool setAudioInfo(AudioInfo info) {
    // nothing to do
    if (is_started) {
      if (info.equals(cfg)) return true;
      if (info.equalsExSampleRate(cfg)) {
        cfg.sample_rate = info.sample_rate;
        LOGI("i2s_set_sample_rates: %d", info.sample_rate);
        return i2s_set_sample_rates((i2s_port_t)cfg.port_no, cfg.sample_rate) == ESP_OK;
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
  bool begin() { return !is_started ? begin(cfg) : true; }

  /// starts the DAC
  bool begin(I2SConfigESP32 cfg) {
    TRACED();
    this->cfg = cfg;
    switch (cfg.rx_tx_mode) {
      case TX_MODE:
        return begin(cfg, cfg.pin_data, I2S_PIN_NO_CHANGE);
      case RX_MODE:
        // usually we expet cfg.pin_data but if the used assinged rx we might
        // consider this one
        return begin(
            cfg, I2S_PIN_NO_CHANGE,
            cfg.pin_data != I2S_PIN_NO_CHANGE ? cfg.pin_data : cfg.pin_data_rx);
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
    i2s_driver_uninstall(i2s_num);
    is_started = false;
  }

  /// provides the actual configuration
  I2SConfigESP32 config() { return cfg; }

  /// writes the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    TRACED();

    size_t result = 0;
    if (isNoChannelConversion(cfg)) {
      if (i2s_write(i2s_num, src, size_bytes, &result, ticks_to_wait_write) !=
          ESP_OK) {
        TRACEE();
      }
      LOGD("i2s_write %d -> %d bytes", size_bytes, result);
    } else {
      result =
          writeExpandChannel(i2s_num, cfg.bits_per_sample, src, size_bytes);
    }
    return result;
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    size_t result = 0;
    if (isNoChannelConversion(cfg)) {
      if (i2s_read(i2s_num, dest, size_bytes, &result, ticks_to_wait_read) !=
          ESP_OK) {
        TRACEE();
      }
    } else if (cfg.channels == 1) {
      // I2S has always 2 channels. We support to reduce it to 1
      uint8_t temp[size_bytes * 2];
      if (i2s_read(i2s_num, temp, size_bytes * 2, &result,
                   ticks_to_wait_read) != ESP_OK) {
        TRACEE();
      }
      // convert to 1 channel
      switch (cfg.bits_per_sample) {
        case 16: {
          ChannelReducerT<int16_t> reducer16(1, 2);
          result = reducer16.convert((uint8_t *)dest, temp, result);
        } break;
        case 24: {
          ChannelReducerT<int24_t> reducer24(1, 2);
          result = reducer24.convert((uint8_t *)dest, temp, result);
        } break;
        case 32: {
          ChannelReducerT<int32_t> reducer32(1, 2);
          result = reducer32.convert((uint8_t *)dest, temp, result);
        } break;
        default:
          LOGE("invalid bits_per_sample: %d", cfg.bits_per_sample);
          break;
      }
    } else {
      LOGE("Invalid channels: %d", cfg.channels);
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
  I2SConfigESP32 cfg = defaultConfig(RX_MODE);
  i2s_port_t i2s_num;
  i2s_config_t i2s_config;
  bool is_started = false;
  TickType_t ticks_to_wait_read = portMAX_DELAY;
  TickType_t ticks_to_wait_write = portMAX_DELAY;

  bool isNoChannelConversion(I2SConfigESP32 cfg) {
    if (cfg.channels == 2) return true;
    if (cfg.channels == 1 && cfg.channel_format == I2S_CHANNEL_FMT_ALL_RIGHT)
      return true;
    if (cfg.channels == 1 && cfg.channel_format == I2S_CHANNEL_FMT_ALL_LEFT)
      return true;
    if (cfg.channels == 1 && cfg.channel_format == I2S_CHANNEL_FMT_ONLY_RIGHT)
      return true;
    if (cfg.channels == 1 && cfg.channel_format == I2S_CHANNEL_FMT_ONLY_LEFT)
      return true;
    return false;
  }

  /// starts the DAC
  bool begin(I2SConfigESP32 cfg, int txPin, int rxPin) {
    TRACED();
    cfg.logInfo();
    this->cfg = cfg;
    this->i2s_num = (i2s_port_t)cfg.port_no;
    setChannels(cfg.channels);

    i2s_config_t i2s_config_new = {
        .mode = toMode(cfg),
        .sample_rate = (eps32_i2s_sample_rate_type)cfg.sample_rate,
        .bits_per_sample = (i2s_bits_per_sample_t)cfg.bits_per_sample,
        .channel_format = (i2s_channel_fmt_t)cfg.channel_format,
        .communication_format = toCommFormat(cfg.i2s_format),
        .intr_alloc_flags = 0,  // default interrupt priority
        .dma_buf_count = cfg.buffer_count,
        .dma_buf_len = cfg.buffer_size,
        .use_apll = (bool)cfg.use_apll,
        .tx_desc_auto_clear = cfg.auto_clear,
#if ESP_IDF_VERSION_MAJOR >= 4
        .fixed_mclk = (int)(cfg.fixed_mclk > 0 ? cfg.fixed_mclk : 0),
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
#endif
    };
    i2s_config = i2s_config_new;

    // We make sure that we can reconfigure
    if (is_started) {
      end();
      LOGD("%s", "I2S restarting");
    }

    // setup config
    LOGD("i2s_driver_install");
    if (i2s_driver_install(i2s_num, &i2s_config, 0, NULL) != ESP_OK) {
      LOGE("%s - %s", __func__, "i2s_driver_install");
    }

    // setup pin config
    if (this->cfg.signal_type == Digital || this->cfg.signal_type == PDM) {
      i2s_pin_config_t pin_config = {
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 4, 0)
          .mck_io_num = cfg.pin_mck,
#endif
          .bck_io_num = cfg.pin_bck,
          .ws_io_num = cfg.pin_ws,
          .data_out_num = txPin,
          .data_in_num = rxPin};

      LOGD("i2s_set_pin");
      if (i2s_set_pin(i2s_num, &pin_config) != ESP_OK) {
        LOGE("%s - %s", __func__, "i2s_set_pin");
      }
    } else {
      LOGD("Using built in DAC");
      // for internal DAC, this will enable both of the internal channels
      i2s_set_pin(i2s_num, NULL);
    }

    // clear initial buffer
    LOGD("i2s_zero_dma_buffer");
    i2s_zero_dma_buffer(i2s_num);

    is_started = true;
    LOGD("%s - %s", __func__, "started");
    return true;
  }

  // update the cfg.i2s.channel_format based on the number of channels
  void setChannels(int channels) { cfg.channels = channels; }

  /// writes the data by making sure that we send 2 channels
  size_t writeExpandChannel(i2s_port_t i2s_num, const int bits_per_sample,
                            const void *src, size_t size_bytes) {
    size_t result = 0;
    int j;
    switch (bits_per_sample) {
      case 8:
        for (j = 0; j < size_bytes; j++) {
          int8_t frame[2];
          int8_t *data = (int8_t *)src;
          frame[0] = data[j];
          frame[1] = data[j];
          size_t result_call = 0;
          if (i2s_write(i2s_num, frame, sizeof(int8_t) * 2, &result_call,
                        ticks_to_wait_write) != ESP_OK) {
            TRACEE();
          } else {
            result += result_call;
          }
        }
        break;

      case 16:
        for (j = 0; j < size_bytes / 2; j++) {
          int16_t frame[2];
          int16_t *data = (int16_t *)src;
          frame[0] = data[j];
          frame[1] = data[j];
          size_t result_call = 0;
          if (i2s_write(i2s_num, frame, sizeof(int16_t) * 2, &result_call,
                        ticks_to_wait_write) != ESP_OK) {
            TRACEE();
          } else {
            result += result_call;
          }
        }
        break;

      case 24:
        for (j = 0; j < size_bytes / 4; j++) {
          int24_t frame[2];
          int24_t *data = (int24_t *)src;
          frame[0] = data[j];
          frame[1] = data[j];
          size_t result_call = 0;
          if (i2s_write(i2s_num, frame, sizeof(int24_t) * 2, &result_call,
                        ticks_to_wait_write) != ESP_OK) {
            TRACEE();
          } else {
            result += result_call;
          }
        }
        break;

      case 32:
        for (j = 0; j < size_bytes / 4; j++) {
          int32_t frame[2];
          int32_t *data = (int32_t *)src;
          frame[0] = data[j];
          frame[1] = data[j];
          size_t result_call = 0;
          if (i2s_write(i2s_num, frame, sizeof(int32_t) * 2, &result_call,
                        ticks_to_wait_write) != ESP_OK) {
            TRACEE();
          } else {
            result += result_call;
          }
        }
        break;
    }
    return size_bytes;
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

  // determines the i2s_comm_format_t - by default we use
  // I2S_COMM_FORMAT_STAND_I2S
  i2s_comm_format_t toCommFormat(I2SFormat mode) {
    switch (mode) {
      case I2S_PHILIPS_FORMAT:
      case I2S_STD_FORMAT:
        return (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S;
      case I2S_LEFT_JUSTIFIED_FORMAT:
      case I2S_MSB_FORMAT:
        return (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S |
                                   I2S_COMM_FORMAT_I2S_MSB);
      case I2S_RIGHT_JUSTIFIED_FORMAT:
      case I2S_LSB_FORMAT:
        return (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S |
                                   I2S_COMM_FORMAT_I2S_LSB);
      case I2S_PCM:
        return (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_PCM_SHORT;

      default:
        LOGE("unsupported mode");
        return (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S;
    }
  }
#pragma GCC diagnostic pop

  int getModeDigital(I2SConfigESP32 &cfg) {
    int i2s_format = cfg.is_master ? I2S_MODE_MASTER : I2S_MODE_SLAVE;
    int i2s_rx_tx = 0;
    switch (cfg.rx_tx_mode) {
      case TX_MODE:
        i2s_rx_tx = I2S_MODE_TX;
        break;
      case RX_MODE:
        i2s_rx_tx = I2S_MODE_RX;
        break;
      case RXTX_MODE:
        i2s_rx_tx = I2S_MODE_RX | I2S_MODE_TX;
        break;
      default:
        LOGE("Undefined rx_tx_mode: %d", cfg.rx_tx_mode);
    }
    return (i2s_format | i2s_rx_tx);
  }

  // determines the i2s_format_t
  i2s_mode_t toMode(I2SConfigESP32 &cfg) {
    i2s_mode_t mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    switch (cfg.signal_type) {
      case Digital:
        mode = (i2s_mode_t)getModeDigital(cfg);
        break;

      case PDM:
        mode = (i2s_mode_t)(getModeDigital(cfg) | I2S_MODE_PDM);
        break;

      case Analog:
#if defined(USE_ANALOG)
        mode = (i2s_mode_t)(cfg.rx_tx_mode ? I2S_MODE_DAC_BUILT_IN
                                           : I2S_MODE_ADC_BUILT_IN);
#else
        LOGE("mode not supported");
#endif
        break;

      default:
        LOGW("signal_type undefined");
        mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        break;
    }
    return mode;
  }
};

using I2SDriver = I2SDriverESP32;

}  // namespace audio_tools

#endif