
#pragma once
#if defined(__arm__)  && __has_include("mbed.h")
#include "AudioPWM/PWMAudioBase.h"
#include "AudioTimer/AudioTimer.h"
#include "mbed.h"

namespace audio_tools {

// forward declaration
class PWMAudioStreamMBED;
typedef PWMAudioStreamMBED PWMAudioStream;

/**
 * @brief Audio output to PWM pins for MBED based Arduino implementations
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PWMAudioStreamMBED : public PWMAudioStreamBase {

    public:

        PWMAudioStreamMBED(){
            LOGD("PWMAudioStreamMBED");
        }

        // Ends the output
        virtual void end() override {
            LOGD(LOG_METHOD);
            ticker.end(); // it does not hurt to call this even if it has not been started
            is_timer_started = false;

            // stop and release pins
            for (int j=0;j<audio_config.channels;j++){
                if (pins[j]!=nullptr){
                    pins[j]->suspend(); 
                    delete pins[j];
                    pins[j] = nullptr;
                }
            }
            pins.clear();
            //pins.shrink_to_fit();
        }


    protected:
        Vector<mbed::PwmOut*> pins;      
        TimerAlarmRepeating ticker; // calls a callback repeatedly with a timeout

        /// when we get the first write -> we activate the timer to start with the output of data
        virtual void startTimer() override {
            if (!is_timer_started){
                LOGD(LOG_METHOD);
                long wait_time = 1000000l / audio_config.sample_rate;
                ticker.setCallbackParameter(this);
                ticker.begin(defaultPWMAudioOutputCallback, wait_time, US);
                is_timer_started = true;
            }
        }

        /// Setup PWM Pins
        virtual void setupPWM(){
            LOGD(LOG_METHOD);
            unsigned long period = 1000000l / audio_config.pwm_frequency;  // -> 30.517578125 microseconds
            pins.resize(audio_config.channels);
            for (int j=0;j<audio_config.channels;j++){
                LOGD("Processing channel %d", j);
                int gpio = audio_config.start_pin + j;
                if (audio_config.pins!=nullptr){
                    // use defined pins
                    gpio = audio_config.pins[j];
                }
                mbed::PwmOut* pin = new mbed::PwmOut(digitalPinToPinName(gpio));
                LOGI("PWM Pin: %d", gpio);
                pin->period_us(period);  
                pin->write(0.0f);  // 0% duty cycle ->  
                pin->resume(); // in case it was suspended before
                pins[j] = pin;
            }
        }

        /// not used -> see startTimer();
        virtual void setupTimer() {
        } 

        virtual int maxChannels() {
            return 16;
        };

        /// provides the max value for the configured resulution
        virtual int maxOutputValue(){
            return 1000;
        }
        
        /// write a pwm value to the indicated channel. The max value depends on the resolution
        virtual void pwmWrite(int channel, int value){
            float float_value = static_cast<float>(value) / maxOutputValue();
            pins[channel]->write(float_value);    // pwm the value is between 0.0 and 1.0 
        }

        /// timer callback: write the next frame to the pins
        static inline void  defaultPWMAudioOutputCallback(void *obj) {
            PWMAudioStreamMBED* accessAudioPWM = (PWMAudioStreamMBED*) obj;
            if (accessAudioPWM!=nullptr){
                accessAudioPWM->playNextFrame();
            }
        }

};


} // Namespace


#endif

