#pragma once

#ifdef ESP8266
#include "AudioTools/AudioLogger.h"
#include "i2s.h"

namespace audio_tools {

/**
 * @brief Basic I2S API - for the ESP8266
 * Only 16 bits are supported !
 * 
 */
class I2SBase {
  friend class I2SStream;
  public:

    /// Provides the default configuration
    I2SConfig defaultConfig(RxTxMode mode) {
        I2SConfig c(mode);
        return c;
    }

    /// starts the DAC 
    void begin(I2SConfig cfg) {
      i2s_set_rate(cfg.sample_rate);
      cfg.bits_per_sample = 16;
      AudioLogger logger = AudioLogger::instance();
      if(!i2s_rxtx_begin(cfg.rx_tx_mode == RX_MODE, cfg.rx_tx_mode == TX_MODE)){
          logger.error("i2s_rxtx_begin failed");
      }
    }

    /// stops the I2C and unistalls the driver
    void end(){
      i2s_end();
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

  protected:
    I2SConfig cfg;
    
    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      size_t frame_size = cfg.channels * (cfg.bits_per_sample/8);
      uint16_t frame_count = size_bytes / frame_size;
      return i2s_write_buffer(( int16_t *)src, frame_count ) * frame_size;
    }

    /// reads the data from the I2S interface
    size_t readBytes(void *dest, size_t size_bytes){
      size_t result_bytes = 0;
      uint16_t frame_size = cfg.channels * (cfg.bits_per_sample/8);
      size_t frames = size_bytes / frame_size;
      int16_t *ptr = (int16_t *)dest;     

      for (int j=0;j<frames;j++){
          int16_t *left = ptr;
          int16_t *right = ptr+1;
          bool ok = i2s_read_sample(left, right, false); // RX data returned in both 16-bit outputs.
          if(!ok){
            break;
          }
          ptr += 2;
          result_bytes += frame_size;
      }
      return result_bytes;
    }

};

}

#endif
