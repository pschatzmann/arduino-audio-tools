#pragma once

#include "AudioConfig.h"
#if defined(ESP32) && defined(USE_ANALOG) && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0) || defined(DOXYGEN)
#include "AudioAnalog/AnalogDriverBase.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "soc/dac_channel.h"
#include "soc/adc_channel.h"
#include "soc/rtc.h"
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
 * @brief Please use AnalogAudioStream: A very fast ADC and DAC using the ESP32 I2S interface.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogDriverESP32  : public AnalogDriverBase {
  public:
    /// Default constructor
    AnalogDriverESP32() = default;

    /// Destructor
    virtual ~AnalogDriverESP32() {
      end();
    }

    /// starts the DAC 
    bool begin(AnalogConfigESP32 cfg) {
      TRACEI();
      cfg.logInfo();

      if (adc_config.is_auto_center_read){
        LOGI("auto_center")
        auto_center.begin(cfg.channels, cfg.bits_per_sample);
      }

      if (!is_driver_installed){
        port_no = (i2s_port_t) cfg.port_no;

        adc_config = cfg;
        i2s_config_t i2s_config = {
            .mode                   = (i2s_mode_t)cfg.mode_internal,
            .sample_rate            = (eps32_i2s_sample_rate_type)cfg.sample_rate,
            .bits_per_sample        = (i2s_bits_per_sample_t)cfg.bits_per_sample,
            .channel_format         = (cfg.channels == 1 && cfg.rx_tx_mode == RX_MODE) ? I2S_CHANNEL_FMT_ONLY_LEFT :I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format   = (i2s_comm_format_t)0,
            .intr_alloc_flags       = 0,
            .dma_buf_count          = cfg.buffer_count,
            .dma_buf_len            = cfg.buffer_size,
            .use_apll               = cfg.use_apll,
            .tx_desc_auto_clear     = cfg.auto_clear,
#if ESP_IDF_VERSION_MAJOR >= 4 
            .fixed_mclk             = 0,
            .mclk_multiple          = I2S_MCLK_MULTIPLE_DEFAULT,
            .bits_per_chan          = I2S_BITS_PER_CHAN_DEFAULT,
#endif
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

        switch (cfg.rx_tx_mode) {
          case RX_MODE:
            LOGI("RX_MODE");
            
            setupInputPin(cfg.adc_pin);

            if (i2s_set_adc_mode(adc_unit, adc_channel)!=ESP_OK) {
              LOGE( "%s - %s", __func__, "i2s_driver_install");
              return false;
            }

            // enable the ADC
            if (i2s_adc_enable(port_no)!=ESP_OK) {
              LOGE( "%s - %s", __func__, "i2s_adc_enable");
              return false;
            }

            break;

          case TX_MODE:
            LOGI("TX_MODE");
            if (i2s_set_pin(port_no, nullptr) != ESP_OK) LOGE("i2s_set_pin");
            if (i2s_set_dac_mode( I2S_DAC_CHANNEL_BOTH_EN) != ESP_OK) LOGE("i2s_set_dac_mode");
            if (i2s_set_sample_rates(port_no, cfg.sample_rate)  != ESP_OK) LOGE("i2s_set_sample_rates");
            //if (i2s_set_clk(port_no, cfg.sample_rate, cfg.bits_per_sample, I2S_CHANNEL_STEREO)) LOGE("i2s_set_clk");

            break;

          default:
            LOGE( "Unsupported MODE: %d", cfg.rx_tx_mode);
            return false;
            break;

        }
      } else {
        i2s_start(port_no);
        if (adc_config.rx_tx_mode == RX_MODE){
          i2s_adc_enable(port_no); 
        } 
      }
      active = true;
      return true;
    }

    /// stops the I2S and unistalls the driver
    void end() override {
        LOGI(__func__);
        if (active) {
          i2s_zero_dma_buffer(port_no);
        }

        // close ADC
        if (adc_config.rx_tx_mode == RX_MODE){
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

    // /// Overides the sample rate and uses the max value which is around  ~13MHz. Call this methd after begin();
    // void setMaxSampleRate() {
    //   //this is the hack that enables the highest sampling rate possible ~13MHz, have fun
    //   SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_A_V, 1, I2S_CLKM_DIV_A_S);
    //   SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_B_V, 1, I2S_CLKM_DIV_B_S);
    //   SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_NUM_V, 1, I2S_CLKM_DIV_NUM_S); 
    //   SET_PERI_REG_BITS(I2S_SAMPLE_RATE_CONF_REG(0), I2S_TX_BCK_DIV_NUM_V, 1, I2S_TX_BCK_DIV_NUM_S); 
    // }

     /// writes the data to the I2S interface
    size_t write(const uint8_t *src, size_t size_bytes) override { 
      TRACED();

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
      TRACED();
      size_t result = 0;
      if (i2s_read(port_no, dest, size_bytes, &result, portMAX_DELAY)!=ESP_OK){
        TRACEE();
      }
      // make sure that the center is at 0
      if (adc_config.is_auto_center_read){
        auto_center.convert(dest, result);
      }
      LOGD( "%s - len: %d -> %d", __func__, size_bytes, result);
      return result;
    }

    virtual int available() override {
      return active ? adc_config.buffer_size*adc_config.buffer_count : 0;
    }

  protected:
    AnalogConfigESP32 adc_config;
    ConverterAutoCenter auto_center;
    i2s_port_t port_no;
    bool active = false;
    bool is_driver_installed = false;
    size_t result=0;

  // input values
    adc_unit_t adc_unit;
    adc1_channel_t adc_channel;

    /// Defines the current ADC pin. The following GPIO pins are supported: GPIO32-GPIO39
    void setupInputPin(int gpio){
      TRACED();

      switch(gpio){
        case 32:
          adc_unit = ADC_UNIT_1;
          adc_channel = ADC1_GPIO32_CHANNEL;
          break;
        case 33:
          adc_unit = ADC_UNIT_1;
          adc_channel  = ADC1_GPIO33_CHANNEL;
          break;
        case 34:
          adc_unit = ADC_UNIT_1;
          adc_channel  = ADC1_GPIO34_CHANNEL;
          break;
        case 35:
          adc_unit = ADC_UNIT_1;
          adc_channel  = ADC1_GPIO35_CHANNEL;
          break;
        case 36:
          adc_unit = ADC_UNIT_1;
          adc_channel  = ADC1_GPIO36_CHANNEL;
          break;
        case 37:
          adc_unit = ADC_UNIT_1;
          adc_channel  = ADC1_GPIO37_CHANNEL;
          break;
        case 38:
          adc_unit = ADC_UNIT_1;
          adc_channel  = ADC1_GPIO38_CHANNEL;
          break;
        case 39:
          adc_unit = ADC_UNIT_1;
          adc_channel  = ADC1_GPIO39_CHANNEL;
          break;

        default:
          LOGE( "%s - pin GPIO%d is not supported", __func__,gpio);
      }
    }

    // The internal DAC only supports 8 bit values - so we need to convert the data
    size_t outputStereo(const void *src, size_t size_bytes) {
      TRACED();
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
      TRACED();
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
/// @brief AnalogAudioStream 
using AnalogDriver = AnalogDriverESP32;

} // namespace

#endif
