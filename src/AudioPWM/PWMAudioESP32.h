
#pragma once
#ifdef ESP32
#include "AudioPWM/PWMAudioBase.h"


namespace audio_tools {

// forward declaration
class PWMDriverESP32;
/**
 * @typedef  DriverPWMBase
 * @brief Please use DriverPWMBase!
 */
using PWMDriver = PWMDriverESP32;


/**
 * @brief Information for a PIN
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PinInfoESP32 {
    int pwm_channel;
    int gpio;
};

typedef PinInfoESP32 PinInfo;

/**
 * @brief Audio output to PWM pins for the ESP32. The ESP32 supports up to 16 channels. 
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PWMDriverESP32 : public DriverPWMBase {
    public:
        //friend void pwm_callback(void*ptr);

        PWMDriverESP32(){
            TRACED();
        }

        // Ends the output
        virtual void end(){
            TRACED();
            timer.end();
            is_timer_started = false;
            for (int j=0;j<audio_config.channels;j++){
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0 , 0)
                ledcDetach(pins[j].gpio);
#else
                ledcDetachPin(pins[j].gpio);
#endif
            }
        }

        /// when we get the first write -> we activate the timer to start with the output of data
        virtual void startTimer(){
            if (!timer){
                TRACEI();
                audio_config = audioInfo();
                timer.begin(pwm_callback, audio_config.sample_rate, HZ);
                is_timer_started = true;
            }
        }

        /// Setup LED PWM
        virtual void setupPWM(){
            // frequency is driven by selected resolution
            audio_config.pwm_frequency = frequency(audio_config.resolution) * 1000;

            pins.resize(audio_config.channels);
            for (int j=0;j<audio_config.channels;j++){
                pins[j].gpio = audio_config.pins()[j];
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0 , 0)
                ledcAttach(pins[j].gpio, audio_config.pwm_frequency, audio_config.resolution);
#else
                int pwmChannel = j;
                pins[j].pwm_channel = pwmChannel;
                ledcSetup(pins[j].pwm_channel, audio_config.pwm_frequency, audio_config.resolution);
                ledcAttachPin(pins[j].gpio, pins[j].pwm_channel);
#endif
                LOGI("setupPWM: pin=%d, channel=%d, frequency=%d, resolution=%d", pins[j].gpio, pins[j].pwm_channel, audio_config.pwm_frequency, audio_config.resolution);
            }
            logPins();
        }

        void logPins(){
            for (int j=0;j<pins.size();j++){
                LOGI("pin%d: %d", j, pins[j].gpio);
            }
        }

        /// Setup ESP32 timer with callback
        virtual void setupTimer() {
            timer.setCallbackParameter(this);
            timer.setIsSave(false);
        } 

        /// write a pwm value to the indicated channel. The max value depends on the resolution
        virtual void pwmWrite(int channel, int value){
            ledcWrite(pins[channel].pwm_channel, value);  
        }

    protected:
        Vector<PinInfo> pins;      
        TimerAlarmRepeating timer;

        /// provides the max value for the indicated resulution
        int maxUnsignedValue(int resolution){
            return pow(2,resolution);
        } 

        virtual int maxChannels() {
            return 16;
        };

        /// provides the max value for the configured resulution
        virtual int maxOutputValue(){
            return maxUnsignedValue(audio_config.resolution);
        }
        
        /// determiens the PWM frequency based on the requested resolution
        float frequency(int resolution) {
            switch (resolution){
                case 8: return 312.5;
                case 9: return 156.25;
                case 10: return 78.125;
                case 11: return 39.0625;
            }
            return 312.5;
        }    

        /// timer callback: write the next frame to the pins
        static void pwm_callback(void*ptr){
            PWMDriverESP32 *accessAudioPWM =  (PWMDriverESP32*)ptr;
            if (accessAudioPWM!=nullptr){
                accessAudioPWM->playNextFrame();
            }
        }

};


}

#endif

