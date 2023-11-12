#pragma once

#include "AudioConfig.h"
#if defined(ESP32) && defined(USE_ANALOG) &&                                   \
        ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) ||                     \
    defined(DOXYGEN)

#include "AudioAnalog/AnalogConfigESP32V1.h"
#include "AudioAnalog/AnalogAudioBase.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioStreamsConverter.h"


namespace audio_tools {


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

  size_t readBytes(uint8_t *dest, size_t size_bytes) override {
    TRACED();
    size_t total_bytes = 0;
    // allocate buffer for the requested bytes
    int sample_count = size_bytes / sizeof(int16_t);
    adc_digi_output_data_t result_data[sample_count];
    memset(&result_data, 0, sizeof(result_data));
    uint32_t result_cont;
    if (adc_continuous_read(adc_handle, (uint8_t*) result_data, sizeof(result_data), &result_cont, timeout) ==
        ESP_OK) {
       // Result must fit into uint16_T !   
        uint16_t *result16 = (uint16_t*) dest;
        uint16_t *end = (uint16_t*) (dest+size_bytes);
        int result_count = result_cont / sizeof(adc_digi_output_data_t);
        LOGD("adc_continuous_read -> %d bytes / %d samples", result_cont, result_count);

        for (int i = 0; i < result_count; i++) {
            adc_digi_output_data_t *p = &result_data[i];
            uint16_t chan_num = AUDIO_ADC_GET_CHANNEL(p);
            uint32_t data = AUDIO_ADC_GET_DATA(p);

            if (isValidADCChannel((adc_channel_t)chan_num)){
              LOGD("Idx: %d, channel: %d, data: %u", i, chan_num, data);

              assert(result16 < end); // make sure we dont write past the end
              if (cfg.adc_calibration_active){
                int data_milliVolts;    
                auto err = adc_cali_raw_to_voltage(adc_cali_handle, data, &data_milliVolts);
                if (err == ESP_OK) {
                  // map 0 - 3300 millivolts to range from 0 to 65535 ?
                  int result_full_range = data_milliVolts;// * 19;
                  *result16 = result_full_range;
                  assert(result_full_range == (int) *result16); // check that we did not loose any bytes
                  result16++;
                  total_bytes += sizeof(uint16_t);
                } else {
                  LOGE("adc_cali_raw_to_voltage: %d", err);
                }
              } else {
                *result16 = data;
                assert(data == (uint32_t) *result16); // check that we did not loose any bytes
                result16++;
                total_bytes += sizeof(uint16_t); 
              }

            } else {
              LOGD("invalid channel: %d, data: %u", chan_num, data);
            }
        }
        // make sure that the center is at 0, so the result will be int16_t
        if (cfg.is_auto_center_read){
          auto_center.convert(dest, total_bytes);
        }

    } else {
      LOGE("adc_continuous_read");
      total_bytes = 0;
    }
    return total_bytes;
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
  ConverterAutoCenter auto_center;

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
    
    // determine conv_frame_size which must be multiple of SOC_ADC_DIGI_DATA_BYTES_PER_CONV
    uint32_t conv_frame_size = (uint32_t)cfg.buffer_size * SOC_ADC_DIGI_RESULT_BYTES;
    uint8_t calc_multiple_diff = conv_frame_size % SOC_ADC_DIGI_DATA_BYTES_PER_CONV;
    if(calc_multiple_diff != 0){
      conv_frame_size = (conv_frame_size + calc_multiple_diff);
    }
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = (uint32_t)conv_frame_size * cfg.buffer_count,
        .conv_frame_size = conv_frame_size,
    };

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
        .sample_freq_hz = (uint32_t) cfg.sample_rate,
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

    // set up optional calibration
    if (!setupADCCalibration()){
      return false;
    }

    // Start ADC
    err = adc_continuous_start(adc_handle);
    if (err != ESP_OK) {
      LOGE("adc_continuous_start unsuccessful with error: %d", err);
      return false;
    }
    
    // setup up optinal auto center which puts the avg at 0 
    auto_center.begin(cfg.channels, cfg.bits_per_sample, true);

    LOGI("adc_continuous_start successful");
    return true;
  }

  bool checkBitsPerSample() {
    // bits per sample must be multiple of 8
    // int to_be_bits = cfg.adc_bit_width;
    // int diff = to_be_bits % 8;
    // if (diff!=0){
    //   to_be_bits+=diff;
    // }
    int to_be_bits = 16; // for the time beeing we support only 16 bits!

    if (cfg.bits_per_sample == 0) {
      cfg.bits_per_sample = to_be_bits;
      LOGI("bits per sample set to: %d", cfg.bits_per_sample);
    } 
    
    if (cfg.bits_per_sample == 16) {
      LOGI("bits per sample: %d", cfg.bits_per_sample);
      return true;
    } else {
      LOGE("checking bits per sample error. It should be: %d but is %d",to_be_bits, cfg.bits_per_sample);
      return false;
    }
  }

  bool isValidADCChannel(adc_channel_t channel){
    for(int j=0; j<cfg.channels; j++){
      if (cfg.adc_channels[j] == channel) {
        return true;
      }
    }
    return false;
  } 

  bool setupADCCalibration(){
    if (!cfg.adc_calibration_active) return true;

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

    // setup calibration only when requested
    if(adc_cali_handle == NULL){
      #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        // curve fitting is preferred
        adc_cali_curve_fitting_config_t cali_config;
        cali_config.unit_id  = (adc_unit_t) unit;
        cali_config.atten    = (adc_atten_t) cfg.adc_attenuation;
        cali_config.bitwidth = (adc_bitwidth_t) cfg.adc_bit_width;
        auto err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
      #elif !defined(CONFIG_IDF_TARGET_ESP32H2)
        // line fitting
        adc_cali_line_fitting_config_t cali_config;
        cali_config.unit_id  = (adc_unit_t) unit;
        cali_config.atten    = (adc_atten_t) cfg.adc_attenuation;
        cali_config.bitwidth = (adc_bitwidth_t) cfg.adc_bit_width;
        auto err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
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
    return true;
  }

};

/// @brief AnalogAudioStream
using AnalogDriver = AnalogDriverESP32V1;

} // namespace audio_tools

#endif
