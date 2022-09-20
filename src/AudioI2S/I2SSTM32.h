#pragma once

#ifdef STM32
#include "AudioI2S/I2SConfig.h"

namespace audio_tools {
#include "stm32-i2s.h"

/**
 * @brief Basic I2S API - for the STM32
 * Depends on https://github.com/pschatzmann/STM32F411-i2s-prototype
 * We just add a write and read buffer!
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
      if (cfg.sample_rate!=44100){
        LOGE("Sample rate not supported: %d", cfg.sample_rate);
        return false;
      }
      if (cfg.channels!=2){
        LOGE("Channels not supported: %d", cfg.channels);
        return false;
      }

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
      return p_tx_buffer->writeArray((uint8_t*)src, size_bytes);
    }

    size_t readBytes(void *dest, size_t size_bytes){
      return p_rx_buffer->readArray((uint8_t*)dest, size_bytes);
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
};

}

#endif
