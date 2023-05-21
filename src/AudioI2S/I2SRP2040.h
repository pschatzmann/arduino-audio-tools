#pragma once

#if defined(ARDUINO_ARCH_RP2040) 
#include "AudioI2S/I2SConfig.h"
#if defined(ARDUINO_ARCH_MBED_RP2040)
#  include "RP2040-I2S.h"
#else
#  include <I2S.h>
#endif


namespace audio_tools {

#if !defined(ARDUINO_ARCH_MBED_RP2040)//todo why this if?
//static ::I2S I2S;
#endif


/**
 * @brief Basic I2S API - for the ...
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriverRP2040 {
  friend class I2SStream;

  public:

    /// Provides the default configuration
    I2SConfig defaultConfig(RxTxMode mode) {
        I2SConfig c(mode);
        return c;
    }

    /// starts the DAC with the default config in TX Mode
    bool begin(RxTxMode mode = TX_MODE)  {
      TRACED();
      return begin(defaultConfig(mode));
    }

    /// starts the DAC 
    bool begin(I2SConfig cfg) {
      TRACEI();
      this->cfg = cfg;
      cfg.logInfo();
      switch (cfg.rx_tx_mode){
      case TX_MODE:
        i2s = I2S(OUTPUT);
        break;
      case RX_MODE:
        i2s = I2S(INPUT);
        break;
      default:
        LOGE("Unsupported mode: only TX_MODE, RX_MODE is supported");
        return false;
        break;
      }

      if (cfg.pin_ws == cfg.pin_bck + 1){ //normal pin order
        if(!i2s.setBCLK(cfg.pin_bck)){
          LOGE("Could not set bck pin: %d", cfg.pin_bck);
          return false;
        }
      } else if(cfg.pin_ws == cfg.pin_bck - 1){ //reverse pin order
        if (!i2s.swapClocks() || !i2s.setBCLK(cfg.pin_ws)){//setBCLK() actually sets the lower pin of bck/ws
          LOGE("Could not set bck pin: %d", cfg.pin_bck);
          return false;
        }
      } else{
        LOGE("pins bck: '%d' and ws: '%d' must be next to each other", cfg.pin_bck, cfg.pin_ws);
        return false;
      }
      if (!i2s.setDATA(cfg.pin_data)){
        LOGE("Could not set data pin: %d", cfg.pin_data);
        return false;
      }

      if (i2s.setBitsPerSample(cfg.bits_per_sample)){
        LOGE("Could not set bits per sample: %d", cfg.bits_per_sample);
        return false;
      }

      if (!i2s.setBuffers(cfg.buffer_count, cfg.buffer_size)){
        LOGE("Could not set buffers: Count: '%d', size: '%d'", cfg.buffer_count, cfg.buffer_size);
        return false;
      }

      if(cfg.i2s_format == I2S_LEFT_JUSTIFIED_FORMAT){//todo is I2S_LEFT_JUSTIFIED_FORMAT even LSBJ?
        if(!i2s.setLSBJFormat()){
          LOGE("Could not set LSB Format")
          return false;
        }
      } else if(cfg.i2s_format != I2S_STD_FORMAT){
        LOGE("Unsupported I2S format");
        return false;
      }

      if (cfg.channels < 1 || cfg.channels > 2 ){
        LOGE("Unsupported channels: '%d' - only 1 or 2 is supported", cfg.channels);
        return false;
      }

      if (!i2s.begin(cfg.sample_rate)){
        LOGE("Could not start I2S");
        return false;
      }
      return true;
    }

    /// stops the I2C and uninstalls the driver
    void end()  {
      i2s.end();//todo flush before end?
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

    /// writes the data to the I2S interface 
    size_t writeBytes(const void *src, size_t size_bytes)  {
      TRACED();
      size_t result = 0;
      int16_t *p16 = (int16_t *)src;
      
      if (cfg.channels==1){//TODO Mono->Stereo that works with all bits per sample
        int samples = size_bytes/2;
        // multiply 1 channel into 2
        int16_t buffer[samples*2]; // from 1 byte to 2 bytes
        for (int j=0;j<samples;j++){
          buffer[j*2]= p16[j];
          buffer[j*2+1]= p16[j];
        } 
        result = i2s.write((const uint8_t*)buffer, size_bytes*2)/2;
      } else if (cfg.channels==2){
        result = i2s.write((const uint8_t*)src, size_bytes);//TODO ensure that size_bytes is always multiple of 4
      } 
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes)  {
      TRACEE();
      size_t result = 0;
      return result;
    }

    int availableForWrite()  {
        //availableForWrite() returns amount of 32-bit words NOT amount of Bytes! *4 to get bytes
      if (cfg.channels == 1){
        return i2s.availableForWrite()*4/2;// return half of it because we double when writing
      } else {
        return i2s.availableForWrite()*4;
      } 
    }

    int available()  {
      return 0;
    }

    void flush()   {
      return i2s.flush();
    }

  protected:
    I2SConfig cfg;
    I2S i2s;
};

using I2SDriver = I2SDriverRP2040;


}

#endif
