#pragma once
#include "AudioToolsConfig.h"

// Support AnalogAudioStream
#if defined(USE_ANALOG) 
#  include "AnalogDriverBase.h"
#  include "AnalogDriverESP32.h"
#  include "AnalogDriverESP32V1.h"
#  include "AnalogDriverMBED.h"
#  include "AnalogDriverZephyr.h"
#  include "AnalogDriverArduino.h"

namespace audio_tools {

/**
 * @brief ESP32: A very fast ADC and DAC using the ESP32 I2S interface.
 * For all other architectures we support reading of audio only using 
 * analog input with a timer.
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogAudioStream  : public AudioStream {
  public:
    /// Default constructor
    AnalogAudioStream()=default;
    /// Constructor with alternative driver
    AnalogAudioStream(AnalogDriverBase &driver) { p_driver = &driver;};

    /// Destructor
    virtual ~AnalogAudioStream() {
      end();
    }

    /// Provides the default configuration
    AnalogConfig defaultConfig(RxTxMode mode=TX_MODE) {
        TRACED();
        AnalogConfig config(mode);
        return config;
    }

    /// updates the sample rate dynamically 
    virtual void setAudioInfo(AudioInfo info) {
        TRACEI();
        if (adc_config.sample_rate != info.sample_rate
            || adc_config.channels != info.channels
            || adc_config.bits_per_sample != info.bits_per_sample) {
            adc_config.sample_rate = info.sample_rate;
            adc_config.bits_per_sample = info.bits_per_sample;
            adc_config.channels = info.channels;
            adc_config.logInfo();
            end();
            begin(adc_config);        
        }
    }

    /// Reopen with last config
    bool begin() override {
      return begin(adc_config);
    }

    /// starts the DAC 
    bool begin(AnalogConfig cfg) {
      TRACEI();
      return p_driver->begin(cfg);
    }

    /// stops the I2S and unistalls the analog
    void end() override {
      TRACEI();
      p_driver->end();
    }

    AnalogConfig &config() {
      return adc_config;
    }

     /// ESP32 only: writes the data to the I2S interface
    size_t write(const uint8_t *data, size_t len) override { 
      TRACED();
      return p_driver->write(data, len);
    }   

    size_t readBytes(uint8_t *data, size_t len) override {
        return p_driver->readBytes(data, len);
    }

    int available() override {
        return p_driver->available();
    }

    int availableForWrite() override {
        return p_driver->availableForWrite();
    }

    /// Provides access to the driver
    AnalogDriverBase* driver() {
        return p_driver;
    }

protected:
    AnalogDriver analog_driver;
    AnalogDriverBase *p_driver = &analog_driver;
    AnalogConfig adc_config;
};

}

#endif

// Support AnalogAudioArduino
#if defined(USE_TIMER) && defined(ARDUINO)
#  include "AnalogAudioArduino.h"
#endif
