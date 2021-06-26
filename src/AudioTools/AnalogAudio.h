#pragma once

#include "AudioConfig.h"
#if(defined ESP32)
#include "driver/i2s.h"
#include "esp_a2dp_api.h"
//#include "freertos/queue.h"

namespace audio_tools {

typedef int16_t arrayOf2int16_t[2];

const char* ADC_TAG = "ADC";
/**
 * @brief ESP32 specific configuration for i2s input via adc. The default input pin is GPIO34. We always use int16_t values on 2 channels 
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * 
 */
class AnalogConfig {
  public:
    // allow ADC to access the protected methods
    friend class AnalogAudio;

    // public config parameters
    RxTxMode mode;
    int sample_rate = 44100;
    int dma_buf_count = I2S_BUFFER_COUNT;
    int dma_buf_len = I2S_BUFFER_SIZE;
    bool use_apll = false;
    int mode_internal; 

    AnalogConfig() {
        this->mode = RX_MODE;
        setPin(DEFAUT_ADC_PIN);
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
    }
    

    /// Default constructor
    AnalogConfig(RxTxMode mode) {
      this->mode = mode;
      if (mode == RX_MODE) {
        setPin(DEFAUT_ADC_PIN);
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
      } else {
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
      }
    }

    /// Copy constructor
    AnalogConfig(const AnalogConfig &cfg) = default;

    /// provides the current ADC pin
    int pin() {
      return adc_pin;
    }

    /// Defines the current ADC pin. The following GPIO pins are supported: GPIO32-GPIO39
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
          LOGE( "%s - pin GPIO%d is not supported", __func__,gpio);
      }
    }

  protected:
    adc_unit_t unit;
    adc1_channel_t channel;
    int adc_pin;

};


/**
 * @brief A very fast ADC and DAC using the ESP32 I2S interface.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogAudio {
  friend class AnalogAudioStream;

  public:
    /// Default constructor
    AnalogAudio() {
    }

    /// Destructor
    ~AnalogAudio() {
      end();
    }

    /// Provides the default configuration
    AnalogConfig defaultConfig(RxTxMode mode) {
        LOGD( "%s", __func__);
        AnalogConfig config(mode);
        return config;
    }

    /// starts the DAC 
    void begin(AnalogConfig cfg) {
      LOGI("%s", __func__);
      disableCore0WDT();

      adc_config = cfg;
      i2s_config_t i2s_config = {
          .mode = (i2s_mode_t) cfg.mode_internal,
          .sample_rate = cfg.sample_rate,
          .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
          .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
          .communication_format = I2S_COMM_FORMAT_I2S_LSB,
          .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
          .dma_buf_count = cfg.dma_buf_count,
          .dma_buf_len = cfg.dma_buf_len,
          .use_apll = cfg.use_apll,
          .tx_desc_auto_clear = false,
          .fixed_mclk = 0};


      // setup config
      if (i2s_driver_install(i2s_num, &i2s_config, 0, NULL)!=ESP_OK){
        LOGE( "%s - %s", __func__, "i2s_driver_install");
      }      

      // clear i2s buffer
      if (i2s_zero_dma_buffer(i2s_num)!=ESP_OK) {
        LOGE( "%s - %s", __func__, "i2s_zero_dma_buffer");
      }

      switch (cfg.mode) {
        case RX_MODE:
          //init ADC pad
          if (i2s_set_adc_mode(cfg.unit, cfg.channel)!=ESP_OK) {
            LOGE( "%s - %s", __func__, "i2s_driver_install");
          }

          // enable the ADC
          if (i2s_adc_enable(i2s_num)!=ESP_OK) {
            LOGE( "%s - %s", __func__, "i2s_adc_enable");
          }
          break;
        case TX_MODE:
          i2s_set_pin(i2s_num, NULL); 
          break;
      }
      LOGI(ADC_TAG, "%s - %s", __func__,"end");

    }

    /// stops the I2C and unistalls the driver
    void end(){
        LOGD( "%s", __func__);
        enableCore0WDT();
        i2s_driver_uninstall(i2s_num);    
    }

    /// Reads data from I2S
    size_t read(int16_t (*src)[2], size_t sizeFrames){
      size_t len = readBytes(src, sizeFrames * sizeof(int16_t)*2); // 2 bytes * 2 channels     
      size_t result = len / (sizeof(int16_t) * 2); 
      LOGD( "%s - len: %d -> %d", __func__,sizeFrames, result);
      return result;
    }

    AnalogConfig &config() {
      return adc_config;
    }
    
  protected:
    const i2s_port_t i2s_num = I2S_NUM_0; // Analog input only supports 0!
    AnalogConfig adc_config;
    
    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      size_t result = 0;            
      if (i2s_write(i2s_num, src, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
        LOGE("%s", __func__);
      }
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes){
      size_t result = 0;
      if (i2s_read(i2s_num, dest, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
        LOGE("%s", __func__);
      }
      LOGD( "%s - len: %d -> %d", __func__, size_bytes, result);
      //vTaskDelay(1);
      return result;
    }

};

}

#endif


