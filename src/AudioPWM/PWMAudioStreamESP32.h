
#pragma once
#ifdef ESP32
#include "AudioPWM/PWMAudioStreamBase.h"


namespace audio_tools {

// forward declaration
class PWMAudioStreamESP32;
typedef PWMAudioStreamESP32 PWMAudioStream;
static PWMAudioStreamESP32 *accessAudioPWM = nullptr; 

/**
 * @brief Configuration for PWM output
 *   RES | BITS | MAX_FREQ (kHz)
 *   ----|------|-------
 *    8  | 256  | 312.5
 *    9  | 512  | 156.25
 *   10  | 1024 | 78.125
 *   11  | 2048 | 39.0625
 *
 *   The default resolution is 8. The value must be between 8 and 11 and also drives the PWM frequency.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3

 */


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

        virtual int maxChannels() {
            return 16;
        };

        // Ends the output
        virtual void end(){
	 		LOGD(__FUNCTION__);
            timerAlarmDisable(timer);
            for (int j=0;j<audio_config.channels;j++){
                ledcDetachPin(pins[j].gpio);
            }
            data_write_started = false;
        }


    protected:
        Vector<PinInfo> pins;      
        hw_timer_t * timer = nullptr;
        portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

        /// when we get the first write -> we activate the timer to start with the output of data
        void setWriteStarted(){
            if (!data_write_started){
                LOGI("timerAlarmEnable");
                data_write_started = true;
                timerAlarmEnable(timer);
            }
        }

        /// Setup LED PWM
        void setupPWM(){
            LOGD(__FUNCTION__);

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
                LOGD("-> ledcSetup");
                ledcSetup(pwmChannel, frequency(audio_config.resolution), audio_config.resolution);
                LOGD("-> ledcAttachPin");
                ledcAttachPin(pins[j].gpio, pwmChannel);
            }
        }

        /// Setup ESP32 timer with callback
        void setupTimer() {
	 		LOGD(__FUNCTION__);

            // Attach timer int at sample rate
            timer = timerBegin(0, 1, true); // Timer at full 40Mhz, no prescaling
            uint64_t counter = 40000000 / audio_config.sample_rate;
            LOGI("-> timer counter is %lu", counter);
            LOGD("-> timerAttachInterrupt");
            timerAttachInterrupt(timer, &defaultPWMAudioOutputCallback, true);
            LOGD("-> timerAlarmWrite");
            timerAlarmWrite(timer, counter, true); // Timer fires at ~44100Hz [40Mhz / 907]
        } 

        /// provides the max value for the configured resulution
        int maxUnsignedValue(){
            return maxUnsignedValue(audio_config.resolution);
        }
        
        /// provides the max value for the indicated resulution
        int maxUnsignedValue(int resolution){
            return pow(2,resolution);
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
        void pwmWrite(int channel, int value){
            ledcWrite(pins[channel].pwm_channel, value);          
        }


        /// determines the next scaled value
        int nextValue() {
            //static long counter=0; static int min_value=INT_MAX; static int max_value=INT_MIN;
            //counter++;

            int result;
            switch(audio_config.bits_per_sample ){
                case 8: {
                    int value = buffer->read();
                    if (value<0){
                        LOGE("Could not read full data");
                        value = 0;
                    }
                    result = map(value, -maxValue(8), maxValue(8), 0, maxUnsignedValue());
                    break;
                }
                case 16: {
                    int16_t value;
                    if (buffer->readArray((uint8_t*)&value,2)!=2){
                        LOGE("Could not read full data");
                    }
                    result = map(value, -maxValue(16), maxValue(16), 0, maxUnsignedValue());
                    break;
                }
                case 24: {
                    int24_t value;
                    if (buffer->readArray((uint8_t*)&value,3)!=3){
                        LOGE("Could not read full data");
                    }
                    result = map((int32_t)value, -maxValue(24), maxValue(24), 0, maxUnsignedValue());
                    break;
                }
                case 32: {
                    int32_t value;
                    if (buffer->readArray((uint8_t*)&value,4)!=4){
                        LOGE("Could not read full data");
                    }
                    result = map(value, -maxValue(32), maxValue(32), 0, maxUnsignedValue());
                    break;
                }
            }        
            return result;
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

} // Namespace


#endif

