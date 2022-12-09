#pragma once
#include "AudioConfig.h"
#if defined(USE_I2S_ANALOG) 
#if defined(ESP32) 
# include "driver/i2s.h"
# include "driver/adc.h"
# include "soc/dac_channel.h"
# include "soc/adc_channel.h"
#endif
namespace audio_tools {

/**
 * @brief ESP32 specific configuration for i2s input via adc. The default input pin is GPIO34. We always use int16_t values. The default
 * output pins are GPIO25 and GPIO26! 
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogConfig : public AudioBaseInfo {
  public:
    int buffer_count = I2S_BUFFER_COUNT;
    int buffer_size = I2S_BUFFER_SIZE;
    RxTxMode rx_tx_mode;

#if defined(ESP32) && defined(USE_I2S_ANALOG) 
    // allow ADC to access the protected methods
    friend class AnalogDriverESP32;

    // public config parameters
    int port_no = I2S_NUM_0; // Analog input and output only supports 0!
    bool use_apll = false;
    bool uninstall_driver_on_end = true;
    int mode_internal; 

    AnalogConfig() {
        sample_rate = 44100;
        bits_per_sample = 16;
        channels = 2;
        this->rx_tx_mode = TX_MODE;
        // enable both channels
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    }

    /// Default constructor
    AnalogConfig(RxTxMode rxtxMode) {
      sample_rate = 44100;
      bits_per_sample = 16;
      channels = 2;
      rx_tx_mode = rxtxMode;
      if (rx_tx_mode == RX_MODE) {
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

    void logInfo() {
      AudioBaseInfo::logInfo();
      if (rx_tx_mode == TX_MODE){
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
#else 
    AnalogConfig() {
        sample_rate = 10000;
        bits_per_sample = 16;
        channels = 2;
        buffer_size = ADC_BUFFER_SIZE;
        buffer_count = ADC_BUFFERS;
        rx_tx_mode = RX_MODE;
    }
    /// Default constructor
    AnalogConfig(RxTxMode rxtxMode) : AnalogConfig() {
      rx_tx_mode = rxtxMode;
      if (rxtxMode != RX_MODE) {
        LOGE("Only RX_MODE supported");
      }
    }
  int start_pin = PIN_ADC_START;

#endif
  
};

class AnalogDriverBase {
public:
    virtual bool begin(AnalogConfig cfg);
    virtual void end();
    virtual void setMaxSampleRate() {}
    virtual size_t write(const uint8_t *src, size_t size_bytes) { return 0;}
    virtual size_t readBytes(uint8_t *dest, size_t size_bytes);
    virtual int available();
};

} // ns
#endif
