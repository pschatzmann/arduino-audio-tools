#pragma once

#include "AudioConfig.h"
#if defined(USE_ANALOG) && defined(ESP32) && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0) || defined(DOXYGEN)

#   include "driver/i2s.h"
#   include "driver/adc.h"
#   include "soc/dac_channel.h"
#   include "soc/adc_channel.h"

namespace audio_tools {

/**
 * @brief ESP32 specific configuration for i2s input via adc. The default input pin is GPIO34. We always use int16_t values. The default
 * output pins are GPIO25 and GPIO26! 
 * 
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogConfigESP32 : public AudioInfo {
  public:
    int buffer_count = ANALOG_BUFFER_COUNT;
    int buffer_size = ANALOG_BUFFER_SIZE;
    RxTxMode rx_tx_mode;
    bool is_blocking_write = true;
    bool is_auto_center_read = true;

    // allow ADC to access the protected methods
    friend class AnalogDriverESP32;
    bool use_apll = false;

    // public config parameters
    int port_no = I2S_NUM_0; // Analog input and output only supports 0!
    bool auto_clear = I2S_AUTO_CLEAR;
    bool uninstall_driver_on_end = true;
    int mode_internal; 
    int adc_pin;

    /// Default constructor
    AnalogConfigESP32(RxTxMode rxtxMode=TX_MODE) {
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
    AnalogConfigESP32(const AnalogConfigESP32 &cfg) = default;

    void logInfo() {
      AudioInfo::logInfo();
      if (rx_tx_mode == TX_MODE){
        LOGI("analog left output pin: %d", 25);
        LOGI("analog right output pin: %d", 26);
      } 
    }

    /// Defines an alternative input pin (for the left channel)
    void setInputPin1(int pin){
      this->adc_pin = pin;
    }
  
};

#ifndef ANALOG_CONFIG
#define ANALOG_CONFIG
using AnalogConfig = AnalogConfigESP32;
#endif

} // ns
#endif
