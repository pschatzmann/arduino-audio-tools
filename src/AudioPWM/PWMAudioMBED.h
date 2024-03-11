
#pragma once
#if defined(ARDUINO_ARCH_MBED)
#include "AudioPWM/PWMAudioBase.h"
#include "AudioTimer/AudioTimer.h"
#include "mbed.h"


namespace audio_tools {

// forward declaration
class PWMDriverMBED;
/**
 * @typedef  DriverPWMBase
 * @brief Please use DriverPWMBase!
 */
using PWMDriver = PWMDriverMBED;

/**
 * @brief Audio output to PWM pins for MBED based Arduino implementations
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PWMDriverMBED : public DriverPWMBase {

    public:

        PWMDriverMBED() = default;

        // Ends the output
        virtual void end() override {
            TRACED();
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
                TRACED();
                long wait_time = 1000000l / audio_config.sample_rate;
                ticker.setCallbackParameter(this);
                ticker.begin(defaultPWMAudioOutputCallback, wait_time, US);
                is_timer_started = true;
            }
        }

        /// Setup PWM Pins
        virtual void setupPWM(){
            TRACED();
            unsigned long period = 1000000l / audio_config.pwm_frequency;  // -> 30.517578125 microseconds
            pins.resize(audio_config.channels);
            for (int j=0;j<audio_config.channels;j++){
                LOGD("Processing channel %d", j);
                auto gpio = audio_config.pins()[j];
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

        /// Maximum supported channels
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
        static void  defaultPWMAudioOutputCallback(void *obj) {
            PWMDriverMBED* accessAudioPWM = (PWMDriverMBED*) obj;
            if (accessAudioPWM!=nullptr){
                accessAudioPWM->playNextFrame();
            }
        }

};


} // Namespace


#endif

