
#pragma once
#ifdef ESP32
#include "AudioPWM/PWMAudioBase.h"


namespace audio_tools {

// forward declaration
class PWMAudioStreamESP32;
typedef PWMAudioStreamESP32 PWMAudioStream;
static PWMAudioStreamESP32 *accessAudioPWM = nullptr; 


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
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PWMAudioStreamESP32 : public PWMAudioStreamBase {
    friend void defaultPWMAudioOutputCallback();
    public:

        PWMAudioStreamESP32(){
            LOGD("PWMAudioStreamESP32");
            accessAudioPWM = this;
        }

        // Ends the output
        virtual void end(){
            LOGD(LOG_METHOD);
            timerAlarmDisable(timer);
            for (int j=0;j<audio_config.channels;j++){
                ledcDetachPin(pins[j].gpio);
            }
            is_timer_started = false;
        }


    protected:
        Vector<PinInfo> pins;      
        hw_timer_t * timer = nullptr;
        portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

        /// when we get the first write -> we activate the timer to start with the output of data
        virtual void startTimer(){
            if (!is_timer_started){
                LOGI("timerAlarmEnable");
                is_timer_started = true;
                timerAlarmEnable(timer);
            }
        }

        /// Setup LED PWM
        virtual void setupPWM(){
            LOGD(LOG_METHOD);
            // frequency is driven by selected resolution
            uint32_t freq = frequency(audio_config.resolution)*1000;
            audio_config.pwm_frequency = freq;

            pins.resize(audio_config.channels);
            for (int j=0;j<audio_config.channels;j++){
                LOGD("Processing channel %d", j);
                int pwmChannel = j;
                pins[j].pwm_channel = pwmChannel;
                if (audio_config.pins==nullptr){
                    // use sequential pins starting from start_pin
                    LOGD("-> defining pin %d",audio_config.start_pin + j);
                    pins[j].gpio = audio_config.start_pin + j;
                } else {
                    // use defined pins
                    LOGD("-> defining pin %d",audio_config.pins[j]);
                    pins[j].gpio = audio_config.pins[j];
                }
                LOGD("-> ledcSetup:  frequency=%d / resolution=%d", freq, audio_config.resolution);
                ledcSetup(pwmChannel, freq, audio_config.resolution);
                LOGD("-> ledcAttachPin: %d", pins[j].gpio);
                ledcAttachPin(pins[j].gpio, pwmChannel);
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
             LOGD(LOG_METHOD);

            // Attach timer int at sample rate
            int prescale = 2;
            bool rising_edge = true;
            timer = timerBegin(audio_config.timer_id, prescale, rising_edge); // Timer at full 40Mhz,  prescaling 2
            uint32_t counter = 20000000 / audio_config.sample_rate;
            LOGI("-> timer counter is %zu", counter);
            LOGD("-> timerAttachInterrupt");
            bool interrupt_edge_type = true;
            timerAttachInterrupt(timer, &defaultPWMAudioOutputCallback, interrupt_edge_type);
            LOGD("-> timerAlarmWrite");
            bool auto_reload = true;
            timerAlarmWrite(timer, counter, auto_reload); // Timer fires at ~44100Hz [40Mhz / 907]
        } 

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

        /// write a pwm value to the indicated channel. The max value depends on the resolution
        virtual void pwmWrite(int channel, int value){
            ledcWrite(pins[channel].pwm_channel, value);          
        }
      
};

/// timer callback: write the next frame to the pins
void IRAM_ATTR defaultPWMAudioOutputCallback() {
    if (accessAudioPWM!=nullptr){
        portENTER_CRITICAL_ISR(&(accessAudioPWM->timerMux));
        accessAudioPWM->playNextFrame();
        portEXIT_CRITICAL_ISR(&(accessAudioPWM->timerMux));
    }
}

}

#endif

