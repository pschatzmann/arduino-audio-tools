#pragma once

#ifdef STM32
#include "AudioI2S/I2SConfig.h"

namespace audio_tools {
#include "stm32-i2s.h"

/**
 * @brief Basic I2S API - for the STM32
 * Depends on https://github.com/pschatzmann/STM32F411-i2s-prototype
 * We just add a write and read buffer and pass some parameters to the STM32 API!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class I2SBase {
  friend class I2SStream;

  public:

    /// Provides the default configuration
    I2SConfig defaultConfig(RxTxMode mode = TX_MODE) {
        I2SConfig c(mode);
        return c;
    }

    /// starts the DAC with the default config in TX Mode
    bool begin(RxTxMode mode = TX_MODE) {
      return begin(defaultConfig(mode));
    }

    /// starts the DAC 
    bool begin(I2SConfig cfg) {
      deleteBuffers();

      if (cfg.bits_per_sample!=16){
        LOGE("Bits per second not supported: %d", cfg.bits_per_sample);
        return false;
      }
      if (cfg.channels>2 || cfg.channels<=0){
        LOGE("Channels not supported: %d", cfg.channels);
        return false;
      }
      
      setupDefaultI2SParameters();

      switch(cfg.rx_tx_mode){
        case RX_MODE:
          p_rx_buffer = new NBuffer<uint8_t>(cfg.buffer_size, cfg.buffer_count);
        	startI2SReceive(&hi2s3, writeFromReceive, cfg.buffer_size);
          break;
        case TX_MODE:
          p_tx_buffer = new NBuffer<uint8_t>(cfg.buffer_size, cfg.buffer_count);
      	  startI2STransmit(&hi2s3, readToTransmit, cfg.buffer_size);
          break;
        case RXTX_MODE:
          p_tx_buffer = new NBuffer<uint8_t>(cfg.buffer_size, cfg.buffer_count);
	        startI2STransmitReceive(&hi2s3, readToTransmit, writeFromReceive, cfg.buffer_size);
          break;
        default:
          LOGE("Unsupported mode");
          return false;

      }

      return true;
    }

    /// stops the I2C and unistalls the driver
    void end(){
      stopI2S();
      deleteBuffers();
    }

    /// we assume the data is already available in the buffer
    int available() {
      return I2S_BUFFER_COUNT*I2S_BUFFER_SIZE;
    }

    /// We limit the write size to the buffer size
    int availableForWrite() {
      return I2S_BUFFER_COUNT*I2S_BUFFER_SIZE;
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      size_t result = 0;
      if (cfg.channels == 2){
        result = p_tx_buffer->writeArray((uint8_t*)src, size_bytes);
      } else {
        // write each sample 2 times 
        int samples = size_bytes / 2;
        int16_t *src_16 = (int16_t *)src;
        int16_t tmp[2];
        int result = 0;
        for (int j=0;j<samples;j++){
          tmp[0]=src_16[j];
          tmp[1]=src_16[j];
          if (p_tx_buffer->availableForWrite()>=4){
            p_tx_buffer->writeArray((uint8_t*)tmp, 4);
            result = j*2;
          } else {
            // abort if the buffer is full
            break;
          }
        } 
      }
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes){
      if (cfg.channels == 2){
        return p_rx_buffer->readArray((uint8_t*)dest, size_bytes);
      } else {
        // combine two channels to 1: so request double the amout
        int req_bytes = size_bytes*2;
        uint8_t tmp[req_bytes];
        int16_t *tmp_16 = (int16_t*) tmp;
        int eff_bytes = p_rx_buffer->readArray((uint8_t*)tmp, req_bytes);
        // add 2 channels together
        int16_t *dest_16 = (int16_t *)dest; 
        int16_t eff_samples = eff_bytes / 2;
        int16_t idx = 0;
        for (int j=0;j<eff_samples;j+=2){
          dest_16[idx++] = static_cast<float>(tmp_16[j])+tmp_16[j+1] / 2.0;
        }
        return eff_bytes / 2;
      }
    }

    static void writeFromReceive(uint8_t *buffer, uint16_t byteCount){
      p_rx_buffer->writeArray(buffer, byteCount);
    }

    static void readToTransmit(uint8_t *buffer, uint16_t byteCount) {
      memset(buffer,0,byteCount);
      p_tx_buffer->readArray(buffer, byteCount);
    }

  protected:
    I2SConfig cfg;
    inline static NBuffer<uint8_t> *p_tx_buffer=nullptr;
    inline static NBuffer<uint8_t> *p_rx_buffer=nullptr;

    void deleteBuffers() {
      if (p_rx_buffer!=nullptr) {
        delete p_rx_buffer;
        p_rx_buffer = nullptr;
      }
      if (p_tx_buffer!=nullptr) {
        delete p_tx_buffer;
        p_tx_buffer = nullptr;
      }
    }

    void setupDefaultI2SParameters() {
      i2s_default_samplerate = getSampleRate(cfg);
      i2s_default_mode = getMode(cfg);
      i2s_default_standard = getStandard(cfg);
      i2s_default_fullduplexmode = cfg.rx_tx_mode == RXTX_MODE ? I2S_FULLDUPLEXMODE_ENABLE : I2S_FULLDUPLEXMODE_DISABLE;
    }

    uint32_t getMode(I2SConfig &cfg){
      if (cfg.is_master) {
        switch(cfg.rx_tx_mode){
          case RX_MODE:
            return I2S_MODE_MASTER_RX;
          case TX_MODE:
            return I2S_MODE_MASTER_TX;
          default:
            LOGE("RXTX_MODE not supported");
            return I2S_MODE_MASTER_TX;
        }
      } else {
        switch(cfg.rx_tx_mode){
          case RX_MODE:
            return I2S_MODE_SLAVE_RX;
          case TX_MODE:
            return I2S_MODE_SLAVE_TX;
          default:
            LOGE("RXTX_MODE not supported");
            return I2S_MODE_SLAVE_TX;
        }
      }
    }

    
    uint32_t getStandard(I2SConfig &cfg){
      uint32_t result;
      switch(cfg.i2s_format) {
          case I2S_PHILIPS_FORMAT:
          return I2S_STANDARD_PHILIPS;
        case I2S_STD_FORMAT:
        case I2S_LSB_FORMAT:
        case I2S_RIGHT_JUSTIFIED_FORMAT:
          return I2S_STANDARD_MSB;
        case I2S_MSB_FORMAT:
        case I2S_LEFT_JUSTIFIED_FORMAT:
          return I2S_STANDARD_LSB;
      }
      return I2S_STANDARD_PHILIPS;
    }

    uint32_t getSampleRate(I2SConfig &cfg){
      switch(cfg.sample_rate){
        case I2S_AUDIOFREQ_192K:              
        case I2S_AUDIOFREQ_96K:             
        case I2S_AUDIOFREQ_48K:               
        case I2S_AUDIOFREQ_44K:               
        case I2S_AUDIOFREQ_32K:               
        case I2S_AUDIOFREQ_22K:               
        case I2S_AUDIOFREQ_16K:               
        case I2S_AUDIOFREQ_11K:               
        case I2S_AUDIOFREQ_8K:
          return cfg.sample_rate;
        default:
          LOGE("Unsupported sample rate: %u", cfg.sample_rate);
          return cfg.sample_rate;
      }
    }


};

}

#endif
