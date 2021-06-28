
#pragma once
#include "Stream.h"
#include "AudioConfig.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/Vector.h"

#ifdef ARDUINO_ARCH_RP2040
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/time.h"
#include <limits>       // std::numeric_limits

namespace audio_tools {

// forwrd declaratioin of callback
class PWMAudioStreamPico;
bool defaultAudioOutputCallback(repeating_timer* ptr);
typedef PWMAudioStreamESP32 AudioPWM;

/**
 * @brief Rasperry Pico Channel to pin assignments
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PicoChannelOut {
    int gpio = -1;
    int audioChannel;
    uint slice; // pico pwm slice
    uint channel; // pico pwm channel
};

/**
 * @brief Configuration for Rasperry Pico PWM output
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PWMConfigPico {
    int sample_rate = 10000;  // sample rate in Hz
    int channels = 2;
    int pwm_freq = 60000;    // audable range is from 20 to 20,000Hz 
    int amplitude_out = 127; // amplitude of square wave (pwm values -amplitude to amplitude) for one byte
    int amplitude_in = 0; 
    int start_pin = 2; // channel 0 will be on gpio 2, channel 1 on 3 etc

    int maxChannels() {
        return 16;
    }    
} default_config;

typedef PWMConfigPico PWMConfig;

/**
 * @brief Audio output for the Rasperry Pico to PWM pins.
   The Raspberry Pi Pico has 8 PWM blocks/slices(1-8) and each PWM block provides up to two PWM outputs(A-B). 
 * @author Phil Schatzmann
 * @copyright GPLv3

 */

template <class T>
class PWMAudioStreamPico : public Stream {
    friend bool defaultAudioOutputCallback(repeating_timer* ptr);

    public:

        PWMAudioStreamPico(){
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
            this->audio_config = config;

	 		LOGD(__FUNCTION__);
            LOGI("sample_rate: %d", audio_config.sample_rate);
            LOGI("channels: %d", audio_config.channels);
            LOGI("pwm_freq: %d", audio_config.pwm_freq);
            LOGI("start_pin: %d", audio_config.start_pin);
            LOGI("amplitude_out: %d", audio_config.amplitude_out);
            LOGI("amplitude_in: %d", audio_config.amplitude_in);

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
        Vector<PicoChannelOut> pins;      
        NBuffer<T> buffer = NBuffer<T>(PWM_BUFFER_SIZE, PWM_BUFFERS);
        repeating_timer_t timer;
        uint64_t underflow_count = 0;
        bool data_write_started = false;

        // setup pwm config and all pins
        void setupPWM(){
	 		LOGD(__FUNCTION__);
            pwm_config cfg = setupPWMConfig();
            
            // initialize empty pins
            PicoChannelOut empty;
            pins.resize(audio_config.channels, empty);

            // setup pin values
            for (int j;j< audio_config.channels;j++) {
                int gpio = audio_config.start_pin + j;
                int channel = j;
                pins[channel].slice = pwm_gpio_to_slice_num(gpio);
                pins[channel].channel = pwm_gpio_to_channel(gpio);
                pins[channel].audioChannel = j;
                pins[channel].gpio = gpio;
                setupPWMPin(cfg, pins[channel]);
            }
        }

        // defines the pwm_config which will be used to drive the pins
        pwm_config setupPWMConfig() {
	 		LOGD(__FUNCTION__);
            // setup pwm frequency
            pwm_config pico_pwm_config = pwm_get_default_config();
            float pwmClockDivider = static_cast<float>(clock_get_hz(clk_sys)) / (audio_config.pwm_freq * audio_config.amplitude_out * 2);
            LOGI("clock speed is %f", static_cast<float>(clock_get_hz(clk_sys)));
            LOGI("divider is %f", pwmClockDivider);
            pwm_config_set_clkdiv(&pico_pwm_config, pwmClockDivider);
            pwm_config_set_clkdiv_mode(&pico_pwm_config, PWM_DIV_FREE_RUNNING);
            pwm_config_set_phase_correct(&pico_pwm_config, true);
            pwm_config_set_wrap (&pico_pwm_config, audio_config.amplitude_out);
            return pico_pwm_config;
        } 

        // set up pwm 
        void setupPWMPin(pwm_config &cfg, PicoChannelOut &pinInfo){
	 		LOGD("%s for gpio %d",__FUNCTION__, pinInfo.gpio);
            // setup pwm pin  
            int gpio = pinInfo.gpio;
            gpio_set_function(gpio, GPIO_FUNC_PWM);
            pinInfo.slice = pwm_gpio_to_slice_num(gpio);
            pinInfo.channel = pwm_gpio_to_channel(gpio);
            pwm_init(pinInfo.slice, &cfg, true);

            // set initial output value 
            pwm_set_chan_level(pinInfo.slice, pinInfo.channel, 0); 
        }

        void setupTimer(){
	 		LOGD(__FUNCTION__);
            // setup timer
            uint32_t time = 1000000.0 / audio_config.sample_rate;
            LOGI("Timer value %d us", time);

            if (!add_repeating_timer_us(-time, defaultAudioOutputCallback, nullptr, &timer)){
                LOGI("Error: alarm_pool_add_repeating_timer_us failed; no alarm slots available");
            }
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

