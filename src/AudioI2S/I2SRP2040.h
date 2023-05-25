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

      if (!i2s.setBitsPerSample(cfg.bits_per_sample)){
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
      
      if (cfg.channels==1){
        result = writeExpandChannel(src,size_bytes);
      } else if (cfg.channels==2){
        size_bytes = size_bytes /4*4;//ensures that size_bytes is always multiple of 4. Just don't write the rest, if it isn't
        result = i2s.write((uint8_t*) src, size_bytes);
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

    //writes 1 channel to I2S while expanding it to 2 channels
    //writes multiple of 4 bytes like I2S::write() wants
    //returns amount of bytes written from src to i2s
    size_t writeExpandChannel(const void *src, size_t size_bytes){
      size_t writtenBytes = 0;

      switch(cfg.bits_per_sample){
        case 8:
        for(int i = 0; i<size_bytes - 1; i += 2){//2 Samples read at a time to write 4 bytes
          int8_t frame[4];
          int8_t *data = (int8_t*) src;
          frame[0] = data[i];
          frame[1] = data[i];
          frame[2] = data[i+1];//i<size_bytes-1 in for() because of this +1
          frame[3] = data[i+1];
          size_t justWritten = i2s.write((uint8_t*)frame,4);
          if(!justWritten){//write() only writes/returns multiples of 4. Either it writes 0 or 4. 
            return writtenBytes;
          } else {
            //half because we doubled the bytes before writing to get 2 channels from 1
            writtenBytes += justWritten / 2;
          }
        }
        break;
        case 16:
        for(int i = 0; i<size_bytes/sizeof(int16_t); i++){//1 sample written at a time to write 4 bytes
          int16_t frame[2];
          int16_t *data = (int16_t *) src;
          frame[0] = data[i];
          frame[1] = data[i];
          size_t justWritten = i2s.write((uint8_t*)frame, 4);//todo or use write(uint32_t,false) instead?
          if(!justWritten){//write() only writes/returns multiples of 4. Either it writes 0 or 4.
            return writtenBytes;
          } else{
            //half because we doubled the bytes before writing to get 2 channels from 1
            writtenBytes += justWritten /2;
          }
        }
        break;
        case 24://24bps are already stored as left-aligned int32_t => handle just like 32bps
        case 32:
        for(int i = 0; i<size_bytes/sizeof(int32_t); i++){//1 sample written at a time to write 2*4 bytes
          int32_t frame[2];
          int32_t *data = (int32_t *) src;
          frame[0] = data[i];
          frame[1] = data[i];
          size_t justWritten = i2s.write((uint8_t*) frame, 8);
          if(justWritten != 8){//because we write 8 bytes, write() might return 0,4,8
            writtenBytes += justWritten /2;
            return writtenBytes;
          } else {
            //half because we doubled the bytes before writing to get 2 channels from 1
            writtenBytes += justWritten /2;
          }

        }
        break;
      }
      //mono=>stereo = doubling of bytes. We return here how many bytes from src were written
      return writtenBytes;

    }
};

using I2SDriver = I2SDriverRP2040;


}

#endif
