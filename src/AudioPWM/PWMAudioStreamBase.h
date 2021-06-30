#pragma once

#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTools.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/Vector.h"
#include "Stream.h"

namespace audio_tools {

// forward declarations
// Callback for user
typedef bool (*PWMCallbackType)(uint8_t channels, int16_t* data);
// Callback used by system
void defaultPWMAudioOutputCallback();
// Stream classes
class PWMAudioStreamESP32;
class PWMAudioStreamPico;

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
        LOGI("buffer_size: %d", buffer_size);
    }

#ifdef ESP32
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
 */
    int resolution = 8;     // must be between 8 and 11 -> drives pwm frequency
    int start_pin = 4; 
    uint8_t timer_id = 0;   // must be between 0 and 3
#endif

#ifdef ARDUINO_ARCH_RP2040
    int pwm_freq = 60000;   // audable range is from 20 to 20,000Hz 
    int start_pin = 2;      // channel 0 will be on gpio 2, channel 1 on 3 etc
#endif

// for architectures which support many flexible pins we support setPins
#if defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
    friend class PWMAudioStreamESP32;
    friend class PWMAudioStreamPico;


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

    protected:
        int *pins = nullptr;


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

            is_timer_started = true;
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
                LOGI("Allocating new buffer %d * %d bytes",config.buffers, config.buffer_size);
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
                startTimer();
            }
            return result;
        }

        // blocking write for an array: we expect a singed value and convert it into a unsigned 
        virtual size_t write(const uint8_t *wrt_buffer, size_t size){
            LOGD("write: %lu bytes", size)
            bool log_flag = true;
            while(availableForWrite()<size){
                if (log_flag) LOGI("Buffer is full - waiting...");
                log_flag = false;
                delay(10);
            }
            size_t result = buffer->writeArray(wrt_buffer, size);
            if (result!=size){
                LOGW("Could not write all data: %d -> %d", size, result);
            }
            // activate the timer now - if not already done
            startTimer();
            return result;
        }

        // When the timer does not have enough data we increase the underflow_count;
        uint32_t underflowsPerSecond(){
            return underflow_per_second;
        }
        // provides the effectivly measured output frames per second
        uint32_t framesPerSecond(){
            return frames_per_second;
        }


        virtual void pwmWrite(int channel, int value) = 0;

    protected:
        PWMConfig audio_config;
        NBuffer<uint8_t> *buffer = nullptr;
        PWMCallbackType user_callback = nullptr;
        uint32_t underflow_count = 0;
        uint32_t underflow_per_second = 0;
        uint32_t frame_count = 0;
        uint32_t frames_per_second = 0;
        uint64_t time_1_sec;
        bool is_timer_started = false;

        virtual void logConfig() {
            audio_config.logConfig();
        }

        virtual void setupPWM() = 0;
        virtual void setupTimer() = 0;
        virtual int maxChannels() = 0;
        virtual int maxOutputValue() = 0;


        /// when we get the first write -> we activate the timer to start with the output of data
        virtual void startTimer(){
            if (!is_timer_started){
        	 	LOGD(__FUNCTION__);
                is_timer_started = true;
            }
        }

        inline void updateStatistics(){
            frame_count++;
            if (millis()>=time_1_sec){
                time_1_sec = millis()+1000;
                frames_per_second = frame_count;
                underflow_per_second = underflow_count;
                underflow_count = 0;
                frame_count = 0;
            }
        }


        void playNextFrameCallback(){
	 		//LOGD(__FUNCTION__);
            uint8_t channels = audio_config.channels;
            int16_t data[channels];
            if (user_callback(channels, data)){
                for (uint8_t j=0;j<audio_config.channels;j++){
                    int value  = map(data[j], -maxValue(16), maxValue(16), 0, 255); 
                    pwmWrite(j, value);
                }
                updateStatistics();                
            }
        }


        /// writes the next frame to the output pins 
        void playNextFrameStream(){
            if (is_timer_started){
	 		    //LOGD(__FUNCTION__);
                int required = (audio_config.bits_per_sample / 8) * audio_config.channels;
                if (buffer->available() >= required){
                    for (int j=0;j<audio_config.channels;j++){
                        int value  = nextValue();
                        pwmWrite(j, value);
                    }
                } else {
                    underflow_count++;
                }
                updateStatistics();
            } else {
                //LOGE("is_timer_started is false");
            }
        } 

        void playNextFrame(){
    	 	//LOGD(__FUNCTION__);
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
                    result = map(value, -maxValue(8), maxValue(8), 0, maxOutputValue());
                    break;
                }
                case 16: {
                    int16_t value;
                    if (buffer->readArray((uint8_t*)&value,2)!=2){
                        LOGE("Could not read full data");
                    }
                    result = map(value, -maxValue(16), maxValue(16), 0, maxOutputValue());
                    break;
                }
                case 24: {
                    int24_t value;
                    if (buffer->readArray((uint8_t*)&value,3)!=3){
                        LOGE("Could not read full data");
                    }
                    result = map((int32_t)value, -maxValue(24), maxValue(24), 0, maxOutputValue());
                    break;
                }
                case 32: {
                    int32_t value;
                    if (buffer->readArray((uint8_t*)&value,4)!=4){
                        LOGE("Could not read full data");
                    }
                    result = map(value, -maxValue(32), maxValue(32), 0, maxOutputValue());
                    break;
                }
            }        
            return result;
        }      
};

} // ns
