#pragma once

#include "AudioConfig.h"
#include "AudioTools.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioOutput.h"
#include "AudioBasic/Vector.h"
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
class PWMAudioStreamMBED;

/**
 * @brief Configuration data for PWM audio output
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PWMConfig : public AudioBaseInfo {
    friend class PWMAudioStreamESP32;
    friend class PWMAudioStreamPico;
    friend class PWMAudioStreamMBED;

    PWMConfig(){
        // default basic information
        sample_rate = 10000;  // sample rate in Hz
        channels = 2;
        bits_per_sample = 16;
    }

    // basic pwm information
    uint16_t buffer_size = PWM_BUFFER_SIZE;
    uint8_t buffers = PWM_BUFFERS; 

    // additinal info
    uint32_t pwm_frequency = PWM_FREQUENCY;  // audable range is from 20 to 20,000Hz (not used by ESP32)
    uint8_t resolution = 8;     // Only used by ESP32: must be between 8 and 11 -> drives pwm frequency
    uint8_t timer_id = 0;       // Only used by ESP32 must be between 0 and 3
    
#ifndef __AVR__
    uint16_t start_pin = PIN_PWM_START; 

    // define all pins by passing an array and updates the channels by the number of pins
    template<size_t N> 
    void setPins(int (&array)[N]) {
         LOGD(LOG_METHOD);
        int new_channels = sizeof(array)/sizeof(int);
        if (channels!=new_channels) {
            LOGI("channels updated to %d", new_channels); 
            channels = new_channels;
        }
        pins = array;
        start_pin = -1; // mark start pin as invalid
        LOGI("start_pin: %d", PIN_PWM_START);
    }

#endif

    void logConfig(){
        LOGI("sample_rate: %d", sample_rate);
        LOGI("channels: %d", channels);
        LOGI("bits_per_sample: %d", bits_per_sample);
        LOGI("buffer_size: %u", buffer_size);
        LOGI("pwm_frequency: %u",  (unsigned int)pwm_frequency);
        LOGI("resolution: %d", resolution);
        //LOGI("timer_id: %d", timer_id);
    }

    protected:
        int *pins = nullptr;


} default_config;


/**
 * @brief Common functionality for PWM output
 * 
 */
class PWMAudioStreamBase : public AudioPrint, public AudioBaseInfoDependent {
    public:
        ~PWMAudioStreamBase(){
            if (is_timer_started){
                end();
            }
        }

        PWMConfig defaultConfig() {
            return default_config;
        }

        PWMConfig config() {
            return audio_config;
        }

        // /// Starts the PWMAudio using callbacks
        bool begin(uint16_t sampleRate, uint8_t channels, PWMCallbackType cb) {
             LOGD(LOG_METHOD);
            if (channels>maxChannels()){
                LOGE("Only max %d channels are supported!",maxChannels());
                return false;
            }
            audio_config.channels = channels;
            audio_config.sample_rate = sampleRate;
            user_callback = cb;

            audio_config.logConfig();
            setupPWM();
            setupTimer();

            is_timer_started = true;
            return true;
        }

        /// updates the sample rate dynamically 
        virtual void setAudioInfo(AudioBaseInfo info) {
             LOGI(LOG_METHOD);
            PWMConfig cfg = audio_config;
            if (cfg.sample_rate != info.sample_rate
                || cfg.channels != info.channels
                || cfg.bits_per_sample != info.bits_per_sample) {
                cfg.sample_rate = info.sample_rate;
                cfg.bits_per_sample = info.bits_per_sample;
                cfg.channels = info.channels;
                cfg.logInfo();
                end();
                begin(cfg);        
            }
        }

        /// starts the processing using Streams
        bool begin(PWMConfig config ){
             LOGD(LOG_METHOD);
            this->audio_config = config;
            if (config.channels>maxChannels()){
                LOGE("Only max %d channels are supported!",maxChannels());
                return false;
            }             
            // allocate new buffer
            if (buffer==nullptr) {
                LOGI("Allocating new buffer %d * %d bytes",config.buffers, config.buffer_size);
                buffer = new NBuffer<uint8_t>(config.buffer_size, config.buffers);
            } else {
                buffer->reset();
            }
            // check allocation
            if (buffer==nullptr){
                LOGE("not enough memory to allocate the buffers");
                return false;
            }

            audio_config.logConfig();
            setupPWM();
            setupTimer();

            return true;
        }  

        // restart with prior definitions
        bool begin(){
             LOGD(LOG_METHOD);
            // allocate buffer if necessary
            if (user_callback==nullptr) {
                if (buffer==nullptr) {
                    LOGI("->Allocating new buffer %d * %d bytes",audio_config.buffers, audio_config.buffer_size);
                    buffer = new NBuffer<uint8_t>(audio_config.buffer_size, audio_config.buffers);
                } else {
                    buffer->reset();
                }
            }
            // initialize if necessary
            if (!is_timer_started){
                audio_config.logConfig();
                setupPWM();
                setupTimer();
            }

            // reset class variables
            is_timer_started = true;
            underflow_count = 0;
            underflow_per_second = 0;
            frame_count = 0;
            frames_per_second = 0;     
            
            LOGI("->Buffer available: %d", buffer->available());
            LOGI("->Buffer available for write: %d", buffer->availableForWrite());
            LOGI("->is_timer_started: %s ", is_timer_started ? "true" : "false");
            return true;
        } 

        virtual void end(){
             LOGD(LOG_METHOD);
            is_timer_started = false;
        }

        virtual int availableForWrite() { 
            return buffer==nullptr ? 0 : buffer->availableForWrite();
        }

        virtual void flush() { 
        }

        // blocking write for a single byte
        virtual size_t write(uint8_t value) {
            size_t result = 0;
            if (buffer->availableForWrite()>1){
                result = buffer->write(value);
                startTimer();
            }
            return result;
        }

        // blocking write for an array: we expect a singed value and convert it into a unsigned 
        virtual size_t write(const uint8_t *wrt_buffer, size_t size){
            size_t available = min((size_t)availableForWrite(),size);
            LOGD("write: %u bytes -> %u", (unsigned int)size, (unsigned int)available);
            size_t result = buffer->writeArray(wrt_buffer, available);
            if (result!=available){
                LOGW("Could not write all data: %u -> %d", (unsigned int) size, result);
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
        uint32_t time_1_sec;
        bool is_timer_started = false;

        virtual void setupPWM() = 0;
        virtual void setupTimer() = 0;
        virtual int maxChannels() = 0;
        virtual int maxOutputValue() = 0;


        /// when we get the first write -> we activate the timer to start with the output of data
        virtual void startTimer(){
            if (!is_timer_started){
                 LOGD(LOG_METHOD);
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
             //LOGD(LOG_METHOD);
            uint8_t channels = audio_config.channels;
            int16_t data[channels];
            if (user_callback(channels, data)){
                for (uint8_t j=0;j<audio_config.channels;j++){
                    int value  = map(data[j], -NumberConverter::maxValue(16), NumberConverter::maxValue(16), 0, 255); 
                    pwmWrite(j, value);
                }
                updateStatistics();                
            }
        }


        /// writes the next frame to the output pins 
        void playNextFrameStream(){
            if (is_timer_started){
                //LOGD(LOG_METHOD);
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
            } 
        } 

        void playNextFrame(){
            // LOGD(LOG_METHOD);
            if (user_callback!=nullptr){
                playNextFrameCallback();
            } else {
                playNextFrameStream();
            }
        }

        /// determines the next scaled value
        virtual int nextValue() {
            int result = 0;
            switch(audio_config.bits_per_sample ){
                case 8: {
                    int16_t value = buffer->read();
                    if (value<0){
                        LOGE("Could not read full data");
                        value = 0;
                    }
                    result = map(value, -NumberConverter::maxValue(8), NumberConverter::maxValue(8), 0, maxOutputValue());
                    break;
                }
                case 16: {
                    int16_t value;
                    if (buffer->readArray((uint8_t*)&value,2)!=2){
                        LOGE("Could not read full data");
                    }
                    result = map(value, -NumberConverter::maxValue(16), NumberConverter::maxValue(16), 0, maxOutputValue());
                    break;
                }
                case 24: {
                    int24_t value;
                    if (buffer->readArray((uint8_t*)&value,3)!=3){
                        LOGE("Could not read full data");
                    }
                    result = map((int32_t)value, -NumberConverter::maxValue(24), NumberConverter::maxValue(24), 0, maxOutputValue());
                    break;
                }
                case 32: {
                    int32_t value;
                    if (buffer->readArray((uint8_t*)&value,4)!=4){
                        LOGE("Could not read full data");
                    }
                    result = map(value, -NumberConverter::maxValue(32), NumberConverter::maxValue(32), 0, maxOutputValue());
                    break;
                }
            }        
            return result;
        }      
};

} // ns

