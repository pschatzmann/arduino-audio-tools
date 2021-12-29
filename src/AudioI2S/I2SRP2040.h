#pragma once

#ifdef ARDUINO_ARCH_RP2040
#include "AudioI2S/I2SConfig.h"
#include "AudioExperiments/I2SBitBangRP2040.h"
#include <I2S.h>
namespace audio_tools {

class I2SBasePIO;
#if USE_I2S==2
typedef RP2040BitBangI2SCore1 I2SBase;
#elif USE_I2S==3
typedef RP2040BitBangI2SWithInterrupts I2SBase;
#else
typedef I2SBasePIO I2SBase;
#endif


/**
 * @brief Basic I2S API - for the ...
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SBasePIO {
  friend class I2SStream;

  public:

    /// Provides the default configuration
    I2SConfig defaultConfig(RxTxMode mode) {
        I2SConfig c(mode);
        return c;
    }

    /// starts the DAC with the default config in TX Mode
    bool begin(RxTxMode mode = TX_MODE) {
      LOGD(LOG_METHOD);
      return begin(defaultConfig(mode));
    }

    /// starts the DAC 
    bool begin(I2SConfig cfg) {
      LOGI(LOG_METHOD);
      cfg.logInfo();
      if (cfg.rx_tx_mode != TX_MODE ){
          LOGE("Unsupported mode: only TX_MODE is supported");
          return false;
      }
      if (!I2S.setBCLK(cfg.pin_bck)){
          LOGE("Could not set bck pin: %d", cfg.pin_bck);
          return false;
      }
      if (!I2S.setDOUT(cfg.pin_data)){
          LOGE("Could not set data pin: %d", cfg.pin_data);
          return false;
      }
      if (cfg.bits_per_sample != 16){
          LOGE("Unsupported bits_per_sample: %d", cfg.bits_per_sample);
          return false;
      }
      if (cfg.channels != 2 ){
          LOGE("Unsupported channels: '%d' - only 2 is supported", cfg.channels);
          return false;
      }
      if (!I2S.begin(cfg.sample_rate)){
          LOGE("Could not start I2S");
          return false;
      }
      return true;
    }

    /// stops the I2C and unistalls the driver
    void end(){
      I2S.end();
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

    /// writes the data to the I2S interface 
    size_t writeBytes(const void *src, size_t size_bytes){
      LOGD(LOG_METHOD);
      // size_t result = I2S.write(src, frames); 
      size_t result = 0;
      uint32_t *p32 = (uint32_t *)src;
      for (int j=0;j<size_bytes/4;j++){
        while (!I2S.write((void*)p32[j], 4)){
          delay(5);
        }
        result = j*4;
      }
      LOGD("%s: %d -> %d ", LOG_METHOD, size_bytes, result);
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes){
      LOGD(LOG_METHOD);
      size_t result = 0;
      return result;
    }

    virtual int availableForWrite() {
      return I2S.availableForWrite();
    }

    int available() {
      return I2S.available();
    }

  protected:
    I2SConfig cfg;
    
};

}

#endif
