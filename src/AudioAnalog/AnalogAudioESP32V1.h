#pragma once

#include "AudioConfig.h"
#if defined(ESP32) && defined(USE_ANALOG) &&                                   \
        ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) ||                     \
    defined(DOXYGEN)
#include "AudioAnalog/AnalogAudioBase.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioStreamsConverter.h"

namespace audio_tools {

#define GET_UNIT(x) ((x >> 3) & 0x1)

/**
 * @brief AnalogAudioStream: A very fast DAC using DMA using the new
 * dac_continuous API
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogDriverESP32V1 : public AnalogDriverBase {
public:
  /// Default constructor
  AnalogDriverESP32V1() = default;

  /// Destructor
  virtual ~AnalogDriverESP32V1() { end(); }

  /// starts the DAC
  bool begin(AnalogConfigESP32V1 cfg) {
    TRACEI();
    bool result = true;
    this->cfg = cfg;

    switch (cfg.rx_tx_mode) {
    case TX_MODE:
      if (!setup_tx()) {
        return false;
      }
      // convert to 16 bits
      if (!converter.begin(cfg, 16)) {
        LOGE("converter");
        return false;
      }
      active_tx = true;
      break;
    case RX_MODE:
      if (!setup_rx()) {
        return false;
      }
      active_rx = true;
      break;
    default:
      LOGE("mode");
      return false;
    }

    active = true;
    return active;
  }

  /// stops the I2S and uninstalls the driver
  void end() override {
    TRACEI();
#ifdef HAS_ESP32_DAC
    if (active_tx) {
      dac_continuous_del_channels(dac_handle);
    }
#endif
    if (active_rx) {
      adc_continuous_stop(adc_handle);
      adc_continuous_deinit(adc_handle);
    }
    converter.end();
    active_tx = false;
    active_rx = false;
    active = false;
  }

  /// writes the data to the DAC
  size_t write(const uint8_t *src, size_t size_bytes) override {
    TRACED();
    return io.write(src, size_bytes);
  }

  size_t readBytes(uint8_t *dest, size_t size_bytes) override {
    TRACED();
    uint32_t result = 0;
    size_t samples = size_bytes / 2;

    if (adc_continuous_read(adc_handle, dest, samples, &result, timeout) ==
        ESP_OK) {
      // convert unsigned to signed ?
    } else {
      result = 0;
    }
    return result * 2;
  }
  int available() override { return active_rx ? DEFAULT_BUFFER_SIZE : 0; }

  void setTimeout(int value) { timeout = value; }

protected:
#ifdef HAS_ESP32_DAC
  dac_continuous_handle_t dac_handle;
#endif
  adc_continuous_handle_t adc_handle;
  AnalogConfigESP32V1 cfg;
  bool active = false;
  bool active_tx = false;
  bool active_rx = false;
  TickType_t timeout = portMAX_DELAY;

  /// writes the int16_t data to the DAC
  class IO16Bit : public AudioStream {
  public:
    IO16Bit(AnalogDriverESP32V1 *driver) { self = driver; }
    size_t write(const uint8_t *src, size_t size_bytes) override {
      TRACED();
#ifdef HAS_ESP32_DAC
      size_t result = 0;

      // convert signed 16 bit to unsigned 8 bits
      int16_t *data16 = (int16_t *)src;
      uint8_t *data8 = (uint8_t *)src;
      int samples = size_bytes / 2;
      for (int j = 0; j < samples; j++) {
        data8[j] = (32768u + data16[j]) >> 8;
      }

      if (dac_continuous_write(self->dac_handle, data8, samples, &result,
                               self->timeout) != ESP_OK) {
        result = 0;
      }
      return result * 2;
#else
      return 0;
#endif
    }

  protected:
    AnalogDriverESP32V1 *self;

  } io{this};

  NumberFormatConverterStream converter{io};

#ifdef HAS_ESP32_DAC

  bool setup_tx() {
    dac_continuous_config_t cont_cfg = {
        .chan_mask =
            cfg.channels == 1 ? cfg.dac_mono_channel : DAC_CHANNEL_MASK_ALL,
        .desc_num = (uint32_t)cfg.buffer_count,
        .buf_size = (size_t)cfg.buffer_size,
        .freq_hz = (uint32_t)cfg.sample_rate,
        .offset = 0,
        .clk_src =
            cfg.use_apll
                ? DAC_DIGI_CLK_SRC_APLL
                : DAC_DIGI_CLK_SRC_DEFAULT, // Using APLL as clock source to
                                            // get a wider frequency range
        .chan_mode = DAC_CHANNEL_MODE_ALTER,
    };
    // Allocate continuous channels
    if (dac_continuous_new_channels(&cont_cfg, &dac_handle) != ESP_OK) {
      LOGE("new_channels");
      return false;
    }
    if (dac_continuous_enable(dac_handle) != ESP_OK) {
      LOGE("enable");
      return false;
    }
    return true;
  }

#else
  bool setup_tx() {
    LOGE("DAC not supported");
    return false;
  }
#endif

  bool setup_rx() {
    int max_channels = sizeof(cfg.adc_channels) / sizeof(adc_channel_t);
    if (cfg.channels > max_channels) {
      LOGE("channels: %d, max: %d", cfg.channels, max_channels);
      return false;
    } else {
      LOGI("channels: %d, max: %d", cfg.channels, max_channels);
    }
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = (uint32_t)cfg.buffer_size * cfg.buffer_count,
        .conv_frame_size = (uint32_t)cfg.buffer_size /
                           SOC_ADC_DIGI_DATA_BYTES_PER_CONV *
                           SOC_ADC_DIGI_DATA_BYTES_PER_CONV,
    };
    esp_err_t err = adc_continuous_new_handle(&adc_config, &adc_handle);
    if (err != ESP_OK) {
      LOGE("adc_continuous_new_handle failed with error: %d", err);
      return false;
    } else {
      LOGI("adc_continuous_new_handle successful");
    }

    if ((cfg.sample_rate < SOC_ADC_SAMPLE_FREQ_THRES_LOW) ||
        (cfg.sample_rate > SOC_ADC_SAMPLE_FREQ_THRES_HIGH)) {
      LOGE("sample rate: %u can not be set, range: %u to %u",
           SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH);
      return false;
    } else {
      LOGI("sample rate: %u, range: %u to %u", cfg.sample_rate,
           SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH);
    }

    if ((cfg.adc_bit_width < SOC_ADC_DIGI_MIN_BITWIDTH) ||
        (cfg.adc_bit_width > SOC_ADC_DIGI_MAX_BITWIDTH)) {
      LOGE("adc bit width: %u cannot be set, range: %u to %u",
           SOC_ADC_DIGI_MIN_BITWIDTH, SOC_ADC_DIGI_MAX_BITWIDTH);
      return false;
    } else {
      LOGI("adc bit width: %u, range: %u to %u", cfg.adc_bit_width,
           SOC_ADC_DIGI_MIN_BITWIDTH, SOC_ADC_DIGI_MAX_BITWIDTH);
    }

    if (!checkBitsPerSample()) {
      return false;
    }

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = cfg.sample_rate,
        .conv_mode = cfg.adc_conversion_mode,
        .format = cfg.adc_output_type,
    };
    adc_digi_pattern_config_t adc_pattern[cfg.channels] = {0};
    dig_cfg.pattern_num = cfg.channels;
    for (int i = 0; i < cfg.channels; i++) {
      uint8_t unit = GET_UNIT(cfg.adc_channels[i]);
      uint8_t ch = cfg.adc_channels[i] & 0x7;
      adc_pattern[i].atten = cfg.adc_attenuation;
      adc_pattern[i].channel = ch;
      adc_pattern[i].unit = unit;
      adc_pattern[i].bit_width = cfg.adc_bit_width;
    }
    dig_cfg.adc_pattern = adc_pattern;

    LOGI("dig_cfg.sample_freq_hz: %u", dig_cfg.sample_freq_hz);
    LOGI("dig_cfg.conv_mode: %u", dig_cfg.conv_mode);
    LOGI("dig_cfg.format: %u", dig_cfg.format);
    for (int i = 0; i < cfg.channels; i++) {
      LOGI("dig_cfg.adc_pattern[%d].atten: %u", i,
           dig_cfg.adc_pattern[i].atten);
      LOGI("dig_cfg.adc_pattern[%d].channel: %u", i,
           dig_cfg.adc_pattern[i].channel);
      LOGI("dig_cfg.adc_pattern[%d].unit: %u", i, dig_cfg.adc_pattern[i].unit);
      LOGI("dig_cfg.adc_pattern[%d].bit_width: %u", i,
           dig_cfg.adc_pattern[i].bit_width);
    }

    err = adc_continuous_config(adc_handle, &dig_cfg);
    if (err != ESP_OK) {
      LOGE("adc_continuous_config unsuccessful with error: %d", err);
      return false;
    }
    LOGI("adc_continuous_config successful");

    err = adc_continuous_start(adc_handle);
    if (err != ESP_OK) {
      LOGE("adc_continuous_start unsuccessful with error: %d", err);
      return false;
    }

    LOGI("adc_continuous_start successful");
    return true;
  }

  // this is maybe a little bit too much since currently any supported
  // adc_bit_width leads to int16_t data
  bool checkBitsPerSample() {
    // adjust bits per sample based on adc bit width setting (multiple of 8)
    LOGI("cfg.bits_per_sample: %d", cfg.bits_per_sample);
    if (cfg.bits_per_sample == 0) {
      if (cfg.adc_bit_width <= 8) {
        cfg.bits_per_sample = 8;
      } else if (cfg.adc_bit_width <= 16) {
        cfg.bits_per_sample = 16;
      } else if (cfg.adc_bit_width <= 24) {
        cfg.bits_per_sample = 24;
      } else if (cfg.adc_bit_width <= 32) {
        cfg.bits_per_sample = 32;
      } else {
        cfg.bits_per_sample = 16;
      }
      LOGI("bits per sample: %d", cfg.bits_per_sample);
      return true;
    }

    // check bits per sample with requested adc_bit_width
    switch (cfg.bits_per_sample) {
      case 8:
        if (cfg.adc_bit_width > 8) {
          LOGE("bits_per_sample %d not valid for adc_bit_width %d",
               cfg.bits_per_sample, cfg.adc_bit_width);
          return false;
        }
        break;
      case 16:
        if (cfg.adc_bit_width <= 8 || cfg.adc_bit_width > 16) {
          LOGE("bits_per_sample %d not valid for adc_bit_width %d",
               cfg.bits_per_sample, cfg.adc_bit_width);
          return false;
        }
        break;
      case 24:
        if (cfg.adc_bit_width <= 16 || cfg.adc_bit_width > 24) {
          LOGE("bits_per_sample %d not valid for adc_bit_width %d",
               cfg.bits_per_sample, cfg.adc_bit_width);
          return false;
        }
        break;
      case 32:
        if (cfg.adc_bit_width <= 24 || cfg.adc_bit_width >= 32) {
          LOGE("bits_per_sample %d not valid for adc_bit_width %d",
               cfg.bits_per_sample, cfg.adc_bit_width);
          return false;
        }
        break; 
    }
    return true;
  }
};

/// @brief AnalogAudioStream
using AnalogDriver = AnalogDriverESP32V1;

} // namespace audio_tools

#endif
