#pragma once

#include "AudioConfig.h"
#ifdef I2S_SUPPORT
#include "AudioTypes.h"
#include "I2S_ESP32.h"
#include "I2S_ESP8266.h"
#include "I2S_SAMD.h"

namespace audio_tools {

/**
 * @brief A Simple I2S interface class for the ESP32 which supports the reading and writing with a defined data type
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template<typename T>
class I2S : public I2SBase {

  public:
    /// Default Constructor
    I2S() {
    }

    /// Destructor
    ~I2S() {
      end();
    }

    void begin(I2SConfig cfg) {
      // define bits per sampe from data type
      cfg.bits_per_sample = sizeof(T) * 8;
      I2SBase::begin(cfg);
    }

    /// writes the data to the I2S interface
    size_t write(T (*src)[2], size_t frameCount){
      return writeBytes(src, frameCount * sizeof(T) * 2); // 2 bytes * 2 channels      
    }
    
    /// Reads data from I2S
    size_t read(T (*src)[2], size_t frameCount){
      size_t len = readBytes(src, frameCount * sizeof(T) * 2); // 2 bytes * 2 channels     
      size_t result = len / (sizeof(T) * 2); 
      return result;
    }
  
};


}

#endif
