#pragma once

#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTools.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/Vector.h"
#include "Stream.h"

namespace audio_tools {

typedef bool (*PWMCallbackType)(uint8_t channels, int16_t* data);
void defaultPWMAudioOutputCallback();
/**
 * PWMConfigAVR
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PWMConfig {
    uint16_t sample_rate = 10000;  // sample rate in Hz
    uint8_t channels = 2;
    uint8_t bits_per_sample = 16;
    uint16_t buffer_size = PWM_BUFFER_SIZE;
    uint8_t buffers = PWM_BUFFERS; 

    void logConfig(){
        LOGI("sample_rate: %d", sample_rate);
        LOGI("channels: %d", channels);
        LOGI("bits_per_sample: %d", bits_per_sample);
    }

#ifdef ESP32
    int resolution = 8;  // must be between 8 and 11 -> drives pwm frequency
    int start_pin = 3; 
    int *pins = nullptr;

    // define all pins by passing an array and updates the channels by the number of pins
    template<size_t N> 
    void setPins(int (&array)[N]) {
	 	LOGD(__FUNCTION__);
        int new_channels = sizeof(array)/sizeof(int);
        if (channels!=new_channels) {
            LOGI("channels updated to %d", new_channels); 
            channels = new_channels;
        }
        pins = array;
        start_pin = -1; // mark start pin as invalid
    }
#endif

#ifdef ARDUINO_ARCH_RP2040
    int pwm_freq = 60000;    // audable range is from 20 to 20,000Hz 
    int start_pin = 2; // channel 0 will be on gpio 2, channel 1 on 3 etc
#endif

} default_config;


/**
 * @brief Common functionality for PWM output
 * 
 */
class PWMAudioStreamBase : public Stream {
    public:

        PWMConfig defaultConfig() {
            return default_config;
        }

        PWMConfig config() {
            return audio_config;
        }

        virtual int maxChannels() = 0;

        /// Starts the PWMAudio using callbacks
        bool begin(uint16_t sampleRate, uint8_t channels, PWMCallbackType cb) {
	 		LOGD(__FUNCTION__);
            if (channels>maxChannels()){
                LOGE("Only max %d channels are supported!",maxChannels());
                return false;
            }
            audio_config.channels = channels;
            audio_config.sample_rate = sampleRate;
            user_callback = cb;
            logConfig();

            setupPWM();
            setupTimer();

            data_write_started = true;
            return true;
        }


        /// starts the processing using Streams
        bool begin(PWMConfig config ){
	 		LOGD(__FUNCTION__);
            this->audio_config = config;
            if (config.channels>maxChannels()){
                LOGE("Only max %d channels are supported!",maxChannels());
                return false;
            }             
            // allocate new buffer
            if (buffer==nullptr) {
                buffer = new NBuffer<uint8_t>(config.buffer_size, config.buffers);
            }
            // check allocation
            if (buffer==nullptr){
                LOGE("not enough memory to allocate the buffers");
                return false;
            }

            logConfig();
            setupPWM();
            setupTimer();

            return true;
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
            return buffer==nullptr ? 0 : buffer->availableToWrite();
        }

        virtual void flush() { 
        }

        // blocking write for a single byte
        virtual size_t write(uint8_t value) {
            size_t result = 0;
            if (buffer->availableToWrite()>1){
                result = buffer->write(value);
                setWriteStarted();
            }
            return result;
        }

        // blocking write for an array: we expect a singed value and convert it into a unsigned 
        virtual size_t write(const uint8_t *wrt_buffer, size_t size){
            LOGI("write: %lu bytes", size)
            while(availableForWrite()<size){
       	 	    LOGI("Buffer is full - waiting...");
                delay(10);
            }
            size_t result = buffer->writeArray(wrt_buffer, size);
            if (result!=size){
                LOGW("Could not write all data: %d -> %d", size, result);
            }
            // activate the timer now - if not already done
            setWriteStarted();
            return result;
        }

        // When the timer does not have enough data we increase the underflow_count;
        uint64_t underflowsPerSecond(){
            return underflow_count;
        }

        uint64_t frameCount(){
            return frame_count;
        }

        virtual void pwmWrite(int channel, int value) = 0;

    protected:
        NBuffer<uint8_t> *buffer;
        bool data_write_started = false;
        uint64_t underflow_count = 0;
        uint64_t frame_count = 0;
        PWMCallbackType user_callback = nullptr;
        PWMConfig audio_config;

        virtual void logConfig() {
            audio_config.logConfig();
        }

        virtual void setupPWM() = 0;
        
        virtual void setupTimer() = 0;


        /// when we get the first write -> we activate the timer to start with the output of data
        virtual void setWriteStarted(){
            if (!data_write_started){
                LOGI("timerAlarmEnable");
                data_write_started = true;
            }
        }

        void playNextFrameCallback(){
            uint8_t channels = audio_config.channels;
            int16_t data[channels];
            if (user_callback(channels, data)){
                frame_count++;
                for (uint8_t j=0;j<audio_config.channels;j++){
                    int value  = map(data[j], -maxValue(16), maxValue(16), 0, 255); 
                    pwmWrite(j, value);
                }                
            }
        }

        /// writes the next frame to the output pins 
        void playNextFrameStream(){
            static long underflow_time = millis()+1000;
            if (data_write_started){
                int required = (audio_config.bits_per_sample / 8) * audio_config.channels;
                if (buffer->available() >= required){
                    underflow_count = 0;
                    frame_count++;
                    for (int j=0;j<audio_config.channels;j++){
                        int value  = nextValue();
                        Serial.println(value);
                        pwmWrite(j, value);
                    }
                } else {
                    underflow_count++;
                }

                if (underflow_time>millis()){
                    underflow_time = millis()+1000;
                    underflow_count = 0;
                }
            }
        } 

        void playNextFrame(){
            if (user_callback!=nullptr){
                playNextFrameCallback();
            } else {
                playNextFrameStream();
            }
        }


        /// determines the next scaled value
        virtual int nextValue() {
            int result;
            switch(audio_config.bits_per_sample ){
                case 8: {
                    int16_t value = buffer->read();
                    if (value<0){
                        LOGE("Could not read full data");
                        value = 0;
                    }
                    result = map(value, -maxValue(8), maxValue(8), 0, 255);
                    break;
                }
                case 16: {
                    int16_t value;
                    if (buffer->readArray((uint8_t*)&value,2)!=2){
                        LOGE("Could not read full data");
                    }
                    result = map(value, -maxValue(16), maxValue(16), 0, 255);
                    break;
                }
                case 24: {
                    int24_t value;
                    if (buffer->readArray((uint8_t*)&value,3)!=3){
                        LOGE("Could not read full data");
                    }
                    result = map((int32_t)value, -maxValue(24), maxValue(24), 0, 255);
                    break;
                }
                case 32: {
                    int32_t value;
                    if (buffer->readArray((uint8_t*)&value,4)!=4){
                        LOGE("Could not read full data");
                    }
                    result = map(value, -maxValue(32), maxValue(32), 0, 255);
                    break;
                }
            }        
            return result;
        }      
};

} // ns
