#pragma once

#ifdef SPECIFIC_PROCESSOR_ARCHITECTURE

namespace audio_tools {

/**
 * @brief Basic I2S API - for the ...
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

    /// starts the DAC with the default config in TX Mode
    void begin(RxTxMode mode = TX_MODE) {
      begin(defaultConfig())
    }

    /// starts the DAC 
    void begin(I2SConfig cfg) {

    }

    /// stops the I2C and unistalls the driver
    void end(){
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

  protected:
    I2SConfig cfg;
    
    /// writes the data to the I2S interface
    size_t writeBytes(const void *src, size_t size_bytes){
      size_t result = 0;            
      return result;
    }

    size_t readBytes(void *dest, size_t size_bytes){
      size_t result = 0;
      return result;
    }

};


#endif
