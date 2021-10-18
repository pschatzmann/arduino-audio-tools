#pragma once

#ifdef ARDUINO_ARCH_RP2040
#include "AudioI2S/I2SConfig.h"
#include <I2S.h>

namespace audio_tools {

/**
 * @brief Basic I2S API - for the ...
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SBase {
  friend class I2SStream;

  public:

    /// Provides the default configuration
    I2SConfig defaultConfig(RxTxMode mode) {
        I2SConfig c(mode);
        return c;
    }

    /// starts the DAC with the default config in TX Mode
    void begin(RxTxMode mode = TX_MODE) {
      begin(defaultConfig(mode));
    }

    /// starts the DAC 
    void begin(I2SConfig cfg) {
      LOGD(__func__);
      i2s.setBCLK(cfg.pin_bck);
      i2s.setDOUT(cfg.pin_data);
      i2s.setFrequency(cfg.sample_rate);
      if (cfg.bits_per_sample != 16){
          LOGE("Unsupported bits_per_sample: %d", cfg.bits_per_sample);
      }
      if (cfg.rx_tx_mode != TX_MODE ){
          LOGE("Unsupported mode: only TX_MODE is supported");
      }
      if (cfg.channels != 2 ){
          LOGE("Unsupported channels: '%d' - only 2 is supported", cfg.channels);
      }
    }

    /// stops the I2C and unistalls the driver
    void end(){
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      size_t result = i2s.write(src, size_bytes);            
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes){
      size_t result = 0;
      return result;
    }

  protected:
    I2SConfig cfg;
    I2SClass i2s;
    

};

}

#endif
