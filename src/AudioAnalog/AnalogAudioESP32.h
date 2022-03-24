#pragma once

#if defined(ESP32) && !defined(ARDUINO_ESP32S2_DEV) && !defined(ARDUINO_ESP32C3_DEV)
#include "AudioConfig.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "soc/dac_channel.h"
#include "soc/adc_channel.h"
#include "AudioTools/AudioStreams.h"

namespace audio_tools {


// Output I2S data to built-in DAC, no matter the data format is 16bit or 32 bit, the DAC module will only take the 8bits from MSB
// so we convet it to a singed 16 bit value
static inline uint16_t convert8DAC(int64_t value, int value_bits_per_sample){
    uint16_t result = value;
    if (value_bits_per_sample!=16){
      // convert to 16 bit value
      result = (value * NumberConverter::maxValue(16) / NumberConverter::maxValue(value_bits_per_sample));
    }
    // uint_t positive range
    result+= 32768;
    return result;    
}

/**
 * @brief ESP32 specific configuration for i2s input via adc. The default input pin is GPIO34. We always use int16_t values. The default
 * output pins are GPIO25 and GPIO26! 
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogConfig : public AudioBaseInfo {
  public:
    // allow ADC to access the protected methods
    friend class AnalogAudioStream;

    // public config parameters
    RxTxMode mode;
    int dma_buf_count = I2S_BUFFER_COUNT;
    int dma_buf_len = I2S_BUFFER_SIZE;
    int mode_internal; 
    int port_no = I2S_NUM_0; // Analog input and output only supports 0!
    bool use_apll = false;
    bool uninstall_driver_on_end = true;

    AnalogConfig() {
        sample_rate = 44100;
        bits_per_sample = 16;
        channels = 2;
        this->mode = TX_MODE;
        // enable both channels
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    }

    /// Default constructor
    AnalogConfig(RxTxMode rxtxMode) {
      sample_rate = 44100;
      bits_per_sample = 16;
      channels = 2;
      mode = rxtxMode;
      if (mode == RX_MODE) {
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
        setInputPin1(PIN_ADC1);
//        setInputPin2(PIN_ADC2);
        LOGI("I2S_MODE_ADC_BUILT_IN");
      } else {
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
        LOGI("I2S_MODE_DAC_BUILT_IN");
      }
    }

    /// Copy constructor
    AnalogConfig(const AnalogConfig &cfg) = default;

    /// Defines an alternative input pin (for the left channel)
    void setInputPin1(int pin=PIN_ADC1){
      setInputPin1(pin,0);
    }

    // /// Defines an alternative input pin for the right channel
    // void setInputPin2(int pin=PIN_ADC2){
    //   setInputPin1(pin,1);
    // }

    void logInfo() {
      AudioBaseInfo::logInfo();
      if (mode == TX_MODE){
        LOGI("analog left output pin: %d", 25);
        LOGI("analog right output pin: %d", 26);
      } else {
        LOGI("input pin1: %d", adc_pin[0]);
        if (channels==2){
          LOGI("input pin2: %d", adc_pin[1]);
        }
      }
    }

  protected:
  // input values
    int adc_pin[2];
    adc_unit_t adc_unit[2];
    adc1_channel_t adc_channel[2];

    /// Defines the current ADC pin. The following GPIO pins are supported: GPIO32-GPIO39
    void setInputPin1(int gpio, int channelIdx){
      this->adc_pin[channelIdx] = gpio;
      switch(gpio){
        case 32:
          adc_unit[channelIdx] = ADC_UNIT_1;
          adc_channel[channelIdx] = ADC1_GPIO32_CHANNEL;
          break;
        case 33:
          adc_unit[channelIdx] = ADC_UNIT_1;
          adc_channel[channelIdx]  = ADC1_GPIO33_CHANNEL;
          break;
        case 34:
          adc_unit[channelIdx] = ADC_UNIT_1;
          adc_channel[channelIdx]  = ADC1_GPIO34_CHANNEL;
          break;
        case 35:
          adc_unit[channelIdx] = ADC_UNIT_1;
          adc_channel[channelIdx]  = ADC1_GPIO35_CHANNEL;
          break;
        case 36:
          adc_unit[channelIdx] = ADC_UNIT_1;
          adc_channel[channelIdx]  = ADC1_GPIO36_CHANNEL;
          break;
        case 37:
          adc_unit[channelIdx] = ADC_UNIT_1;
          adc_channel[channelIdx]  = ADC1_GPIO37_CHANNEL;
          break;
        case 38:
          adc_unit[channelIdx] = ADC_UNIT_1;
          adc_channel[channelIdx]  = ADC1_GPIO38_CHANNEL;
          break;
        case 39:
          adc_unit[channelIdx] = ADC_UNIT_1;
          adc_channel[channelIdx]  = ADC1_GPIO39_CHANNEL;
          break;

        default:
          LOGE( "%s - pin GPIO%d is not supported", __func__,gpio);
      }
    }    
};


/**
 * @brief A very fast ADC and DAC using the ESP32 I2S interface.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogAudioStream  : public AudioStreamX {
  public:
    /// Default constructor
    AnalogAudioStream() {
    }

    /// Destructor
    ~AnalogAudioStream() {
      end();
    }

    /// Provides the default configuration
    AnalogConfig defaultConfig(RxTxMode mode=TX_MODE) {
        LOGD( "%s", __func__);
        AnalogConfig config(mode);
        return config;
    }

    /// updates the sample rate dynamically 
    virtual void setAudioInfo(AudioBaseInfo info) {
        LOGI(LOG_METHOD);
        if (adc_config.sample_rate != info.sample_rate
            || adc_config.channels != info.channels
            || adc_config.bits_per_sample != info.bits_per_sample) {
            adc_config.sample_rate = info.sample_rate;
            adc_config.bits_per_sample = info.bits_per_sample;
            adc_config.channels = info.channels;
            adc_config.logInfo();
            end();
            begin(adc_config);        
        }
    }

    /// Reopen with last config
    bool begin() override {
      return begin(adc_config);
    }

    /// starts the DAC 
    bool begin(AnalogConfig cfg) {
      LOGI(LOG_METHOD);
      cfg.logInfo();

      if (!is_driver_installed){
        port_no = (i2s_port_t) cfg.port_no;

        adc_config = cfg;
        i2s_config_t i2s_config = {
            .mode                   = (i2s_mode_t)cfg.mode_internal,
            .sample_rate            = (eps32_i2s_sample_rate_type)cfg.sample_rate,
            .bits_per_sample        = (i2s_bits_per_sample_t)cfg.bits_per_sample,
            .channel_format         = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format   = (i2s_comm_format_t)0,
            .intr_alloc_flags       = 0,
            .dma_buf_count          = cfg.dma_buf_count,
            .dma_buf_len            = cfg.dma_buf_len,
            .use_apll               = cfg.use_apll,
            .tx_desc_auto_clear     = false
          // .fixed_mclk = 0
        };


        // setup config
        if (i2s_driver_install(port_no, &i2s_config, 0, nullptr)!=ESP_OK){
          LOGE( "%s - %s", __func__, "i2s_driver_install");
          return false;
        }    

        // record driver as installed
        is_driver_installed = true;  

        // clear i2s buffer
        if (i2s_zero_dma_buffer(port_no)!=ESP_OK) {
          LOGE( "%s - %s", __func__, "i2s_zero_dma_buffer");
          return false;
        }

        switch (cfg.mode) {
          case RX_MODE:
            LOGI("RX_MODE");

            if (i2s_set_adc_mode(cfg.adc_unit[0], cfg.adc_channel[0])!=ESP_OK) {
              LOGE( "%s - %s", __func__, "i2s_driver_install");
              return false;
            }

            // if (cfg.channels>1){
            //   if (i2s_set_adc_mode(cfg.adc_unit[1], cfg.adc_channel[1])!=ESP_OK) {
            //     LOGE( "%s - %s", __func__, "i2s_driver_install");
            //     return false;
            //   }
            // }

            // enable the ADC
            if (i2s_adc_enable(port_no)!=ESP_OK) {
              LOGE( "%s - %s", __func__, "i2s_adc_enable");
              return false;
            }

            // if (adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_DB_11)!=ESP_OK){
            //   LOGE( "%s - %s", __func__, "adc1_config_channel_atten");
            // }
            // if (adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_11)!=ESP_OK){
            //   LOGE( "%s - %s", __func__, "adc1_config_channel_atten");
            // }
            break;

          case TX_MODE:
            LOGI("TX_MODE");
            if (i2s_set_pin(port_no, nullptr) != ESP_OK) LOGE("i2s_set_pin");
            if (i2s_set_dac_mode( I2S_DAC_CHANNEL_BOTH_EN) != ESP_OK) LOGE("i2s_set_dac_mode");
            if (i2s_set_sample_rates(port_no, cfg.sample_rate)  != ESP_OK) LOGE("i2s_set_sample_rates");
            //if (i2s_set_clk(port_no, cfg.sample_rate, cfg.bits_per_sample, I2S_CHANNEL_STEREO)) LOGE("i2s_set_clk");

            break;

          default:
            LOGE( "Unsupported MODE: %d", cfg.mode);
            return false;
            break;

        }
      } else {
        i2s_start(port_no);
        if (adc_config.mode == RX_MODE){
          i2s_adc_enable(port_no); 
        } 
      }
      active = true;
      return true;
    }

    /// stops the I2S and unistalls the driver
    void end() override {
        LOGI(__func__);
        i2s_zero_dma_buffer(port_no);

        // close ADC
        if (adc_config.mode == RX_MODE){
          i2s_adc_disable(port_no); 
        } 
        if (adc_config.uninstall_driver_on_end){
          i2s_driver_uninstall(port_no); 
          is_driver_installed = false;
        } else {
          i2s_stop(port_no);
        }
        active = false;   
    }


    AnalogConfig &config() {
      return adc_config;
    }

     /// writes the data to the I2S interface
    virtual size_t write(const uint8_t *src, size_t size_bytes) override { 
      LOGD(LOG_METHOD);

      size_t result = 0;   
      if (size_bytes>0 && src!=nullptr){
        switch(adc_config.channels){
          case 1:
            result = outputMono(src, size_bytes);    
            break;      
          case 2:
            result = outputStereo(src, size_bytes);    
            break;   
          default:
            LOGE("Unsupported number of channels: %d", adc_config.channels);   
            stop();
        }
        LOGD("converted write size: %d",result);
      }
      return size_bytes;
    }   

    size_t readBytes(uint8_t *dest, size_t size_bytes) override {
      LOGD(LOG_METHOD);
      size_t result = 0;
      if (i2s_read(port_no, dest, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
        LOGE(LOG_METHOD);
      }
      LOGD( "%s - len: %d -> %d", __func__, size_bytes, result);
      return result;
    }

        /// Reads data from I2S
    size_t read(int16_t (*src)[2], size_t sizeFrames)  {
      size_t len = readBytes((uint8_t*)src, sizeFrames * sizeof(int16_t)*2); // 2 bytes * 2 channels     
      size_t result = len / (sizeof(int16_t) * 2); 
      LOGD( "%s - len: %d -> %d", __func__,sizeFrames, result);
      return result;
    }


    virtual int available() override {
      return active ? adc_config.dma_buf_len*adc_config.dma_buf_count : 0;
    }

  protected:
    AnalogConfig adc_config;
    i2s_port_t port_no;
    bool active = false;
    bool is_driver_installed = false;
    size_t result=0;


    // The internal DAC only supports 8 bit values - so we need to convert the data
    size_t outputStereo(const void *src, size_t size_bytes) {
      LOGD(LOG_METHOD);
      size_t output_size = 0;   
      size_t result;
      uint16_t *dst = (uint16_t *)src;
      switch(adc_config.bits_per_sample){
        case 16: {
            int16_t *data=(int16_t *)src;
            output_size = size_bytes;
            for (int j=0;j<size_bytes/2;j++){
              dst[j] = convert8DAC(data[j], adc_config.bits_per_sample);
            }
          } break;
        case 24: {
            int24_t *data=(int24_t *)src;
            output_size = (size_bytes/3) * 2;
            for (int j=0;j<size_bytes/3;j++){
              dst[j] = (uint32_t)convert8DAC(data[j], adc_config.bits_per_sample);
            }
          } break;
        case 32: {
            int32_t *data=(int32_t *)src;
            output_size = (size_bytes/4) * 2;
            for (int j=0;j<size_bytes/4;j++){
              dst[j] = convert8DAC(data[j], adc_config.bits_per_sample);
            }
          } break;        
      }

      if (output_size>0){
        if (i2s_write(port_no, src, output_size, &result, portMAX_DELAY)!=ESP_OK){
          LOGE("%s: %d", LOG_METHOD, output_size);
        }
      }

      LOGD("i2s_write %d -> %d bytes", size_bytes, result);
      return result;

    }

    // I2S requires stereo so we convert mono to stereo
    size_t outputMono(const void *src, size_t size_bytes) {
      LOGD(LOG_METHOD);
      size_t output_size = 0;   
      uint16_t out[2];
      size_t resultTotal = 0;
      switch(adc_config.bits_per_sample){
        case 16: {
            int16_t *data=(int16_t *)src;
            for (int j=0;j<size_bytes/2;j++){
                out[0] = convert8DAC(data[j], adc_config.bits_per_sample);
                out[1] = out[0];
                if (i2s_write(port_no, out, 4, &result, portMAX_DELAY)!=ESP_OK){
                  LOGE("%s: %d", LOG_METHOD, output_size);
                }
                resultTotal += result;
            }
          }break;
        case 24: {
            int24_t *data=(int24_t*)src;
            for (int j=0;j<size_bytes/3;j++){
                out[0] = convert8DAC(data[j], adc_config.bits_per_sample);
                out[1] = out[0];
                if (i2s_write(port_no, out, 4, &result, portMAX_DELAY)!=ESP_OK){
                  LOGE("%s: %d", LOG_METHOD, output_size);
                }
                resultTotal += result;
            }
          } break;
        case 32: {
            int32_t *data=(int32_t *)src;
            for (int j=0;j<size_bytes/4;j++){
                out[0] = convert8DAC(data[j], adc_config.bits_per_sample);
                out[1] = out[0];
                if (i2s_write(port_no, out, 4, &result, portMAX_DELAY)!=ESP_OK){
                  LOGE("%s: %d", LOG_METHOD, output_size);
                }
                resultTotal += result;
            }
          } break;        
      }

      LOGD("i2s_write %d -> %d bytes", size_bytes, resultTotal);
      return resultTotal;
    }
};


} // namespace

#endif