#pragma once

#include "AudioConfig.h"
#if defined(ESP32) && defined(USE_ANALOG) &&  ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) || defined(DOXYGEN)
#include "AudioAnalog/AnalogAudioBase.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioStreamsConverter.h"

namespace audio_tools {

#define GET_UNIT(x)        ((x>>3) & 0x1)


/**
 * @brief AnalogAudioStream: A very fast DAC using DMA using the new dac_continuous API
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

    switch(cfg.rx_tx_mode){
      case TX_MODE:
        if (!setup_tx()){
          return false;
        }
        active_tx = true;
        break;
      case RX_MODE:
        if (!setup_rx()){
          return false;
        }
        active_rx = true;
        break;
      default:
        LOGE("mode");
        return false;
    }

    // convert to 16 bits
    if (!converter.begin(cfg, 16)){
      LOGE("converter");
      return false;
    }
    active = true;
    return active;
  }

  /// stops the I2S and unistalls the driver
  void end() override {
    TRACEI();
#ifdef HAS_ESP32_DAC
    if (active_tx)  {
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
    return io.readBytes(dest, size_bytes);
  }

  int available() override {
    return 0;
  } 

  void setTimeout(int value){
    timeout = value;
  }

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
      IO16Bit(AnalogDriverESP32V1 *driver){
        self = driver;
      }
      size_t write(const uint8_t *src, size_t size_bytes) override {
        TRACED();
#ifdef HAS_ESP32_DAC
        size_t result = 0;

        // convert signed 16 bit to unsigned 8 bits
        int16_t* data16 = (int16_t*)src;
        uint8_t* data8 = (uint8_t*)src;
        int samples = size_bytes / 2;
        for (int j=0; j < samples; j++){
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

      size_t readBytes(uint8_t *dest, size_t size_bytes) override {
        TRACED();
        uint32_t result = 0;
        size_t samples = size_bytes / 2;

        if(adc_continuous_read(self->adc_handle, dest, samples, &result, self->timeout)== ESP_OK) {
          // convert unsigned 8 bits to signed 16 bits;
          int16_t* data16 = (int16_t*)dest;
          for (int j = result-1; j>=0; j--){
            data16[j] = (static_cast<int16_t>(dest[8]) - 128) * 256;
          }
        } else {
          result = 0;
        }
        return result * 2;
      }
    protected:
      AnalogDriverESP32V1 *self;

  } io{this};

  NumberFormatConverterStream converter{io};

#ifdef HAS_ESP32_DAC

  bool setup_tx(){
    dac_continuous_config_t cont_cfg = {
        .chan_mask =
            cfg.channels == 1 ? cfg.dac_mono_channel : DAC_CHANNEL_MASK_ALL,
        .desc_num = (uint32_t)cfg.buffer_count,
        .buf_size = (size_t) cfg.buffer_size,
        .freq_hz = (uint32_t)cfg.sample_rate,
        .offset = 0,
        .clk_src =
            cfg.use_apll
                ? DAC_DIGI_CLK_SRC_APLL
                : DAC_DIGI_CLK_SRC_DEFAULT,  // Using APLL as clock source to
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
  bool setup_tx(){
    LOGE("DAC not supported");
    return false;
  }
  #endif


  bool setup_rx(){
    int max_channels = sizeof(cfg.adc_channels)/sizeof(adc_channel_t);
    if (cfg.channels > max_channels ){
      LOGE("channels: %d, max: %d", cfg.channels, max_channels);
      return false;
    }
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = (uint32_t)cfg.buffer_size * cfg.buffer_count,
        .conv_frame_size = (uint32_t)cfg.buffer_size,
    };
    adc_continuous_new_handle(&adc_config, &adc_handle);

    adc_continuous_config_t dvig_cfg = {
        .sample_freq_hz = (uint32_t)cfg.sample_rate,
        .conv_mode = (adc_digi_convert_mode_t)cfg.adc_conversion_mode,
        .format = (adc_digi_output_format_t)cfg.adc_output_type,
    };
    adc_digi_pattern_config_t adc_pattern[cfg.channels] = {0};
    adc_continuous_config_t dig_cfg = {};
    dig_cfg.pattern_num = cfg.channels;
    for (int i = 0; i < cfg.channels; i++) {
        uint8_t unit = GET_UNIT(cfg.adc_channels[i]);
        uint8_t ch = cfg.adc_channels[i] & 0x7;
        adc_pattern[i].atten = cfg.adc_attenuation;
        adc_pattern[i].channel = ch;
        adc_pattern[i].unit = unit;
        adc_pattern[i].bit_width = cfg.adc_bit_width;

        LOGI("adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
        LOGI("adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
        LOGI("adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    if (adc_continuous_config(adc_handle, &dig_cfg)!=ESP_OK){
      LOGE("adc_continuous_config");
      return false;
    }
    return true;
  }

};

/// @brief AnalogAudioStream
using AnalogDriver = AnalogDriverESP32V1;

}  // namespace

#endif
