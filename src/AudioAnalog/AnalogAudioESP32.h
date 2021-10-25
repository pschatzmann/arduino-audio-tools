#pragma once

#if defined(ESP32) 
#include "AudioConfig.h"
#include "esp_a2dp_api.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "soc/dac_channel.h"
#include "soc/adc_channel.h"
#include "AudioTools/AudioStreams.h"

namespace audio_tools {

typedef int16_t arrayOf2int16_t[2];

const char* ADC_TAG = "ADC";

// Output I2S data to built-in DAC, no matter the data format is 16bit or 32 bit, the DAC module will only take the 8bits from MSB
static int16_t convert8DAC(int value, int value_bits_per_sample){
    // -> convert to positive 
    int16_t result = (value * maxValue(8) / maxValue(value_bits_per_sample)) + maxValue(8) / 2;
    return result;    
}


/**
 * @brief ESP32 specific configuration for i2s input via adc. The default input pin is GPIO34. We always use int16_t values. The default
 * output pins are GPIO25 and GPIO26! 
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * 
 */
class AnalogConfig : public AudioBaseInfo {
  public:
    // allow ADC to access the protected methods
    friend class AnalogAudio;

    // public config parameters
    RxTxMode mode;
    int dma_buf_count = I2S_BUFFER_COUNT;
    int dma_buf_len = I2S_BUFFER_SIZE;
    int mode_internal; 
    int port_no = I2S_NUM_0; // Analog input and output only supports 0!
    bool use_apll = false;

    AnalogConfig() {
        sample_rate = 44100;
        bits_per_sample = 16;
        channels = 2;
        this->mode = TX_MODE;
        // enable both channels
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_DAC_BUILT_IN);
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
        setInputPin2(PIN_ADC2);
      } else {
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
      }
    }

    /// Copy constructor
    AnalogConfig(const AnalogConfig &cfg) = default;

    /// Defines an alternative input pin (for the left channel)
    void setInputPin1(int pin=PIN_ADC1){
      setInputPin1(pin,0);
    }

    /// Defines an alternative input pin for the right channel
    void setInputPin2(int pin=PIN_ADC2){
      setInputPin1(pin,1);
    }

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
class AnalogAudio  : public AudioStream {

  public:
    /// Default constructor
    AnalogAudio() {
    }

    /// Destructor
    ~AnalogAudio() {
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

    /// starts the DAC 
    void begin(AnalogConfig cfg) {
      LOGI(LOG_METHOD);
      cfg.logInfo();
      port_no = (i2s_port_t) cfg.port_no;

      adc_config = cfg;
      i2s_config_t i2s_config = {
          .mode = (i2s_format_t) cfg.mode_internal,
          .sample_rate = cfg.sample_rate,
          .bits_per_sample = (i2s_bits_per_sample_t)16,
          .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
          .communication_format = (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_I2S,
          .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
          .dma_buf_count = cfg.dma_buf_count,
          .dma_buf_len = cfg.dma_buf_len,
          .use_apll = cfg.use_apll,
          .tx_desc_auto_clear = false
        //  .fixed_mclk = 0
        };


      // setup config
      if (i2s_driver_install(port_no, &i2s_config, 0, nullptr)!=ESP_OK){
        LOGE( "%s - %s", __func__, "i2s_driver_install");
        return;
      }      

      // clear i2s buffer
      if (i2s_zero_dma_buffer(port_no)!=ESP_OK) {
        LOGE( "%s - %s", __func__, "i2s_zero_dma_buffer");
        return;
      }

      switch (cfg.mode) {
        case RX_MODE:
          //init ADC pad

          if (i2s_set_adc_mode(cfg.adc_unit[0], cfg.adc_channel[0])!=ESP_OK) {
            LOGE( "%s - %s", __func__, "i2s_driver_install");
            return;
          }

          if (cfg.channels>1){
            if (i2s_set_adc_mode(cfg.adc_unit[1], cfg.adc_channel[1])!=ESP_OK) {
              LOGE( "%s - %s", __func__, "i2s_driver_install");
              return;
            }
          }

          // enable the ADC
          if (i2s_adc_enable(port_no)!=ESP_OK) {
            LOGE( "%s - %s", __func__, "i2s_adc_enable");
            return;
          }

          // if (adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_DB_11)!=ESP_OK){
          //   LOGE( "%s - %s", __func__, "adc1_config_channel_atten");
          // }
          // if (adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_11)!=ESP_OK){
          //   LOGE( "%s - %s", __func__, "adc1_config_channel_atten");
          // }

          break;
        case TX_MODE:
          i2s_set_pin(port_no, nullptr); 
          break;
      }
      active = true;
      LOGI(ADC_TAG, "%s - %s", __func__,"end");

    }

    /// stops the I2C and unistalls the driver
    void end(){
        LOGD( "%s", __func__);
        i2s_adc_disable(port_no); 
        i2s_driver_uninstall(port_no); 
        active = false;   
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
    
     /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      LOGD(LOG_METHOD);

      size_t result = 0;   
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
   
      return result;
    }   

    size_t readBytes(void *dest, size_t size_bytes){
      size_t result = 0;
      if (i2s_read(port_no, dest, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
        LOGE(LOG_METHOD);
      }
      LOGD( "%s - len: %d -> %d", __func__, size_bytes, result);
      //vTaskDelay(1);
      return result;
    }

    virtual size_t write(uint8_t c) {
      return 0;
    }

    virtual int available() {
      return active ? adc_config.dma_buf_len*adc_config.dma_buf_count : 0;
    }

    virtual int read() {
       return -1;
    }    
    
    virtual int peek() {
       return -1;
    }

    virtual void flush() {  
    }

  protected:
    AnalogConfig adc_config;
    i2s_port_t port_no;
    bool active = false;
    size_t result;
    size_t resultTotal = 0;


    // The internal DAC only supports 8 bit values - so we need to convert the data
    size_t outputStereo(const void *src, size_t size_bytes) {
      size_t output_size = 0;   
      size_t result;

      switch(adc_config.bits_per_sample){
        case 16: {
            int16_t *data=(int16_t *)src;
            output_size = (size_bytes/2) * 2;
            for (int j=0;j<size_bytes/2;j++){
              data[j] = convert8DAC(data[j], adc_config.bits_per_sample);
            }
          } break;
        case 24: {
            int24_t *data=(int24_t *)src;
            output_size = (size_bytes/3) * 3;
            for (int j=0;j<size_bytes/3;j++){
              data[j] = convert8DAC(data[j], adc_config.bits_per_sample);
            }
          } break;
        case 32: {
            int32_t *data=(int32_t *)src;
            output_size = (size_bytes/4) * 4;
            for (int j=0;j<size_bytes/4;j++){
              data[j] = convert8DAC(data[j], adc_config.bits_per_sample);
            }
          } break;        
      }

      if (i2s_write(port_no, src, output_size, &result, portMAX_DELAY)!=ESP_OK){
        LOGE(LOG_METHOD);
      }

      LOGD("i2s_write %d -> %d bytes", size_bytes, result);
      return result;

    }

    // I2S requires stereo so we convert mono to stereo
    size_t outputMono(const void *src, size_t size_bytes) {
      size_t output_size = 0;   
      int16_t out[2];

      switch(adc_config.bits_per_sample){
        case 16: {
            int16_t *data=(int16_t *)src;
            for (int j=0;j<size_bytes/2;j++){
                out[0] = convert8DAC(data[j], adc_config.bits_per_sample);
                out[1] = out[0];
                if (i2s_write(port_no, out, 4, &result, portMAX_DELAY)!=ESP_OK){
                  LOGE(LOG_METHOD);
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
                  LOGE(LOG_METHOD);
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
                  LOGE(LOG_METHOD);
                }
                resultTotal += result;
            }
          } break;        
      }

      LOGD("i2s_write %d -> %d bytes", size_bytes, resultTotal);
      return resultTotal;
    }

    
};

/**
 * @brief We support the Stream interface for the AnalogAudio class
 * 
 * @tparam T 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AnalogAudioStream : public BufferedStream  {

    public:
        AnalogAudioStream() : BufferedStream(DEFAULT_BUFFER_SIZE){
        }

        /// Provides the default configuration
        AnalogConfig defaultConfig(RxTxMode mode = TX_MODE) {
            return adc.defaultConfig(mode);
        }

        void setAudioInfo(AudioBaseInfo info){
          LOGI(LOG_METHOD);
          adc.setAudioInfo(info);
        }


        void begin(AnalogConfig cfg) {
          config = cfg;
            adc.begin(cfg);
            // unmute
            mute(false);
        }

        void end() {
            mute(true);
            adc.end();
        }

    protected:
        AnalogAudio adc;
        int mute_pin;
        AnalogConfig config;

        /// set mute pin on or off
        void mute(bool is_mute){
            if (mute_pin>0) {
                digitalWrite(mute_pin, is_mute ? SOFT_MUTE_VALUE : !SOFT_MUTE_VALUE );
            }
        }

        virtual size_t writeExt(const uint8_t* data, size_t len) {
            return adc.writeBytes(data, len);
        }

        virtual size_t readExt( uint8_t *data, size_t length) { 
            return adc.readBytes(data, length);
        }

};

} // namespace

#endif