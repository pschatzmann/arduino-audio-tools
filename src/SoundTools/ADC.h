#pragma once

#ifdef ESP32

#include "esp_a2dp_api.h"
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "driver/adc.h"

#ifndef DEFAUT_ADC_PIN
#define DEFAUT_ADC_PIN 34
#endif

namespace sound_tools {

typedef int16_t arrayOf2int16_t[2];

const char* ADC_TAG = "ADC";
/**
 * @brief ESP32 specific configuration for i2s input via adc. The default input pin is GPIO34.
 * 
 * 
 */
class ADCConfig {
  public:
    int i2s_number = 1;
    int sample_rate = 44100;
    int dma_buf_count = 5;
    int dma_buf_len = 512;
    bool use_apll = false;

    /// Default constructor
    ADCConfig() {
      setPin(DEFAUT_ADC_PIN);
    }

    ADCConfig(const ADCConfig &cfg) = default;

    /// provides the current ADC pin
    int pin() {
      return adc_pin;
    }

    /// Defines the current ADC pin
    void setPin(int gpio){
      this->adc_pin = gpio;
      switch(gpio){
        case 32:
          unit = ADC_UNIT_1;
          channel = ADC1_GPIO32_CHANNEL;
          break;
        case 33:
          unit = ADC_UNIT_1;
          channel = ADC1_GPIO33_CHANNEL;
          break;
        case 34:
          unit = ADC_UNIT_1;
          channel = ADC1_GPIO34_CHANNEL;
          break;
        case 35:
          unit = ADC_UNIT_1;
          channel = ADC1_GPIO35_CHANNEL;
          break;
        case 36:
          unit = ADC_UNIT_1;
          channel = ADC1_GPIO36_CHANNEL;
          break;
        case 37:
          unit = ADC_UNIT_1;
          channel = ADC1_GPIO37_CHANNEL;
          break;
        case 38:
          unit = ADC_UNIT_1;
          channel = ADC1_GPIO38_CHANNEL;
          break;
        case 39:
          unit = ADC_UNIT_1;
          channel = ADC1_GPIO39_CHANNEL;
          break;

        default:
          ESP_LOGE(ADC_TAG, "%s - pin GPIO%d is not supported", __func__,gpio);
      }
    }

  protected:
    adc_unit_t unit;
    adc1_channel_t channel;
    int adc_pin;

};


/**
 * @brief A very fast Analog to Digital Converter which is using the ESP32 I2S interface
 */
class ADC {
  public:
    ADC() {
    }

    ~ADC() {
      stop();
    }

    ADCConfig defaultConfig() {
        ESP_LOGD(ADC_TAG, "%s", __func__);
        ADCConfig config;
        return config;
    }

    /// starts the DAC 
    void begin(ADCConfig cfg) {
      i2s_num = static_cast<i2s_port_t>(cfg.i2s_number);

      i2s_config_t i2s_config = {
          .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
          .sample_rate = cfg.sample_rate,
          .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
          .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
          .communication_format = I2S_COMM_FORMAT_I2S_LSB,
          .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
          .dma_buf_count = cfg.dma_buf_count,
          .dma_buf_len = cfg.dma_buf_len,
          .use_apll = cfg.use_apll,
          .tx_desc_auto_clear = false,
          .fixed_mclk = 0};


      // setup config
      if (i2s_driver_install(i2s_num, &i2s_config, 0, NULL)!=ESP_OK){
        ESP_LOGE(ADC_TAG, "%s - %s", __func__, "i2s_driver_install");
      }      

      i2s_zero_dma_buffer(i2s_num);

      //install and start i2s driver
      i2s_driver_install(i2s_num, &i2s_config, 0, NULL);

      //init ADC pad
      i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_7);

      // enable the ADC
      i2s_adc_enable(i2s_num);

    }

    /// stops the I2C and unistalls the driver
    void stop(){
        ESP_LOGD(ADC_TAG, "%s", __func__);
        i2s_driver_uninstall(i2s_num);    
    }

    /// Reads data from I2S
    size_t read(int16_t (*src)[2], size_t sizeFrames, TickType_t ticks_to_wait=portMAX_DELAY){
      size_t len = readBytes(src, sizeFrames * sizeof(int16_t)*2, ticks_to_wait); // 2 bytes * 2 channels     
      size_t result = len / (sizeof(int16_t) * 2); 
      ESP_LOGD(ADC_TAG, "%s - len: %d -> %d", __func__,sizeFrames, result);
      return result;
    }
    
  protected:
    i2s_port_t i2s_num;
    
    size_t readBytes(void *dest, size_t size_bytes,  TickType_t ticks_to_wait=portMAX_DELAY){
      size_t result = 0;
      if (i2s_read(i2s_num, dest, size_bytes, &result, ticks_to_wait)!=ESP_OK){
        ESP_LOGE(ADC_TAGƒ, "%s", __func__);
      }
      return result;
    }
};

}

#endif