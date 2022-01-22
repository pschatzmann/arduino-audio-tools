#pragma once

#ifdef ESP32

#include "AudioConfig.h"
#include "AudioI2S/I2SConfig.h"

#include "driver/i2s.h"
#include "esp_a2dp_api.h"
#include "esp_system.h"

namespace audio_tools {

/**
 * @brief Basic I2S API - for the ESP32. If we receive 1 channel, we expand the result to 2 channels.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SBase {

  friend class AnalogAudio;
  friend class AudioKitStream;

  public:

    /// Provides the default configuration
    I2SConfig defaultConfig(RxTxMode mode) {
        I2SConfig c(mode);
        return c;
    }

    /// starts the DAC with the default config
    void begin(RxTxMode mode = TX_MODE) {
        begin(defaultConfig(mode));
    }


    /// starts the DAC
    void begin(I2SConfig cfg) {
      LOGD(LOG_METHOD);
      int txPin = cfg.rx_tx_mode == TX_MODE ? cfg.pin_data : I2S_PIN_NO_CHANGE;
      int rxPin = cfg.rx_tx_mode == RX_MODE ? cfg.pin_data : I2S_PIN_NO_CHANGE;
      begin(cfg, txPin, rxPin);
    }

    /// we assume the data is already available in the buffer
    int available() {
      return I2S_BUFFER_COUNT*I2S_BUFFER_SIZE;
    }

    /// We limit the write size to the buffer size
    int availableForWrite() {
      return I2S_BUFFER_COUNT*I2S_BUFFER_SIZE;
    }

    /// stops the I2C and unistalls the driver
    void end(){
        LOGD(LOG_METHOD);
        i2s_driver_uninstall(i2s_num);
        is_started = false;
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      LOGD(LOG_METHOD);

      size_t result = 0;
      if (cfg.channels==2){
        if (i2s_write(i2s_num, src, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
          LOGE(LOG_METHOD);
        }
        LOGD("i2s_write %d -> %d bytes", size_bytes, result);
      } else {
        result = I2SBase::writeExpandChannel(i2s_num, cfg.bits_per_sample, src, size_bytes);
      }
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes){
      size_t result = 0;
      if (i2s_read(i2s_num, dest, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
        LOGE(LOG_METHOD);
      }
      return result;
    }


  protected:
    I2SConfig cfg;
    i2s_port_t i2s_num;
    i2s_config_t i2s_config;
    bool is_started = false;

    /// starts the DAC
    void begin(I2SConfig cfg, int txPin, int rxPin) {
      LOGD(LOG_METHOD);
      cfg.logInfo();
      this->cfg = cfg;
      this->i2s_num = (i2s_port_t) cfg.port_no;
      setChannels(cfg.channels);

      i2s_config_t i2s_config_new = {
            .mode = toMode(cfg),
            .sample_rate = (eps32_i2s_sample_rate_type)cfg.sample_rate,
            .bits_per_sample = (i2s_bits_per_sample_t) cfg.bits_per_sample,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = toCommFormat(cfg.i2s_format),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // default interrupt priority
            .dma_buf_count = I2S_BUFFER_COUNT,
            .dma_buf_len = I2S_BUFFER_SIZE,
            .use_apll = (bool) cfg.use_apll,
            .tx_desc_auto_clear = I2S_AUTO_CLEAR,
            .fixed_mclk = (int) (cfg.use_apll ? cfg.fixed_mclk : 0 )

      };
      i2s_config = i2s_config_new;

      // We make sure that we can reconfigure
      if (is_started) {
          end();
          LOGD("%s", "I2S restarting");
      }

      // setup config
      if (i2s_driver_install(i2s_num, &i2s_config, 0, NULL)!=ESP_OK){
        LOGE("%s - %s", __func__, "i2s_driver_install");
      }

      // setup pin config
      if (this->cfg.is_digital ) {
        i2s_pin_config_t pin_config = {
#if ESP_IDF_VERSION_MAJOR >= 4
            .mck_io_num = cfg.pin_mck,
#endif
            .bck_io_num = cfg.pin_bck,
            .ws_io_num = cfg.pin_ws,
            .data_out_num = txPin,
            .data_in_num = rxPin
        };

        if (i2s_set_pin(i2s_num, &pin_config)!= ESP_OK){
            LOGE("%s - %s", __func__, "i2s_set_pin");
        }
      } else {
          LOGD("Using built in DAC");
          //for internal DAC, this will enable both of the internal channels
          i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
      }

      // clear initial buffer
      i2s_zero_dma_buffer(i2s_num);

      is_started = true;
      LOGD("%s - %s", __func__, "started");
    }

    // update the cfg.i2s.channel_format based on the number of channels
    void setChannels(int channels){
        cfg.channels = channels;
    }


    /// writes the data by making shure that we send 2 channels
    static size_t writeExpandChannel(i2s_port_t i2s_num, const int bits_per_sample, const void *src, size_t size_bytes){
        size_t result = 0;
        int j;
        switch(bits_per_sample){

          case 8:
            for (j=0;j<size_bytes;j++){
              int8_t frame[2];
              int8_t *data = (int8_t *)src;
              frame[0]=data[j];
              frame[1]=data[j];
              size_t result_call = 0;
              if (i2s_write(i2s_num, frame, sizeof(int8_t)*2, &result_call, portMAX_DELAY)!=ESP_OK){
                LOGE(LOG_METHOD);
              } else {
                result += result_call;
              }
            }
            break;

          case 16:
            for (j=0;j<size_bytes/2;j++){
              int16_t frame[2];
              int16_t *data = (int16_t*)src;
              frame[0]=data[j];
              frame[1]=data[j];
              size_t result_call = 0;
              if (i2s_write(i2s_num, frame, sizeof(int16_t)*2, &result_call, portMAX_DELAY)!=ESP_OK){
                LOGE(LOG_METHOD);
              } else {
                result += result_call;
              }
            }
            break;

          case 24:
            for (j=0;j<size_bytes/4;j++){
              int24_t frame[2];
              int24_t *data = (int24_t*) src;
              frame[0]=data[j];
              frame[1]=data[j];
              size_t result_call = 0;
              if (i2s_write(i2s_num, frame, sizeof(int24_t)*2, &result_call, portMAX_DELAY)!=ESP_OK){
                LOGE(LOG_METHOD);
              } else {
                result += result_call;
              }
            }
            break;

          case 32:
            for (j=0;j<size_bytes/4;j++){
              int32_t frame[2];
              int32_t *data = (int32_t*) src;
              frame[0]=data[j];
              frame[1]=data[j];
              size_t result_call = 0;
              if (i2s_write(i2s_num, frame, sizeof(int32_t)*2, &result_call, portMAX_DELAY)!=ESP_OK){
                LOGE(LOG_METHOD);
              } else {
                result += result_call;
              }
            }
            break;
        }
        return result;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    // determines the i2s_comm_format_t - by default we use I2S_COMM_FORMAT_STAND_I2S
    i2s_comm_format_t toCommFormat(I2SFormat mode){

        switch(mode){
          case I2S_PHILIPS_FORMAT:
          case I2S_STD_FORMAT:
            return (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_I2S;
          case I2S_LEFT_JUSTIFIED_FORMAT:
          case I2S_MSB_FORMAT:
            return (i2s_comm_format_t) I2S_COMM_FORMAT_I2S_MSB;
          case I2S_RIGHT_JUSTIFIED_FORMAT:
          case I2S_LSB_FORMAT:
            return (i2s_comm_format_t) I2S_COMM_FORMAT_I2S_LSB;
          // this is strange but the doc specifies that
          // case I2S_PCM_LONG:
          //   return (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_PCM_LONG;
          // case I2S_PCM_SHORT:
          //   return (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_PCM_SHORT;

          default:
            LOGE("unsupported mode");
            return (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_I2S;
        }
    }
#pragma GCC diagnostic pop

    // determines the i2s_format_t
    i2s_mode_t toMode(I2SConfig &cfg) {
      i2s_mode_t mode;
      if (cfg.is_digital) {
        int i2s_format = cfg.is_master ? I2S_MODE_MASTER : I2S_MODE_SLAVE;
        int rx_tx = cfg.rx_tx_mode == TX_MODE ? I2S_MODE_TX : I2S_MODE_RX;
        mode = (i2s_mode_t)(i2s_format | rx_tx);
      } else {
        int i2s_format = cfg.is_master ? I2S_MODE_MASTER : I2S_MODE_SLAVE;
        int rx_tx = cfg.rx_tx_mode == TX_MODE ? (I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN) : (I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
        mode = (i2s_mode_t)(i2s_format | rx_tx);
      }
      return mode;
    }
};

}

#endif
