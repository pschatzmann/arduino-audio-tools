#pragma once
#include "AudioConfig.h"
#ifdef USE_PWM

#include "AudioPWM/PWMAudioESP32.h"
#include "AudioPWM/PWMAudioRP2040.h"
#include "AudioPWM/PWMAudioMBED.h"
#include "AudioPWM/PWMAudioSTM32.h"
// this is experimental at the moment
#include "AudioPWM/PWMAudioAVR.h"


namespace audio_tools {

/**
 * @brief Common functionality for PWM output. 
 * Please use the PWMAudioStream typedef instead which references the implementation
 * @ingroup io
 */
class PWMAudioStream : public AudioPrint {
    public:
        ~PWMAudioStream(){
            if (driver.isTimerStarted()){
                end();
            }
        }

        virtual PWMConfig defaultConfig() {
            return default_config;
        }

        PWMConfig config() {
            return audio_config;
        }

        /// updates the sample rate dynamically 
        virtual void setAudioInfo(AudioBaseInfo info) {
            TRACEI();
            PWMConfig cfg = audio_config;
            if (cfg.sample_rate != info.sample_rate
                || cfg.channels != info.channels
                || cfg.bits_per_sample != info.bits_per_sample) {
                cfg.sample_rate = info.sample_rate;
                cfg.bits_per_sample = info.bits_per_sample;
                cfg.channels = info.channels;
                cfg.logInfo();
                end();
                begin(cfg);        
            }
        }

        // /// Starts the PWMAudio using callbacks
        bool begin(uint16_t sampleRate, uint8_t channels, PWMCallbackType cb) {
            TRACED();
            audio_config.channels = channels;
            audio_config.sample_rate = sampleRate;
            driver.setUserCallback(cb);
            return begin();
        }


        /// starts the processing using Streams
        bool begin(PWMConfig config ) {
            TRACED();
            this->audio_config = config;
            return driver.begin(audio_config);
        }  

        bool begin()  {
            TRACED();
            return driver.begin(audio_config);
        }  

        virtual void end() {
            driver.end();
        }

        int availableForWrite() override { 
            return driver.availableForWrite();
        }

        // blocking write for an array: we expect a singed value and convert it into a unsigned 
        size_t write(const uint8_t *wrt_buffer, size_t size) override{
            return driver.write(wrt_buffer, size);
        }

        // When the timer does not have enough data we increase the underflow_count;
        uint32_t underflowsPerSecond(){
            return driver.underflowsPerSecond();
        }
        // provides the effectivly measured output frames per second
        uint32_t framesPerSecond(){
            return driver.framesPerSecond();
        }

    protected:
        PWMConfig audio_config;
        PWMDriver driver; // platform specific driver

};

}

#endif
