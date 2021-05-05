#pragma once

#ifdef ESP32

#include "driver/i2s.h"
#include "esp_a2dp_api.h"

namespace audio_tools {

/**
 * @brief Basic I2S API - for the ESP32
 * 
 */
class I2SBase {
  friend class I2SStream;

  public:

    /// Provides the default configuration
    I2SConfig defaultConfig(RxTxMode mode) {
        ESP_LOGD(I2S_TAG, "%s", __func__);
        I2SConfig c(mode);
        return c;
    }

    /// starts the DAC 
    void begin(I2SConfig cfg) {
      ESP_LOGD(I2S_TAG, "%s", __func__);
      this->cfg = cfg;
      this->i2s_num = (i2s_port_t) cfg.port_no; 
      setChannels(cfg.channels);

      i2s_config_t i2s_config_new = {
            .mode = (i2s_mode_t) ((cfg.rx_tx_mode == TX_MODE) ? (I2S_MODE_MASTER | I2S_MODE_TX) :  (I2S_MODE_MASTER | I2S_MODE_RX)) ,
            .sample_rate = cfg.sample_rate,
            .bits_per_sample = (i2s_bits_per_sample_t) cfg.bits_per_sample,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // default interrupt priority
            .dma_buf_count = I2S_BUFFER_COUNT,
            .dma_buf_len = I2S_BUFFER_SIZE,
            .use_apll = I2S_USE_APLL,
      };
      i2s_config = i2s_config_new;
      logConfig();
           
      // We make sure that we can reconfigure
      if (is_started) {
          end();
          ESP_LOGD(I2S_TAG, "%s", "I2S restarting");
      }


      // setup config
      if (i2s_driver_install(i2s_num, &i2s_config, 0, NULL)!=ESP_OK){
        ESP_LOGE(I2S_TAG, "%s - %s", __func__, "i2s_driver_install");
      }      

      // setup pin config
      if (this->cfg.is_digital ) {
        i2s_pin_config_t pin_config = {
            .bck_io_num = cfg.pin_bck,
            .ws_io_num = cfg.pin_ws,
            .data_out_num = cfg.rx_tx_mode == TX_MODE ? cfg.pin_data : I2S_PIN_NO_CHANGE,
            .data_in_num = cfg.rx_tx_mode == RX_MODE ? cfg.pin_data : I2S_PIN_NO_CHANGE
        };
        logConfigPins(pin_config);

        if (i2s_set_pin(i2s_num, &pin_config)!= ESP_OK){
            ESP_LOGE(I2S_TAG, "%s - %s", __func__, "i2s_set_pin");
        }
      } else {
          ESP_LOGD(I2S_TAG, "Using built in DAC");
          //for internal DAC, this will enable both of the internal channels
          i2s_set_pin(i2s_num, NULL); 
      }

      // clear initial buffer
      i2s_zero_dma_buffer(i2s_num);

      is_started = true;
      ESP_LOGD(I2S_TAG, "%s - %s", __func__, "started");
    }

    /// stops the I2C and unistalls the driver
    void end(){
        ESP_LOGD(I2S_TAG, "%s", __func__);
        i2s_driver_uninstall(i2s_num);   
        is_started = false; 
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

  protected:
    I2SConfig cfg;
    i2s_port_t i2s_num;
    i2s_config_t i2s_config;
    bool is_started = false;

    // update the cfg.i2s.channel_format based on the number of channels
    void setChannels(int channels){
          if (channels==2){
             i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
          } else if (channels==1){
             i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
          }
          cfg.channels = channels;
          
    }
    
    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      size_t result = 0;            
      if (i2s_write(i2s_num, src, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
        ESP_LOGE(I2S_TAG, "%s", __func__);
      }
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes){
      size_t result = 0;
      if (i2s_read(i2s_num, dest, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
        ESP_LOGE(I2S_TAG, "%s", __func__);
      }
      return result;
    }

    void logConfig() {
      ESP_LOGD(I2S_TAG, "mode: %s", cfg.rx_tx_mode == TX_MODE ? "TX":"RX");
      ESP_LOGD(I2S_TAG, "sample rate: %d", cfg.sample_rate);
      ESP_LOGD(I2S_TAG, "bits per sample: %d", cfg.bits_per_sample);
      ESP_LOGD(I2S_TAG, "number of channels: %d", cfg.channels);
    }

    void logConfigPins(i2s_pin_config_t pin_config){
      ESP_LOGD(I2S_TAG, "pin bck_io_num: %d", cfg.pin_bck);
      ESP_LOGD(I2S_TAG, "pin ws_io_num: %d", cfg.pin_ws);
      ESP_LOGD(I2S_TAG, "pin data_num: %d", cfg.pin_data);
    }

};

}

#endif