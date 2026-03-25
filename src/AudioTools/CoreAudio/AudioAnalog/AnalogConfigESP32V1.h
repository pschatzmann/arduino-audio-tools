#pragma once

#include "AudioToolsConfig.h"
#if (defined(ESP32) && defined(USE_ANALOG) && !USE_LEGACY_I2S) || defined(DOXYGEN)

#include "AudioTools/CoreAudio/AudioTypes.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#ifdef ARDUINO
#include "esp32-hal-periman.h"
#endif

#if CONFIG_IDF_TARGET_ESP32
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define ADC_CHANNELS                            \
  {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_4, \
   ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7}
#define NUM_ADC_CHANNELS 6
#define AUDIO_ADC_GET_CHANNEL(p_data) ((p_data)->type1.channel)
#define AUDIO_ADC_GET_DATA(p_data) ((p_data)->type1.data)
#define HAS_ESP32_DAC
#define ADC_CHANNEL_TYPE uint16_t
#define ADC_DATA_TYPE uint16_t
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define AUDIO_ADC_GET_CHANNEL(p_data) ((p_data)->type2.channel)
#define AUDIO_ADC_GET_DATA(p_data) ((p_data)->type2.data)
#define ADC_CHANNELS                                                          \
  {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4, \
   ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_8, ADC_CHANNEL_9}
#define NUM_ADC_CHANNELS 10
#define HAS_ESP32_DAC
#define ADC_CHANNEL_TYPE uint16_t
#define ADC_DATA_TYPE uint16_t
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2 || \
    CONFIG_IDF_TARGET_ESP32C2 
#define ADC_CONV_MODE ADC_CONV_ALTER_UNIT  // ESP32C3 only supports alter mode
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define AUDIO_ADC_GET_CHANNEL(p_data) ((p_data)->type2.channel)
#define AUDIO_ADC_GET_DATA(p_data) ((p_data)->type2.data)
#define ADC_CHANNELS \
  {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4}
#define NUM_ADC_CHANNELS 5
#define ADC_CHANNEL_TYPE uint32_t
#define ADC_DATA_TYPE uint32_t
#elif CONFIG_IDF_TARGET_ESP32C5 || CONFIG_IDF_TARGET_ESP32C6
#define ADC_CONV_MODE ADC_CONV_ALTER_UNIT
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define AUDIO_ADC_GET_CHANNEL(p_data) ((p_data)->type2.channel)
#define AUDIO_ADC_GET_DATA(p_data) ((p_data)->type2.data)
#define ADC_CHANNELS                                           \
  {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, \
   ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6}
#define NUM_ADC_CHANNELS 7
#define ADC_CHANNEL_TYPE uint32_t
#define ADC_DATA_TYPE uint32_t
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define AUDIO_ADC_GET_CHANNEL(p_data) ((p_data)->type2.channel)
#define AUDIO_ADC_GET_DATA(p_data) ((p_data)->type2.data)
#define ADC_CHANNELS                                                          \
  {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4, \
   ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_8, ADC_CHANNEL_9}
#define NUM_ADC_CHANNELS 10
#define ADC_CHANNEL_TYPE uint32_t
#define ADC_DATA_TYPE uint32_t
#elif CONFIG_IDF_TARGET_ESP32P4
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define AUDIO_ADC_GET_CHANNEL(p_data) ((p_data)->type2.channel)
#define AUDIO_ADC_GET_DATA(p_data) ((p_data)->type2.data)
#define ADC_CHANNELS                                           \
  {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, \
   ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7}
#define NUM_ADC_CHANNELS 8
#define ADC_CHANNEL_TYPE uint32_t
#define ADC_DATA_TYPE uint32_t
#endif

// continuous ADC API should run on ADC1

#define ADC_UNIT ADC_UNIT_1
#if defined(HAS_ESP32_DAC) && !USE_LEGACY_I2S
#include "driver/dac_continuous.h"
#endif

namespace audio_tools {

#if defined(SOC_ADC_DIGI_DATA_BYTES_PER_CONV) && \
    defined(SOC_ADC_DIGI_RESULT_BYTES)
#if defined(SOC_ADC_DIGI_MAX_BYTES_PER_CONV_FRAME)
constexpr uint32_t ADC_CONTINUOUS_RX_MAX_CONV_FRAME_BYTES =
    SOC_ADC_DIGI_MAX_BYTES_PER_CONV_FRAME;
#else
constexpr uint32_t ADC_CONTINUOUS_RX_MAX_CONV_FRAME_BYTES = 4092;
#endif

inline uint32_t adcContinuousAlignFrameSize(uint32_t frame_size_bytes) {
  uint32_t alignment = SOC_ADC_DIGI_DATA_BYTES_PER_CONV;
  if (alignment == 0) return frame_size_bytes;
  uint32_t remainder = frame_size_bytes % alignment;
  if (remainder == 0) return frame_size_bytes;
  return frame_size_bytes + (alignment - remainder);
}

inline uint32_t adcContinuousMaxConvFrameBytes() {
  return ADC_CONTINUOUS_RX_MAX_CONV_FRAME_BYTES;
}

inline uint32_t adcContinuousMaxResultsPerFrame() {
  return adcContinuousMaxConvFrameBytes() / SOC_ADC_DIGI_RESULT_BYTES;
}
#endif

/**
 * @brief ESP32 specific configuration for i2s input via adc using the
 * adc_continuous API
 * @author Phil Schatzmann
 * @ingroup platform
 * @copyright GPLv3
 */
class AnalogConfigESP32V1 : public AudioInfo {
  // allow ADC to access the protected methods
  friend class AnalogDriverESP32;

 public:
  // RX: number of DMA conversion frames retained in the driver's internal pool.
  // This does not change the size of a single application read block.
  int buffer_count = ANALOG_BUFFER_COUNT;
  // RX: number of ADC conversion results per DMA conversion frame. This is not
  // the application measurement block size. With N channels, one frame holds
  // buffer_size / N samples per channel.
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
  uint8_t adc_attenuation = ADC_ATTEN_DB_12;  // full voltage range of 3.9V
  uint8_t adc_bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

  adc_unit_t adc_unit = ADC_UNIT;
  adc_channel_t adc_channels[NUM_ADC_CHANNELS] = ADC_CHANNELS;

  /// Default constructor
  AnalogConfigESP32V1(RxTxMode rxtxMode = TX_MODE) {
    sample_rate = 44100;
    channels = 2;
    bits_per_sample = 16;
    rx_tx_mode = rxtxMode;
    switch (rx_tx_mode) {
      case RX_MODE: {
        // make sure that the proposed sample rate is valid
        if (sample_rate * channels > SOC_ADC_SAMPLE_FREQ_THRES_HIGH) {
          sample_rate = SOC_ADC_SAMPLE_FREQ_THRES_HIGH / channels;
        }
        LOGI("I2S_MODE_ADC_BUILT_IN");
        break;
      }
#ifdef HAS_ESP32_DAC
      case TX_MODE:
        use_apll = true;
        LOGI("I2S_MODE_DAC_BUILT_IN");
        break;
#endif

      default:
        LOGE("RxTxMode not supported: %d", rx_tx_mode);
        break;
    }
  }

  /// Copy constructor
  AnalogConfigESP32V1(const AnalogConfigESP32V1 &cfg) = default;

  void logInfo() {
    AudioInfo::logInfo();
#if !defined(ESP32X)
    if (rx_tx_mode == TX_MODE) {
      LOGI("analog left output pin: %d", 25);
      LOGI("analog right output pin: %d", 26);
    }
#endif
#ifdef HAS_ESP32_DAC
    LOGI("use_apll: %d", use_apll);
#endif
  }
};

#ifndef ANALOG_CONFIG
#define ANALOG_CONFIG
using AnalogConfig = AnalogConfigESP32V1;
#endif

}  // namespace audio_tools

#endif
