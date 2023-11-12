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
  #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED        
      adc_cali_delete_scheme_curve_fitting(adc_cali_handle);
  #elif !defined(CONFIG_IDF_TARGET_ESP32H2)
      adc_cali_delete_scheme_line_fitting(adc_cali_handle);
  #endif
    }
    converter.end();
    active_tx = false;
    active_rx = false;
    active = false;
  }

  // writes the data to the DAC
  size_t write(const uint8_t *src, size_t size_bytes) override {
    TRACED();
    return io.write(src, size_bytes);
  }

  // reads data from DMA buffer
  size_t readBytes(uint8_t *dest, size_t size_bytes) override {
    TRACED();

    int data_milliVolts;    
    uint32_t bytes_read = 0;

    // size_bytes should be multiple of SOC_ADC_DIGI_RESULT_BYTES
    if ( (size_bytes % SOC_ADC_DIGI_RESULT_BYTES) !=0) {
      LOGE("requested %d bytes is not a multiple of %d", size_bytes, SOC_ADC_DIGI_RESULT_BYTES);
      return 0;
    }

    size_t num_samples = size_bytes / SOC_ADC_DIGI_RESULT_BYTES;
    LOGI("requesting %d bytes, %d samples", size_bytes, num_samples);

    // Manual says
    // read DMA buffer (hanndle, bufffer, expected length in bytes, real length in bytes, timeout in ms)
    esp_err_t err = adc_continuous_read(adc_handle, dest, size_bytes, &bytes_read, timeout);
    if (err != ESP_OK) {
      if ( err == ESP_ERR_TIMEOUT) { LOGE("reading data failed, increase timeout"); } 
      else { LOGE("reading data failed with error %d", err); }
      dest = NULL;
      return 0;
    }
    
    LOGI("received %d bytes, %d samples, at %d ms", bytes_read, bytes_read / SOC_ADC_DIGI_RESULT_BYTES, millis());

    // Calibrate the ADC readings
    for (int i = 0; i < bytes_read; i += SOC_ADC_DIGI_RESULT_BYTES) {
      int raw_data;
    #if SOC_ADC_DIGI_RESULT_BYTES == 4
      raw_data = *(uint32_t *) &dest[i];
    #else
      raw_data = *(uint16_t *) &dest[i];
    #endif
        err = adc_cali_raw_to_voltage(adc_cali_handle, raw_data, &data_milliVolts);
        if (err == ESP_OK) {
    #if SOC_ADC_DIGI_RESULT_BYTES == 4
        *(uint32_t *) &dest[i] = (uint32_t) data_milliVolts;
    #else
        *(uint16_t *) &dest[i] = (uint16_t) data_milliVolts;
    #endif
        } else { LOGE("converting data to milliVolts failed with error: %d", err); }
    }
    LOGI("calibrated %d bytes %d samples", bytes_read, bytes_read / SOC_ADC_DIGI_RESULT_BYTES);

    return bytes_read; 
  }

  // is data in the ADC buffer?
  int available() override { return active_rx ? DEFAULT_BUFFER_SIZE : 0; }

  // set ADC read timtout
  void setTimeout(int value) { timeout = value; }

protected:
#ifdef HAS_ESP32_DAC
  dac_continuous_handle_t dac_handle;
#endif
  adc_continuous_handle_t adc_handle;
  adc_cali_handle_t adc_cali_handle; 
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

    adc_continuous_handle_cfg_t adc_config;

    // Code from esp32-hal-adc.c:
    // adc_handle[adc_unit].conversion_frame_size = conversions_per_pin * pins_count * SOC_ADC_DIGI_RESULT_BYTES;
    // #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    //     uint8_t calc_multiple = adc_handle[adc_unit].conversion_frame_size % SOC_ADC_DIGI_DATA_BYTES_PER_CONV;
    //     if(calc_multiple != 0){
    //         adc_handle[adc_unit].conversion_frame_size = (adc_handle[adc_unit].conversion_frame_size + calc_multiple);
    //     }
    // #endif
    // adc_handle[adc_unit].buffer_size = adc_handle[adc_unit].conversion_frame_size * 2;
    // adc_continuous_handle_cfg_t adc_config = {
    //     .max_store_buf_size = adc_handle[adc_unit].buffer_size,
    //     .conv_frame_size = adc_handle[adc_unit].conversion_frame_size,
    // };

    /* Because ESPs have different ADC DMA and conversion configurations
       we need to adjust the frame_size accordingly
        ESP32S2
          SOC_ADC_DIGI_RESULT_BYTES        (2)
          SOC_ADC_DIGI_DATA_BYTES_PER_CONV (2)
        ESP32 
          SOC_ADC_DIGI_RESULT_BYTES        (2)
          SOC_ADC_DIGI_DATA_BYTES_PER_CONV (4)
        ESP32C3 || ESP32H2 || ESP32C6 || ESP32S3
          SOC_ADC_DIGI_RESULT_BYTES        (4)
          SOC_ADC_DIGI_DATA_BYTES_PER_CONV (4)
        ESP32C2 
          is not defined, likley the same as c3
    */
    
    adc_config.conv_frame_size = cfg.buffer_size * cfg.buffer_count * SOC_ADC_DIGI_RESULT_BYTES;
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    uint8_t calc_multiple = adc_config.conv_frame_size % SOC_ADC_DIGI_DATA_BYTES_PER_CONV;
    if (calc_multiple != 0) {
        adc_config.conv_frame_size += (SOC_ADC_DIGI_DATA_BYTES_PER_CONV - calc_multiple);
    }
#endif
    adc_config.max_store_buf_size = adc_config.conv_frame_size * 2;    
    LOGI("adc conv_frame_size is %d and max store buf size is %d",adc_config.conv_frame_size, adc_config.max_store_buf_size);

    // Create adc_continuous handle
    esp_err_t err = adc_continuous_new_handle(&adc_config, &adc_handle);
    if (err != ESP_OK) {
      LOGE("adc_continuous_new_handle failed with error: %d", err);
      return false;
    } else {
      LOGI("adc_continuous_new_handle successful");
    }

    // Boundary checks
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

    // Update bits per sample
    if (!checkBitsPerSample()) {
      return false;
    }

    /// Configure the ADC
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

    // Initialize ADC
    err = adc_continuous_config(adc_handle, &dig_cfg);
    if (err != ESP_OK) {
      LOGE("adc_continuous_config unsuccessful with error: %d", err);
      return false;
    }
    LOGI("adc_continuous_config successful");

    // Initialize ADC calibration handle
    //
    // Calibration is applied to an ADC unit (not per channel). There is ADC1 and ADC2 unit (0,1). 
    // ADC2 and WiFi share hardware and WiFi has priority. Therefore we prefere ADC1.

    // Lets make sure ADC Channels chosen are on same unit:
    uint8_t unit=GET_UNIT(cfg.adc_channels[0]);
    for (int i = 1; i < cfg.channels; i++) {
      if (unit != GET_UNIT(cfg.adc_channels[i])) {
        LOGE("error, dont select ADC channels on different ADC units");
        return false;
      }
    }
    
    if(adc_cali_handle == NULL){
      #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        // curve fitting is preferred
        adc_cali_curve_fitting_config_t cali_config;
        cali_config.unit_id  = (adc_unit_t) unit;
        cali_config.atten    = (adc_atten_t) cfg.adc_attenuation;
        cali_config.bitwidth = (adc_bitwidth_t) cfg.adc_bit_width;
        err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
      #elif !defined(CONFIG_IDF_TARGET_ESP32H2)
        // line fitting
        adc_cali_line_fitting_config_t cali_config;
        cali_config.unit_id  = (adc_unit_t) unit;
        cali_config.atten    = (adc_atten_t) cfg.adc_attenuation;
        cali_config.bitwidth = (adc_bitwidth_t) cfg.adc_bit_width;
        err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
      #endif
        if(err != ESP_OK){
            LOGE("creating cali handle failed for ADC%d with atten %d and bitwidth %d", 
                unit, cfg.adc_attenuation, cfg.adc_bit_width);
            return false;
        } else {
          LOGI("created cali handle for ADC%d with atten %d and bitwidth %d", 
                unit, cfg.adc_attenuation, cfg.adc_bit_width);
        }
    }

    // Start ADC
    err = adc_continuous_start(adc_handle);
    if (err != ESP_OK) {
      LOGE("adc_continuous_start unsuccessful with error: %d", err);
      return false;
    }

    LOGI("adc_continuous_start successful");
    return true;
  }

  bool checkBitsPerSample() {
    if (cfg.bits_per_sample == 0) {
      cfg.bits_per_sample = SOC_ADC_DIGI_RESULT_BYTES*8;
      LOGI("bits per sample set to: %d", cfg.bits_per_sample);
      return true;
    } else if (cfg.bits_per_sample == SOC_ADC_DIGI_RESULT_BYTES*8) {
      LOGI("bits per sample: %d", cfg.bits_per_sample);
      return true;
    } else {
      LOGE("checking bits per sample error. It should be: %d but is %d", SOC_ADC_DIGI_RESULT_BYTES*8, cfg.bits_per_sample);
      return false;
    }
  }

};

/// @brief AnalogAudioStream
using AnalogDriver = AnalogDriverESP32V1;

} // namespace audio_tools

#endif
