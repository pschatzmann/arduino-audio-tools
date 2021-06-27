
#pragma once
#include "AudioConfig.h"
#ifdef ESP32
#include "AudioTools/AudioLogger.h"
#include "AudioTools/Vector.h"
#include "Stream.h"

#define PWM_BUFFER_LENGTH 512

namespace audio_tools {

enum PWMResolution {Res8,Res9,Res10,Res11};
void defaultAudioOutputCallback();
class AudioPWM;
static AudioPWM *accessAudioPWM; 

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
 * @author Phil Schatzmann
 * @copyright GPLv3

 */

struct PWMConfig {
    int sample_rate = 10000;  // sample rate in Hz
    int channels = 2;
    int start_pin = 3; 
    int buffer_size = 1024 * 8;
    int bits_per_sample = 16;
    int resolution = 8;  // must be between 8 and 11
} default_config;

/**
 * @brief Information for a PIN
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PINInfo {
    int pwm_channel;
    int gpio;
};

/**
 * @brief Audio output to PWM pins for the ESP32.  
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioPWM : public Stream {
    friend void defaultAudioOutputCallback();

    public:

        AudioPWM(){
            accessAudioPWM = this;
        }

        PWMConfig defaultConfig() {
            return default_config;
        }

        PWMConfig config() {
            return audio_config;
        }

        // starts the processing
        virtual void begin(PWMConfig config){
	 		LOGD(__FUNCTION__);
            this->audio_config = config;
            LOGI("sample_rate: %d", audio_config.sample_rate);
            LOGI("channels: %d", audio_config.channels);
            LOGI("start_pin: %d", audio_config.start_pin);

            setupPWM();
            setupTimer();
        }

        // Ends the output
        virtual void end(){
	 		LOGD(__FUNCTION__);
            timerAlarmDisable(timer);
            for (int j=0;j<audio_config.channels;j++){
                ledcDetachPin(pins[j].gpio);
            }
            has_data = false;
        }

        // not supported
        virtual int available() {
            LOGE("not supported");
            return 0;
        }

        // not supported
        virtual int read(){
            LOGE("not supported");
            return -1;
        }

        // not supported
        virtual int peek() {
            LOGE("not supported");
            return -1;
        }

        // not supported
        virtual size_t readBytes(char *buffer, size_t length){
            LOGE("not supported");
            return 0;
        }

        virtual int availableForWrite() { 
            return buffer.availableToWrite();
        }

        virtual void flush() { 
        }

        // blocking write for a single byte
        virtual size_t write(uint8_t value) {
            if (buffer.availableToWrite()>1){
                buffer.write(value);
                has_data = true;
            }
        }

        // blocking write for an array: we expect a singed value and convert it into a unsigned 
        virtual size_t write(const uint8_t *wrt_buffer, size_t size){
            while(availableForWrite()<size){
                delay(5);
            }
            size_t result = buffer.writeArray(wrt_buffer, size);
            has_data = true;
            return result;
        }


    protected:
        PWMConfig audio_config;
        Vector<PINInfo> pins;      
        NBuffer<uint8_t> buffer = NBuffer<uint8_t>(DEFAULT_BUFFER_SIZE,4);
        int buffer_idx = 0;
        bool data_write_started = false;
        hw_timer_t * timer = NULL;
        portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
        bool has_data = false;


        void setupPWM(){
            pins.resize(audio_config.channels);
            for (int j=0;j<audio_config.channels;j++){
                int pwmChannel = j;
                pins[j].pwm_channel = pwmChannel;
                pins[j].gpio = audio_config.start_pin + j;
                ledcSetup(pwmChannel, frequency(audio_config.resolution), audio_config.resolution);
                ledcAttachPin(pwmChannel, pins[j].gpio);
            }
        }

        void setupTimer() {
            // Attach timer int at sample rate
            timer = timerBegin(0, 1, true); // Timer at full 40Mhz, no prescaling
            uint64_t counter = 40000000 / audio_config.sample_rate;
            LOGI("timer counter is %lu", counter);
            timerAttachInterrupt(timer, &defaultAudioOutputCallback, true);
            timerAlarmWrite(timer, counter, true); // Timer fires at ~44100Hz [40Mhz / 907]
            timerAlarmEnable(timer);
        } 

        int maxUnsignedValue(){
            return maxUnsignedValue(audio_config.bits_per_sample);
        }

        
        int maxUnsignedValue(int resolution){
            return 2^resolution;
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

        /// writes the next frame to the output pins 
        void playNextFrame(){
            int required = (audio_config.bits_per_sample / 8) * audio_config.channels;
            if (has_data && buffer.available() >= required){
                for (int j=0;j<audio_config.channels;j++){
                    ledcWrite(pins[j].pwm_channel, nextValue());
                }
            }
        } 

        /// determines the next scaled value
        int nextValue() {
            switch(audio_config.bits_per_sample ){
                case 8: {
                    int8_t value;
                    buffer.readArray((uint8_t*)&value,1);
                    return map(value, -maxValue(8), maxValue(8), 0, maxUnsignedValue());
                }
                case 16: {
                    int16_t value;
                    buffer.readArray((uint8_t*)&value,2);
                    return map(value, -maxValue(16), maxValue(16), 0, maxUnsignedValue());
                }
                case 24: {
                    int24_t value;
                    buffer.readArray((uint8_t*)&value,3);
                    return map((int32_t)value, -maxValue(24), maxValue(24), 0, maxUnsignedValue());
                }
                case 32: {
                    int32_t value;
                    buffer.readArray((uint8_t*)&value,4);
                    return map(value, -maxValue(32), maxValue(32), 0, maxUnsignedValue());
                }
            }            
            LOGE("nextValue could not be determined because bits_per_sample is not valid: ",audio_config.bits_per_sample);
            return 0;
        }      
      
};

void IRAM_ATTR defaultAudioOutputCallback() {
    if (accessAudioPWM!=nullptr){
        portENTER_CRITICAL_ISR(&(accessAudioPWM->timerMux));
        accessAudioPWM->playNextFrame();
        portEXIT_CRITICAL_ISR(&(accessAudioPWM->timerMux));
    }
}

} // Namespace


#endif

