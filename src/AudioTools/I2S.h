#pragma once

#ifdef ESP32

#include "esp_a2dp_api.h"
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "SoundTypes.h"

namespace audio_tools {

enum I2SMode  { TX_MODE, RX_MODE };
const char* I2S_TAG = "I2S";
/**
 * @brief ESP32 specific configuration for all i2s settings
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template<typename T>
class I2SConfig {
  public:
    i2s_port_t port_no = I2S_NUM_0;
    i2s_config_t i2s;
    i2s_pin_config_t pin;
    int channels = 2;

    I2SConfig() {
        i2s = defaultConfig(TX_MODE);
        pin = defaultPinConfig(TX_MODE);
    }

    /// Default Constructor
    I2SConfig(I2SMode mode) {
        i2s = defaultConfig(mode);
        pin = defaultPinConfig(mode);
    }

    /// Copy constructor
    I2SConfig(const I2SConfig<T> &cfg) {
      port_no = cfg.port_no;
      i2s = cfg.i2s;
      pin = cfg.pin;
    }


  protected:
      i2s_config_t defaultConfig(I2SMode mode) {
      ESP_LOGD(I2S_TAG, "%s", __func__);
      i2s_config_t config = {
            .mode = (i2s_mode_t) ((mode == TX_MODE) ? (I2S_MODE_MASTER | I2S_MODE_TX) :  (I2S_MODE_MASTER | I2S_MODE_RX)) ,
            .sample_rate = 44100,
            .bits_per_sample = (i2s_bits_per_sample_t) (sizeof(T) * 8),
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // default interrupt priority
            .dma_buf_count = 8,
            .dma_buf_len = 1024,
            .use_apll = false,
        };
        return config;
    }

    i2s_pin_config_t defaultPinConfig(I2SMode mode = TX_MODE) {
        ESP_LOGD(I2S_TAG, "%s - mode: %s", __func__, mode==TX_MODE ? "TX" : "RX");
        i2s_pin_config_t config = {
            .bck_io_num = 14,
            .ws_io_num = 15,
            .data_out_num = mode == TX_MODE ? 22 : I2S_PIN_NO_CHANGE,
            .data_in_num = mode == RX_MODE ? 32 : I2S_PIN_NO_CHANGE
        };
       return config;
    }
};


/**
 * @brief A Simple I2S interface class.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template<typename T>
class I2S : public AudioBaseInfoDependent {
  friend class I2SStream;

  public:
    
    /// Default Constructor
    I2S() {
    }

    /// Destructor
    ~I2S() {
      stop();
    }

    /// Provides the default configuration
    I2SConfig<T> defaultConfig(I2SMode mode) {
        ESP_LOGD(I2S_TAG, "%s", __func__);
        I2SConfig<T> c(mode);
        return c;
    }

    /// starts the DAC 
    void begin(I2SConfig<T> cfg) {
      ESP_LOGD(I2S_TAG, "%s", __func__);
      this->cfg = cfg;
      this->i2s_num =  cfg.port_no;            

      // We make sure that we can reconfigure
      if (is_started) {
          stop();
          ESP_LOGD(I2S_TAG, "%s", "I2S restarting");
      }

      ESP_LOGD(I2S_TAG, "sample rate: %d", cfg.i2s.sample_rate);
      ESP_LOGD(I2S_TAG, "bits per sample: %d", cfg.i2s.bits_per_sample);
      ESP_LOGD(I2S_TAG, "number of channels: %d", cfg.channels);

      // setup config
      if (i2s_driver_install(i2s_num, &cfg.i2s, 0, NULL)!=ESP_OK){
        ESP_LOGE(I2S_TAG, "%s - %s", __func__, "i2s_driver_install");
      }      

      // setup pin config
      if (this->cfg.i2s.mode & I2S_MODE_DAC_BUILT_IN ) {
          ESP_LOGD(I2S_TAG, "Using built in DAC");
          //for internal DAC, this will enable both of the internal channels
          i2s_set_pin(i2s_num, NULL); 
      } else {
        if (i2s_set_pin(i2s_num, &cfg.pin)!= ESP_OK){
          ESP_LOGD(I2S_TAG, "pin bck_io_num: %d", cfg.pin.bck_io_num);
          ESP_LOGD(I2S_TAG, "pin ws_io_num: %d", cfg.pin.ws_io_num);
          ESP_LOGD(I2S_TAG, "pin data_out_num: %d", cfg.pin.data_out_num);
          ESP_LOGD(I2S_TAG, "pin data_in_num: %d", cfg.pin.data_in_num);
          ESP_LOGE(I2S_TAG, "%s - %s", __func__, "i2s_set_pin");
        }
      }

      // clear initial buffer
      i2s_zero_dma_buffer(i2s_num);

      is_started = true;
    }

    /// stops the I2C and unistalls the driver
    void stop(){
        ESP_LOGD(I2S_TAG, "%s", __func__);
        i2s_driver_uninstall(i2s_num);   
        is_started = false; 
    }

    /// writes the data to the I2S interface
    size_t write(T (*src)[2], size_t sizeFrames, TickType_t ticks_to_wait=portMAX_DELAY){
      ESP_LOGD(I2S_TAG, "%s", __func__);
      return writeBytes(src, sizeFrames * sizeof(T) * 2, ticks_to_wait); // 2 bytes * 2 channels      
    }
    
    /// Reads data from I2S
    size_t read(T (*src)[2], size_t sizeFrames, TickType_t ticks_to_wait=portMAX_DELAY){
      size_t len = readBytes(src, sizeFrames * sizeof(T) * 2, ticks_to_wait); // 2 bytes * 2 channels     
      size_t result = len / (sizeof(T) * 2); 
      ESP_LOGD(I2S_TAG, "%s - len: %d -> %d", __func__,sizeFrames, result);
      return result;
    }
    
    /// provides the actual configuration
    I2SConfig<T> config() {
      return cfg;
    }

    /// updates the sample rate dynamically 
    virtual void setAudioBaseInfo(AudioBaseInfo info) {
      bool is_update = false;

      if (cfg.i2s.sample_rate != info.sample_rate
        || cfg.i2s.bits_per_sample != info.bits_per_sample) {
        cfg.i2s.sample_rate = info.sample_rate;
        cfg.i2s.bits_per_sample = static_cast<i2s_bits_per_sample_t>(info.bits_per_sample);
        is_update = true;
      }

      if (cfg.channels != info.channels){
          if (info.channels==2){
             cfg.i2s.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
          } else if (info.channels==1){
             cfg.i2s.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
          }
          cfg.channels = info.channels;
          is_update = true;
      }

      if (is_update){
        // restart
        begin(config());        
      }

    }

  protected:
    I2SConfig<T> cfg;
    i2s_port_t i2s_num;
    bool is_started = false;
    
    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes, TickType_t ticks_to_wait=portMAX_DELAY){
      size_t result = 0;            
      if (i2s_write(i2s_num, src, size_bytes, &result, ticks_to_wait)!=ESP_OK){
        ESP_LOGE(I2S_TAG, "%s", __func__);
      }
      return result;
    }

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