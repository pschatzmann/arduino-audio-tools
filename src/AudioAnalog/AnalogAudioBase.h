#pragma once
#include "AudioConfig.h"
#if defined(USE_ANALOG) 
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
class AnalogConfig : public AudioInfo {
  public:
    int buffer_count = PWM_BUFFER_COUNT;
    int buffer_size = PWM_BUFFER_SIZE;
    RxTxMode rx_tx_mode;
    bool is_blocking_write = true;
    bool is_auto_center_read = true;

#if defined(ESP32) && defined(USE_ANALOG) 
    // allow ADC to access the protected methods
    friend class AnalogDriverESP32;

    // public config parameters
    int port_no = I2S_NUM_0; // Analog input and output only supports 0!
    bool use_apll = false;
    bool auto_clear = I2S_AUTO_CLEAR;
    bool uninstall_driver_on_end = true;
    int mode_internal; 
    int adc_pin;

    /// Default constructor
    AnalogConfig(RxTxMode rxtxMode=TX_MODE) {
      sample_rate = 44100;
      bits_per_sample = 16;
      channels = 2;
      rx_tx_mode = rxtxMode;
      if (rx_tx_mode == RX_MODE) {
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
        adc_pin = PIN_ADC1;
        auto_clear = false;
        LOGI("I2S_MODE_ADC_BUILT_IN");
      } else {
        mode_internal = (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
        LOGI("I2S_MODE_DAC_BUILT_IN");
      }
    }

    /// Copy constructor
    AnalogConfig(const AnalogConfig &cfg) = default;

    void logInfo() {
      AudioInfo::logInfo();
      if (rx_tx_mode == TX_MODE){
        LOGI("analog left output pin: %d", 25);
        LOGI("analog right output pin: %d", 26);
      } else {
        LOGI("input pin1: %d", adc_pin);
      }
    }

    /// Defines an alternative input pin (for the left channel)
    void setInputPin1(int pin){
      this->adc_pin = pin;
    }

#else 

    AnalogConfig() {
        sample_rate = 44100;
        bits_per_sample = 16;
        channels = 2;
        buffer_size = ANALOG_BUFFER_SIZE;
        buffer_count = ANALOG_BUFFERS;
        rx_tx_mode = RX_MODE;
    }
    /// Default constructor
    AnalogConfig(RxTxMode rxtxMode) : AnalogConfig() {
      rx_tx_mode = rxtxMode;
    }
    int start_pin = PIN_ANALOG_START;

#endif
  
};

class AnalogDriverBase {
public:
    virtual bool begin(AnalogConfig cfg) = 0;
    virtual void end() = 0;
//  virtual void setMaxSampleRate() {}
    virtual size_t write(const uint8_t *src, size_t size_bytes) { return 0;}
    virtual size_t readBytes(uint8_t *dest, size_t size_bytes) = 0;
    virtual int available() = 0;
    virtual int availableForWrite() { return DEFAULT_BUFFER_SIZE; }
};

} // ns
#endif
