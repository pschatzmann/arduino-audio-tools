#pragma once

#include "AudioConfig.h"
#if defined(USE_ANALOG) && defined(ESP32) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0 , 0) || defined(DOXYGEN)

#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali_scheme.h"
#ifdef ARDUINO
#  include "esp32-hal-periman.h"
#endif

#if CONFIG_IDF_TARGET_ESP32
#  define ADC_CONV_MODE       ADC_CONV_SINGLE_UNIT_1
#  define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE1
#  define ADC_CHANNELS        {ADC_CHANNEL_6, ADC_CHANNEL_7}
#  define AUDIO_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#  define AUDIO_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#  define HAS_ESP32_DAC
#elif CONFIG_IDF_TARGET_ESP32S2
#  define ADC_CONV_MODE       ADC_CONV_SINGLE_UNIT_1
#  define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2
#  define AUDIO_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#  define AUDIO_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#  define ADC_CHANNELS        {ADC_CHANNEL_2, ADC_CHANNEL_3}
#  define HAS_ESP32_DAC
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C6
#  define ADC_CONV_MODE       ADC_CONV_ALTER_UNIT     //ESP32C3 only supports alter mode
#  define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2
#  define AUDIO_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#  define AUDIO_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#  define ADC_CHANNELS        {ADC_CHANNEL_2, ADC_CHANNEL_3}
#elif CONFIG_IDF_TARGET_ESP32S3
#  define ADC_CONV_MODE       ADC_CONV_SINGLE_UNIT_1
#  define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2
#  define AUDIO_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#  define AUDIO_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#  define ADC_CHANNELS        {ADC_CHANNEL_2, ADC_CHANNEL_3} // or channel 4 & 5
#endif

// #define GET_ADC_UNIT_FROM_CHANNEL(x) ((x >> 3) & 0x1)
#  define ADC_UNIT            ADC_UNIT_1 // continuous ADC API only supports ADC1

#ifdef HAS_ESP32_DAC
#  include "driver/dac_continuous.h"
#endif

namespace audio_tools {

/**
 * @brief ESP32 specific configuration for i2s input via adc using the 
 * adc_continuous API
 * 
 * @author Phil Schatzmann
 * @ingroup platform
 * @copyright GPLv3
 */
class AnalogConfigESP32V1 : public AudioInfo {
    // allow ADC to access the protected methods
    friend class AnalogDriverESP32;
  public:
    int buffer_count = ANALOG_BUFFER_COUNT;
    int buffer_size = ANALOG_BUFFER_SIZE;
    RxTxMode rx_tx_mode;
    TickType_t timeout = portMAX_DELAY;

#ifdef HAS_ESP32_DAC
    bool is_blocking_write = true;
    bool use_apll = false;
    /// ESP32: DAC_CHANNEL_MASK_CH0 or DAC_CHANNEL_MASK_CH1
    dac_channel_mask_t dac_mono_channel = DAC_CHANNEL_MASK_CH0;
#endif

    // ADC config parameters
    bool adc_calibration_active = false;
    bool is_auto_center_read = false;
    adc_digi_convert_mode_t adc_conversion_mode = ADC_CONV_MODE;
    adc_digi_output_format_t adc_output_type = ADC_OUTPUT_TYPE;
    uint8_t adc_attenuation = ADC_ATTEN_DB_12; // full voltage range of 3.9V
    uint8_t adc_bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    /// ESP32: ADC_CHANNEL_6, ADC_CHANNEL_7; others ADC_CHANNEL_2, ADC_CHANNEL_3
    adc_channel_t adc_channels[2] = ADC_CHANNELS;

    /// Default constructor
    AnalogConfigESP32V1(RxTxMode rxtxMode=TX_MODE) {
      sample_rate = 44100;
      channels =  2;
      bits_per_sample = 16;
      rx_tx_mode = rxtxMode;
      if (rx_tx_mode == RX_MODE) {
        // make sure that the proposed sample rate is valid
        if (sample_rate*channels>SOC_ADC_SAMPLE_FREQ_THRES_HIGH){
          sample_rate = SOC_ADC_SAMPLE_FREQ_THRES_HIGH / channels;
        }
        LOGI("I2S_MODE_ADC_BUILT_IN");
      } else {
        LOGI("I2S_MODE_DAC_BUILT_IN");
      }
    }

    /// Copy constructor
    AnalogConfigESP32V1(const AnalogConfigESP32V1 &cfg) = default;

    void logInfo() {
      AudioInfo::logInfo();
      if (rx_tx_mode == TX_MODE){
        LOGI("analog left output pin: %d", 25);
        LOGI("analog right output pin: %d", 26);
      } 
    }
};

using AnalogConfig = AnalogConfigESP32V1;


} // ns
#endif
