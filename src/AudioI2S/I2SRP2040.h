#pragma once

#include "AudioI2S/I2SConfig.h"
#if defined(RP2040_HOWER)
#include <I2S.h>


namespace audio_tools {

/**
 * @brief Basic I2S API - for the ...
 * @ingroup platform
 * @author Phil Schatzmann
 * @author LinusHeu
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
    bool begin(RxTxMode mode = TX_MODE) {
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
      //case RX_MODE:
      //  i2s = I2S(INPUT);
      //  break;
      default:
        LOGE("Unsupported mode: only TX_MODE is supported");
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

      if (!i2s.setBitsPerSample(cfg.bits_per_sample)){
        LOGE("Could not set bits per sample: %d", cfg.bits_per_sample);
        return false;
      }

      if (!i2s.setBuffers(cfg.buffer_count, cfg.buffer_size)){
        LOGE("Could not set buffers: Count: '%d', size: '%d'", cfg.buffer_count, cfg.buffer_size);
        return false;
      }

      if(cfg.i2s_format == I2S_LEFT_JUSTIFIED_FORMAT){
        if(!i2s.setLSBJFormat()){
          LOGE("Could not set LSB Format")
          return false;
        }
      } else if(cfg.i2s_format != I2S_STD_FORMAT){
        LOGE("Unsupported I2S format");
        return false;
      }

      if (cfg.channels < 1 || cfg.channels > 2 ){
        LOGE("Unsupported channels: '%d'", cfg.channels);
        return false;
      }

      if (!i2s.begin(cfg.sample_rate)){
        LOGE("Could not start I2S");
        return false;
      }
      return true;
    }

    /// stops the I2C and uninstalls the driver
    void end() {
      flush();
      i2s.end();
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

    /// writes the data to the I2S interface 
    size_t writeBytes(const void *src, size_t size_bytes) {
      LOGD("writeBytes(%d)", size_bytes);
      size_t result = 0;

      if (cfg.channels==1){
        result = writeExpandChannel(src, size_bytes);
      } else if (cfg.channels==2){
        const uint8_t *p = (const uint8_t*) src;
        while(size_bytes >= sizeof(int32_t)){
          bool justWritten = i2s.write(*(int32_t*)p,true); //I2S::write(int32,bool) actually only returns 0 or 1
          if(justWritten){
            size_bytes -= sizeof(int32_t);
            p += sizeof(int32_t);
            result += sizeof(int32_t);
          } else return result;
        }
      }
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes) {
      TRACEE();
      size_t result = 0;
      return result;
    }

    int availableForWrite()  {
      if (cfg.channels == 1){
        return cfg.buffer_size;
      } else {
        return i2s.availableForWrite();
      } 
    }

    int available() {
      return 0;
    }

    void flush() {
      i2s.flush();
    }

  protected:
    I2SConfig cfg;
    I2S i2s;

    //writes 1 channel to I2S while expanding it to 2 channels
    //returns amount of bytes written from src to i2s
    size_t writeExpandChannel(const void *src, size_t size_bytes) {
      switch(cfg.bits_per_sample){
        case 8: {
          int8_t *pt8 = (int8_t*) src;
          for (int j=0;j<size_bytes;j++){
            i2s.write8(pt8[j], pt8[j]); 
            //LOGI("%d", pt8[j]);
          }
        } break;
        case 16: {
          int16_t *pt16 = (int16_t*) src;
          for (int j=0;j<size_bytes/sizeof(int16_t);j++){            
            i2s.write16(pt16[j], pt16[j]); 
          }
        } break;
        case 24: {
          int32_t *pt24 = (int32_t*) src;
          for (int j=0;j<size_bytes/sizeof(int32_t);j++){
            i2s.write24(pt24[j], pt24[j]); 
          }
        } break;
        case 32:{
          int32_t *pt32 = (int32_t*) src;
          for (int j=0;j<size_bytes/sizeof(int32_t);j++){
            i2s.write32(pt32[j], pt32[j]); 
          }
        } break;
      }
      return size_bytes;
    }
};

using I2SDriver = I2SDriverRP2040;


}

#endif
