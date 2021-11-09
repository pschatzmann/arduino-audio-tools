#pragma once

#ifdef STM32
#include "AudioI2S/I2SConfig.h"

namespace audio_tools {

/**
 * @brief Basic I2S API - for the STM32
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
    void begin(RxTxMode mode = TX_MODE) {
      begin(defaultConfig(mode));
    }

    /// starts the DAC 
    void begin(I2SConfig cfg) {
      i2s.Instance = SPI2;
      if (cfg.channels=!2){
        LOGE("Unsupported channels %d - must be 2", cfg.channels);
      }

      i2s.Init.Mode = getMode(cfg);
      i2s.Init.DataFormat = getDataFormat(cfg);
      i2s.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
      i2s.Init.AudioFreq = cfg.sample_rate;
      i2s.Init.CPOL = I2S_CPOL_LOW;
      //i2s.Init.ClockSource = I2S_CLOCK_PLL;
      i2s.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
      if (HAL_I2S_Init(&i2s) != HAL_OK){
        LOGE("HAL_I2S_Init failed");
      }
    }

    /// stops the I2C and unistalls the driver
    void end(){
      if (HAL_I2S_DeInit(&i2s) != HAL_OK){
        LOGE("HAL_I2S_DeInit failed");
      }
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      size_t result = 0;
      HAL_StatusTypeDef res = HAL_I2S_Transmit(&i2s, (uint16_t*)src, size_bytes/2, HAL_MAX_DELAY);
      if(res == HAL_OK) {
        result = size_bytes;
      } else {
        LOGE("HAL_I2S_Transmit failed");
      }
            
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes){
      size_t result = 0;
      HAL_StatusTypeDef res = HAL_I2S_Receive(&i2s, (uint16_t*)dest, size_bytes, HAL_MAX_DELAY);
      if(res == HAL_OK) {
        result = size_bytes;
      } else {
        LOGE("HAL_I2S_Receive failed");
      }

      return result;
    }

  protected:
    I2SConfig cfg;
    I2S_HandleTypeDef i2s;
    
    uint32_t getMode(I2SConfig &cfg){
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

    uint32_t getDataFormat(I2SConfig &cfg) {
        switch(cfg.bits_per_sample){
        case 16:
          return I2S_DATAFORMAT_16B;
        case 24:
          return I2S_DATAFORMAT_24B;
          break;
        case 32:
          return I2S_DATAFORMAT_32B;
          break;
      }
      return I2S_DATAFORMAT_16B;

    }

};

}

#endif
