


#pragma once
#include "AudioConfig.h"

#if defined(USE_I2S) 

#include "AudioTools/AudioTypes.h"
#include "AudioTools/AudioStreams.h"
#include "AudioI2S/I2SConfig.h"
#include "AudioI2S/I2SESP32.h"
#include "AudioI2S/I2SESP32V1.h"
#include "AudioI2S/I2SESP8266.h"
#include "AudioI2S/I2SSAMD.h"
#include "AudioI2S/I2SNanoSenseBLE.h"
#include "AudioI2S/I2SRP2040.h"
#include "AudioI2S/I2SRP2040-MBED.h"
#include "AudioI2S/I2SSTM32.h"


namespace audio_tools {

/**
 * @brief We support the Stream interface for the I2S access. In addition we allow a separate mute pin which might also be used
 * to drive a LED... 
 * @ingroup io
 * @tparam T 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class I2SStream : public AudioStream {

    public:
        I2SStream() = default;

#ifdef ARDUINO
        I2SStream(int mute_pin) {
            TRACED();
            this->mute_pin = mute_pin;
            if (mute_pin>0) {
                pinMode(mute_pin, OUTPUT);
                mute(true);
            }
        }
#endif

        /// Provides the default configuration
        I2SConfig defaultConfig(RxTxMode mode=TX_MODE) {
            return i2s.defaultConfig(mode);
        }

        bool begin() {
            return i2s.begin();
        }

        /// Starts the I2S interface
        bool begin(I2SConfig cfg) {
            TRACED();
            bool result = i2s.begin(cfg);
            // unmute
            mute(false);
            return result;
        }

        /// Stops the I2S interface 
        void end() {
            TRACED();
            mute(true);
            i2s.end();
        }

        /// updates the sample rate dynamically 
        virtual void setAudioInfo(AudioInfo info) {
            TRACEI();
            AudioStream::setAudioInfo(info);
            I2SConfig cfg = i2s.config();
            if (cfg.sample_rate != info.sample_rate
                || cfg.channels != info.channels
                || cfg.bits_per_sample != info.bits_per_sample) {
                cfg.sample_rate = info.sample_rate;
                cfg.bits_per_sample = info.bits_per_sample;
                cfg.channels = info.channels;
                cfg.logInfo("I2SStream");

                i2s.end();
                i2s.begin(cfg);        
            }
        }

        /// Writes the audio data to I2S
        virtual size_t write(const uint8_t *buffer, size_t size) {
            LOGD("I2SStream::write: %d", size);
            return i2s.writeBytes(buffer, size);
        }

        /// Reads the audio data
        virtual size_t readBytes( uint8_t *data, size_t length) override { 
            return i2s.readBytes(data, length);
        }

        /// Provides the available audio data
        virtual int available() override {
            return i2s.available();
        }

        /// Provides the available audio data
        virtual int availableForWrite() override {
            return i2s.availableForWrite();
        }

        void flush() override {}

        /// Provides access to the driver
        I2SDriver* driver() {
            return &i2s;
        }

    protected:
        I2SDriver i2s;        
        int mute_pin;

        /// set mute pin on or off
        void mute(bool is_mute){
#ifdef ARDUINO
            if (mute_pin>0) {
                digitalWrite(mute_pin, is_mute ? SOFT_MUTE_VALUE : !SOFT_MUTE_VALUE );
            }
#endif
        }

};

}

#endif
