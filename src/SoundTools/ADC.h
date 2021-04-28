#pragma once

#ifdef ESP32

#include "esp_a2dp_api.h"
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "driver/adc.h"

namespace sound_tools {

const char* I2S_TAG = "ADC";
/**
 * @brief ESP32 specific configuration for i2s input via adc. The default input pin is GPIO36.
 * 
 * 
 */
template<typename T>
class ADCConfig {
  public:
    int i2s_number = 1;
    int sample_rate = 44100;
    int dma_buf_count = 5;
    int dma_buf_len = 512;
    bool use_apll = false;

    ADCConfig() = default;
    ADCConfig(const A2CConfig<T> &cfg) = default

    /// provides the current ADC pin
    int pin() {
      return pin;
    }

    /// Defines the current ADC pin
    void setPin(int gpio){
      this->pint = gpio;
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
        case 00:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO0_CHANNEL;
          break;
        case 02:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO2_CHANNEL;
          break;
        case 04:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO4_CHANNEL
          break;
        case 12:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO12_CHANNEL
          break;
        case 13:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO13_CHANNEL
          break;
        case 14:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO14_CHANNEL
          break;
        case 15:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO15_CHANNEL;
          break;
        case 25:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO25_CHANNEL;
          break;
        case 26:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO26_CHANNEL;
          break;
        case 27:
          unit = ADC_UNIT_2;
          channel = ADC2_GPIO27_CHANNEL;
          break;

        default:
          ESP_LOGE(I2S_TAG, "%s - pin GPIO%d is not supported", __func__,gpio);
      }
    }

  protected:
    adc_unit_t unit = ADC_UNIT_1;
    adc1_channel_t channel = ADC1; // GPIO36
    int pin = 36;


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

    ADCConfig<T> defaultConfig() {
        ESP_LOGD(I2S_TAG, "%s", __func__);
        I2SConfig<T> config;
        return config;
    }

    /// starts the DAC 
    void begin(ADCConfig<T> cfg) {
      i2s_num = cfg.i2s_number;

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
        ESP_LOGE(I2S_TAG, "%s - %s", __func__, "i2s_driver_install");
      }      

      i2s_zero_dma_buffer(i2s_num);

      //install and start i2s driver
      i2s_driver_install(i2s_num, &i2s_config, 4, &i2s_queue);

      //init ADC pad
      i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_7);

      // enable the ADC
      i2s_adc_enable(i2s_num);

    }

    /// stops the I2C and unistalls the driver
    void stop(){
        ESP_LOGD(I2S_TAG, "%s", __func__);
        i2s_driver_uninstall(i2s_num);    
    }

    /// Reads data from I2S
    size_t read(int16_t src[], size_t sizeFrames, TickType_t ticks_to_wait=portMAX_DELAY){
      size_t len = readBytes(src, sizeFrames * sizeof(T), ticks_to_wait); // 2 bytes * 2 channels     
      size_t result = len / (sizeof(T)); 
      ESP_LOGD(I2S_TAG, "%s - len: %d -> %d", __func__,sizeFrames, result);
      return result;
    }
    
  protected:
    i2s_port_t i2s_num;
    
    size_t readBytes(void *dest, size_t size_bytes,  TickType_t ticks_to_wait=portMAX_DELAY){
      size_t result = 0;
      if (i2s_read(i2s_num, dest, size_bytes, &result, ticks_to_wait)!=ESP_OK){
        ESP_LOGE(I2S_TAG, "%s", __func__);
      }
      return result;
    }
};

}

#endif