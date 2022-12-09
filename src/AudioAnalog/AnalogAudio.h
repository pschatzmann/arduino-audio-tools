#pragma once
#include "AudioConfig.h"
#if defined(USE_I2S_ANALOG) 
#include "AudioAnalog/AnalogAudioBase.h"
#include "AudioAnalog/AnalogAudioESP32.h"
#include "AudioAnalog/AnalogAudioArduino.h"

namespace audio_tools {

/**
 * @brief ESP32: A very fast ADC and DAC using the ESP32 I2S interface.
 * For all other architectures we support reading of audio only using 
 * analog input with a timer.
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogAudioStream  : public AudioStreamX {
  public:
    /// Default constructor
    AnalogAudioStream()=default;

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
    virtual void setAudioInfo(AudioBaseInfo info) {
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
      return driver.begin(cfg);
    }

    /// stops the I2S and unistalls the driver
    void end() override {
      TRACEI();
      driver.end();
    }

    /// Overides the sample rate and uses the max value which is around  ~13MHz. Call this methd after begin();
    void setMaxSampleRate() {
        driver.setMaxSampleRate();
    }

    AnalogConfig &config() {
      return adc_config;
    }

     /// ESP32 only: writes the data to the I2S interface
    size_t write(const uint8_t *src, size_t size_bytes) override { 
      TRACED();
      return driver.write(src, size_bytes);
    }   

    size_t readBytes(uint8_t *dest, size_t size_bytes) override {
        return driver.readBytes(dest, size_bytes);
    }

    int available() override {
        return driver.available();
    }
protected:
    AnalogDriver driver;
    AnalogConfig adc_config;
};

}

#endif