#pragma once

#include "AudioConfig.h"
#ifdef USE_PWM

#include "AudioTools.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioPrint.h"
#include "AudioTools/AudioTypes.h"
#include "AudioBasic/Collections.h"

namespace audio_tools {

// forward declarations
// Callback for user
typedef bool (*PWMCallbackType)(uint8_t channels, int16_t* data);
// Callback used by system
static void defaultPWMAudioOutputCallback();
// Driver classes
class PWMDriverESP32;
class PWMDriverRP2040;
class PWMDriverMBED;
class PWMDriverSTM32;

/**
 * @brief Configuration data for PWM audio output
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PWMConfig : public AudioBaseInfo {

    PWMConfig(){
        // default basic information
        sample_rate = 8000u;  // sample rate in Hz
        channels = 1;
        bits_per_sample = 16;
    }

    // basic pwm information
    uint16_t buffer_size = PWM_BUFFER_SIZE;
    uint8_t buffers = PWM_BUFFERS; 

    // additinal info
    uint16_t pwm_frequency = PWM_AUDIO_FREQUENCY;  // audable range is from 20 to 20,000Hz (not used by ESP32)
    uint8_t resolution = 8;     // Only used by ESP32: must be between 8 and 11 -> drives pwm frequency
    uint8_t timer_id = 0;       // Only used by ESP32 must be between 0 and 3
    
#ifndef __AVR__
    uint16_t start_pin = PIN_PWM_START; 

    /// Defines the pins and the corresponding number of channels (=number of pins)
    void setPins(Pins &pins){
        channels = pins.size();
        pins_data = pins;
    }

    /// Determines the pins (for all channels)
    Pins &pins() {
        if (pins_data.size()==0){
            pins_data.resize(channels);
            for (int j=0;j<channels;j++){
                pins_data[j]=start_pin+j;
            }
        }
        return pins_data;
    }

#endif

    void logConfig(){
        LOGI("sample_rate: %d", sample_rate);
        LOGI("channels: %d", channels);
        LOGI("bits_per_sample: %u", bits_per_sample);
        LOGI("buffer_size: %u", buffer_size);
        LOGI("buffer_count: %u", buffers);
        LOGI("pwm_frequency: %d", pwm_frequency);
        LOGI("resolution: %d", resolution);
        //LOGI("timer_id: %d", timer_id);
    }

    protected:
        Pins pins_data;


} INLINE_VAR default_config;

/**
 * @brief Base Class for all PWM drivers
 * 
 */
class DriverPWMBase {
    public:
        PWMConfig &audioInfo(){
            return audio_config;
        }

        // restart with prior definitions
        bool begin(PWMConfig cfg){
            TRACED();
            audio_config = cfg;;
            if (audio_config.channels>maxChannels()){
                LOGE("Only max %d channels are supported!",maxChannels());
                return false;
            }             
            // allocate buffer if necessary
            if (user_callback==nullptr) {
                if (buffer!=nullptr){
                    delete buffer;
                    buffer = nullptr;
                }
                LOGI("->Allocating new buffer %d * %d bytes",audio_config.buffers, audio_config.buffer_size);
                buffer = new NBuffer<uint8_t>(audio_config.buffer_size, audio_config.buffers);
            }
            
            // initialize if necessary
            if (!isTimerStarted()){
                audio_config.logConfig();
                setupPWM();
                setupTimer();
            }

            // reset class variables
            underflow_count = 0;
            underflow_per_second = 0;
            frame_count = 0;
            frames_per_second = 0;     
            
            LOGI("->Buffer available: %d", buffer->available());
            LOGI("->Buffer available for write: %d", buffer->availableForWrite());
            LOGI("->is_timer_started: %s ", isTimerStarted() ? "true" : "false");
            return true;
        } 

        virtual int availableForWrite() { 
            return buffer==nullptr ? 0 : buffer->availableForWrite();
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

        void setUserCallback(PWMCallbackType cb){
            user_callback = cb;
        }

        bool isTimerStarted() {
            return is_timer_started;
        }

        virtual void setupPWM()  = 0;
        virtual void setupTimer()  = 0;
        virtual void startTimer() = 0;
        virtual int maxChannels() = 0;
        virtual int maxOutputValue() = 0;
        virtual void end() = 0;

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

        void playNextFrameCallback(){
             //TRACED();
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
            if (isTimerStarted() && buffer!=nullptr){
                //TRACED();
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
            // TRACED();
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

#endif
