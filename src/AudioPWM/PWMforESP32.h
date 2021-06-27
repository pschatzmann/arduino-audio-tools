
#pragma once
#include "AudioConfig.h"
#ifdef ESP32
#include "AudioTools/AudioLogger.h"
#include "AudioTools/Vector.h"
#include "Stream.h"
#include <math.h>    

namespace audio_tools {

// forward declaration
class PWMAudioStreamESP32;
typedef PWMAudioStreamESP32 AudioPWM;
void defaultPWMAudioOutputCallback();
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

struct PWMConfigESP32 {
    int sample_rate = 10000;  // sample rate in Hz
    int channels = 2;
    int start_pin = 3; 
    int buffer_size = 1024 * 8;
    int bits_per_sample = 16;
    int resolution = 8;  // must be between 8 and 11 -> drives pwm frequency
} default_config;

typedef PWMConfigESP32 PWMConfig;

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

class PWMAudioStreamESP32 : public Stream {
    friend void defaultPWMAudioOutputCallback();

    public:

        PWMAudioStreamESP32(){
            accessAudioPWM = this;
        }

        PWMConfig defaultConfig() {
            return default_config;
        }

        PWMConfig config() {
            return audio_config;
        }

        // starts the processing
        bool begin(PWMConfig config){
	 		LOGD(__FUNCTION__);
            this->audio_config = config;
            LOGI("sample_rate: %d", audio_config.sample_rate);
            LOGI("channels: %d", audio_config.channels);
            LOGI("bits_per_sample: %d", audio_config.bits_per_sample);
            LOGI("start_pin: %d", audio_config.start_pin);
            LOGI("resolution: %d", audio_config.resolution);

            // controller has max 16 independent channels
            if (audio_config.channels>=16){
                LOGE("Only max 16 channels are supported");
                return false;
            }

            // check selected resolution
            if (audio_config.resolution<8 || audio_config.resolution>11){
                LOGE("The resolution must be between 8 and 11!");
                return false;
            }

            setupPWM();
            setupTimer();
            return true;
        }

        // Ends the output
        virtual void end(){
	 		LOGD(__FUNCTION__);
            timerAlarmDisable(timer);
            for (int j=0;j<audio_config.channels;j++){
                ledcDetachPin(pins[j].gpio);
            }
            data_write_started = false;
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
                setWriteStarted();
            }
        }

        // blocking write for an array: we expect a singed value and convert it into a unsigned 
        virtual size_t write(const uint8_t *wrt_buffer, size_t size){
            LOGI("write: %lu bytes", size)
            while(availableForWrite()<size){
       	 	    LOGI("Buffer is full - waiting...");
                delay(10);
            }
            size_t result = buffer.writeArray(wrt_buffer, size);
            if (result!=size){
                LOGW("Could not write all data: %d -> %d", size, result);
            }
            setWriteStarted();
            return result;
        }


    protected:
        PWMConfig audio_config;
        Vector<PinInfo> pins;      
        NBuffer<uint8_t> buffer = NBuffer<uint8_t>(PWM_BUFFER_SIZE, PWM_BUFFERS);
        hw_timer_t * timer = nullptr;
        portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
        bool data_write_started = false;

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
                int pwmChannel = j;
                pins[j].pwm_channel = pwmChannel;
                pins[j].gpio = audio_config.start_pin + j;
                ledcSetup(pwmChannel, frequency(audio_config.resolution), audio_config.resolution);
                ledcAttachPin(pwmChannel, pins[j].gpio);
            }
        }

        /// Setup ESP32 timer with callback
        void setupTimer() {
	 		LOGD(__FUNCTION__);
            // Attach timer int at sample rate
            timer = timerBegin(0, 1, true); // Timer at full 40Mhz, no prescaling
            uint64_t counter = 40000000 / audio_config.sample_rate;
            LOGI("timer counter is %lu", counter);
            timerAttachInterrupt(timer, &defaultPWMAudioOutputCallback, true);
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

        /// writes the next frame to the output pins 
        void playNextFrame(){
            if (data_write_started){
                int required = (audio_config.bits_per_sample / 8) * audio_config.channels;
                if (buffer.available() >= required){
                    for (int j=0;j<audio_config.channels;j++){
                        int value  = nextValue();
                        //Serial.println(value);
                        ledcWrite(pins[j].pwm_channel, value);
                    }
                } else {
                    LOGW("playNextFrame - underflow");
                }
            }
        } 

        /// determines the next scaled value
        int nextValue() {
            switch(audio_config.bits_per_sample ){
                case 8: {
                    int value = buffer.read();
                    if (value<0){
                        LOGE("Could not read full data");
                        value = 0;
                    }
                    return map(value, -maxValue(8), maxValue(8), 0, maxUnsignedValue());
                }
                case 16: {
                    int16_t value;
                    if (buffer.readArray((uint8_t*)&value,2)!=2){
                        LOGE("Could not read full data");
                    }
                    //Serial.print(value);
                    //Serial.print(" -> ");
                    //Serial.println(map(value, -maxValue(16), maxValue(16), 0, maxUnsignedValue()));
                    return map(value, -maxValue(16), maxValue(16), 0, maxUnsignedValue());
                }
                case 24: {
                    int24_t value;
                    if (buffer.readArray((uint8_t*)&value,3)!=3){
                        LOGE("Could not read full data");
                    }
                    return map((int32_t)value, -maxValue(24), maxValue(24), 0, maxUnsignedValue());
                }
                case 32: {
                    int32_t value;
                    if (buffer.readArray((uint8_t*)&value,4)!=4){
                        LOGE("Could not read full data");
                    }
                    return map(value, -maxValue(32), maxValue(32), 0, maxUnsignedValue());
                }
            }            
            LOGE("nextValue could not be determined because bits_per_sample is not valid: ",audio_config.bits_per_sample);
            return 0;
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

