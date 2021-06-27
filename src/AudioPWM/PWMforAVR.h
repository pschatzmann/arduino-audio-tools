
#pragma once
#include "Stream.h"
#include "AudioConfig.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/Vector.h"

#ifdef __AVR__
#include <limits>       // std::numeric_limits

namespace audio_tools {

// forwrd declaratioin of callback
bool defaultAudioOutputCallback(repeating_timer* ptr);

/**
 * @brief Configuration for PWM output
 * 
 */
struct PWMConfig {
    int sample_rate = 10000;  // sample rate in Hz
    int channels = 2;
    int pwm_freq = 60000;    // audable range is from 20 to 20,000Hz 
    int amplitude_out = 127; // amplitude of square wave (pwm values -amplitude to amplitude) for one byte
    int amplitude_in = 0; 
    int start_pin = 2; // channel 0 will be on gpio 2, channel 1 on 3 etc
} default_config;

/**
 * @brief Audio output to PWM pins
 * 
 */

template <class T>
class AudioPWM : public Stream {
    friend bool defaultAudioOutputCallback(repeating_timer* ptr);

    public:

        AudioPWM(){
            T amplitude_in = getDefaultAmplitude();
            audio_config.amplitude_in = amplitude_in;
            default_config.amplitude_in = amplitude_in;
        }

        PWMConfig defaultConfig() {
            return default_config;
        }

        PWMConfig config() {
            return audio_config;
        }

        // starts the processing
        void begin(PWMConfig config){
	 		LOGD(__FUNCTION__);
            this->audio_config = config;
            LOGI("sample_rate: %d", audio_config.sample_rate);
            LOGI("channels: %d", audio_config.channels);
            LOGI("pwm_freq: %d", audio_config.pwm_freq);
            LOGI("start_pin: %d", audio_config.start_pin);
            LOGI("amplitude_out: %d", audio_config.amplitude_out);
            LOGI("amplitude_in: %d", audio_config.amplitude_in);

            setupPins();
            setupPWM();
            setupTimer();
        }

        // Ends the output
        void end(){
	 		LOGD(__FUNCTION__);
            cancel_repeating_timer(&timer);
            for(auto pin : pins) {
                if (pin.gpio!=-1){
                    pwm_set_enabled(pin.slice, false);
                }
            } 
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
            LOGE("not supported");
            return -1;
        }

        // blocking write for an array: we expect a singed value and convert it into a unsigned 
        virtual size_t write(const uint8_t *wrt_buffer, size_t size){
            int len = size/sizeof(T);
            T* ptr = (T*) wrt_buffer;
            for (int j=0;j<len;j++){
                int32_t value = static_cast<float>(ptr[j] / (audio_config.amplitude_in / audio_config.amplitude_out)) + audio_config.amplitude_out; 
                // try to write value into buffer
                while(buffer.write(value)==0)
                    delay(5);
            } 
            data_write_started = true;
            return size;
        }

        // number of times we did not have enough data for output
        uint64_t underflowCount(){
            return underflow_count;
        }


    protected:
        PWMConfig audio_config;
        Vector<int> pins;      
        NBuffer<T> buffer = NBuffer<T>(DEFAULT_BUFFER_SIZE,4);
        repeating_timer_t timer;
        uint64_t underflow_count = 0;
        bool data_write_started = false;

        // setup pwm config and all pins
        void setupPins(){
	 		LOGD(__FUNCTION__);            
            // initialize empty pins
            pins.resize(audio_config.channels, empty);
            // setup pin values
            for (int j;j< audio_config.channels;j++) {
                int gpio = audio_config.start_pin + j;
                pins[j]=gpio;
                pinMode(gpio, OUTPUT);
            }
        }

        void setupPWM(){

        }    

        void setupTimer(){

        }




        // Output of the next frame - called by the timer callback
        void playNextFrame(){
            if (data_write_started){
                for (int j=0;j<audio_config.channels;j++){
                    if (buffer.available()>0){
                        pwm_set_chan_level(pins[j].slice, pins[j].channel, buffer.read());
                    } else {
                        underflow_count++;
                    }
                }
            }
        }

        // determines the max amplitude for the selected data type
        T getDefaultAmplitude() {
            std::numeric_limits<T> limits;
            return limits.max();
        }       

};


// timed output executed at the sampleRate
bool defaultAudioOutputCallback(repeating_timer* ptr) {
    AudioPWM<int16_t> *self = (AudioPWM<int16_t> *)  ptr->user_data;
    self->playNextFrame();
    return true;
}

} // Namespace


#endif

