#pragma once
#include "AudioConfig.h"
#if defined(USE_ANALOG) 
#if defined(ESP32) 
#  if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
#   include "driver/i2s.h"
#   include "driver/adc.h"
#   include "soc/dac_channel.h"
#   include "soc/adc_channel.h"
#  else
#   include "esp_adc/adc_continuous.h"
#  endif
#endif

#if CONFIG_IDF_TARGET_ESP32
#define ADC_CONV_MODE       ADC_CONV_SINGLE_UNIT_1  //ESP32 only supports ADC1 DMA mode
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE1
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_CONV_MODE       ADC_CONV_BOTH_UNIT
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C2
#define ADC_CONV_MODE       ADC_CONV_ALTER_UNIT     //ESP32C3 only supports alter mode
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_CONV_MODE       ADC_CONV_BOTH_UNIT
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2
#endif



namespace audio_tools {

/**
 * @brief ESP32 specific configuration for i2s input via adc. The default input pin is GPIO34. We always use int16_t values. The default
 * output pins are GPIO25 and GPIO26! 
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogConfig : public AudioInfo {
  public:
    int buffer_count = PWM_BUFFER_COUNT;
    int buffer_size = PWM_BUFFER_SIZE;
    RxTxMode rx_tx_mode;
    bool is_blocking_write = true;
    bool is_auto_center_read = true;

#if defined(ESP32) && defined(USE_ANALOG) 
    // allow ADC to access the protected methods
    friend class AnalogDriverESP32;
    bool use_apll = false;

    // public config parameters
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
    int port_no = I2S_NUM_0; // Analog input and output only supports 0!
    bool auto_clear = I2S_AUTO_CLEAR;
    bool uninstall_driver_on_end = true;
    int mode_internal; 
    int adc_pin;
#else
    int adc_conversion_mode = ADC_CONV_MODE;
    int adc_output_type = ADC_OUTPUT_TYPE;
    int adc_attenuation = ADC_ATTEN_DB_0;
    int adc_bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    #if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C2
    adc_channel_t channel[3] = {ADC_CHANNEL_2, ADC_CHANNEL_3, (ADC_CHANNEL_0 | 1 << 3)};
    #endif
    #if CONFIG_IDF_TARGET_ESP32S2
    adc_channel_t channel[3] = {ADC_CHANNEL_2, ADC_CHANNEL_3, (ADC_CHANNEL_0 | 1 << 3)};
    #endif  
    #if CONFIG_IDF_TARGET_ESP32
    adc_channel_t channel[1] = {ADC_CHANNEL_7};
    #endif
#endif

    /// Default constructor
    AnalogConfig(RxTxMode rxtxMode=TX_MODE) {
      sample_rate = 44100;
      bits_per_sample = 16;
      channels = 2;
      rx_tx_mode = rxtxMode;
      if (rx_tx_mode == RX_MODE) {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
        adc_pin = PIN_ADC1;
        auto_clear = false;
#endif
        LOGI("I2S_MODE_ADC_BUILT_IN");
      } else {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
#endif
        LOGI("I2S_MODE_DAC_BUILT_IN");
      }
    }

    /// Copy constructor
    AnalogConfig(const AnalogConfig &cfg) = default;

    void logInfo() {
      AudioInfo::logInfo();
      if (rx_tx_mode == TX_MODE){
        LOGI("analog left output pin: %d", 25);
        LOGI("analog right output pin: %d", 26);
      } 
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
    /// Defines an alternative input pin (for the left channel)
    void setInputPin1(int pin){
      this->adc_pin = pin;
    }
#endif

#else 

    AnalogConfig() {
        sample_rate = 44100;
        bits_per_sample = 16;
        channels = 2;
        buffer_size = ANALOG_BUFFER_SIZE;
        buffer_count = ANALOG_BUFFERS;
        rx_tx_mode = RX_MODE;
    }
    /// Default constructor
    AnalogConfig(RxTxMode rxtxMode) : AnalogConfig() {
      rx_tx_mode = rxtxMode;
    }
    int start_pin = PIN_ANALOG_START;

#endif
  
};

class AnalogDriverBase {
public:
    virtual bool begin(AnalogConfig cfg) = 0;
    virtual void end() = 0;
    virtual size_t write(const uint8_t *src, size_t size_bytes) { return 0;}
    virtual size_t readBytes(uint8_t *dest, size_t size_bytes) = 0;
    virtual int available() = 0;
    virtual int availableForWrite() { return DEFAULT_BUFFER_SIZE; }
};

} // ns
#endif
