
#pragma once
#if defined(STM32) 
#include "AudioPWM/PWMAudioBase.h"
#include "AudioTimer/AudioTimer.h"

namespace audio_tools {

// forward declaration
class PWMAudioStreamSTM32;
typedef PWMAudioStreamSTM32 PWMAudioStream;

/**
 * @brief Audio output to PWM pins for STM32 based Arduino implementations.
 * This is just a preliminary draft that should potentially be replaced by a
 * version which uses the DMA
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PWMAudioStreamSTM32 : public PWMAudioStreamBase {

    public:

        PWMAudioStreamSTM32(){
            LOGD("PWMAudioStreamSTM32");
        }

        // Ends the output
        virtual void end() override {
            TRACED();
            ticker.end(); // it does not hurt to call this even if it has not been started
            is_timer_started = false;
            if (buffer!=nullptr){
                delete buffer;
                buffer = nullptr;
            }
        }


    protected:
        Vector<int> pins;      
        TimerAlarmRepeating ticker; // calls a callback repeatedly with a timeout
        int64_t max_value;

        /// when we get the first write -> we activate the timer to start with the output of data
        virtual void startTimer() override {
            if (!is_timer_started){
                TRACED();
                uint32_t time = AudioUtils::toTimeUs(audio_config.sample_rate);
                ticker.setCallbackParameter(this);
                ticker.begin(defaultPWMAudioOutputCallback, time, US);
                is_timer_started = true;
            }
        }

        /// Setup PWM Pins
        virtual void setupPWM(){
            TRACED();
            analogWriteFrequency(audio_config.pwm_frequency); // Set PMW period to 2000 Hz instead of 1000
            analogWriteResolution(audio_config.resolution);
            // calculat max output value
            max_value = pow(2,audio_config.resolution);

            // setup pins for output
            pins.resize(audio_config.channels);
            for (int j=0;j<audio_config.channels;j++){
                LOGD("Processing channel %d", j);
                int gpio = audio_config.start_pin + j;
                if (audio_config.pins!=nullptr){
                    // use defined pins
                    gpio = audio_config.pins[j];
                } 
                pins[j]=gpio;
                pinMode(gpio, OUTPUT);
                LOGI("pinmode(%d, OUTPUT)", gpio);
            }        
        }

        /// not used -> see startTimer();
        virtual void setupTimer() {
        } 

        virtual int maxChannels() {
            return 4;
        };

        /// provides the max value for the configured resulution
        virtual int maxOutputValue(){
            return max_value;
        }
        
        /// write a pwm value to the indicated channel. The max value depends on the resolution
        virtual void pwmWrite(int channel, int value){
            analogWrite(pins[channel], value);   
        }

        /// timer callback: write the next frame to the pins
        static inline void  defaultPWMAudioOutputCallback(void *obj) {
            PWMAudioStreamSTM32* accessAudioPWM = (PWMAudioStreamSTM32*) obj;
            if (accessAudioPWM!=nullptr){
                accessAudioPWM->playNextFrame();
            }
        }

};


} // Namespace


#endif

