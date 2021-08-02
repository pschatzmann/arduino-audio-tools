#pragma once

#ifdef __SAMD21G18A__

namespace audio_tools {

/**
 * @brief Basic I2S API - for the SAMD
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
        I2S.begin(cfg.i2s_mode, cfg.sample_rate, cfg.bits_per_sample, cfg.is_master);
        if (cfg.mode = TX_MODE){
            I2S.enableTransmitter();
        } else {
            I2S.enableReceiver();
        }
    }

    /// stops the I2C and unistalls the driver
    void end(){
        I2S.end();
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

    size_t writeBytes(const void *src, size_t size_bytes){
      return I2S.write((const uint8_t *)src, size_bytes);
    }

    size_t readBytes(void *dest, size_t size_bytes){
      return I2S.read(src, size_bytes);
    }

  protected:
    I2SConfig cfg;
    

};


#endif
