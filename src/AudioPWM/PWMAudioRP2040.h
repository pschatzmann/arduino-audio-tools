
#pragma once
#if defined(RP2040_HOWER) 
#include "AudioPWM/PWMAudioBase.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/time.h"
#include "hardware/structs/clocks.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

namespace audio_tools {

// forwrd declaratioin of callback
class PWMDriverRP2040;
/**
 * @typedef  DriverPWMBase
 * @brief Please use DriverPWMBase!
 */
using PWMDriver = PWMDriverRP2040;

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
 * @brief Audio output for the Rasperry Pico to PWM pins.
   The Raspberry Pi Pico has 8 PWM blocks/slices(1-8) and each PWM block provides up to two PWM outputs(A-B). 
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3

 */

class PWMDriverRP2040 : public DriverPWMBase {
    //friend bool defaultPWMAudioOutputCallbackPico(repeating_timer* ptr);

    public:

        PWMDriverRP2040(){
            TRACED();
        }

        /// Ends the output -> resets the timer and the pins
        void end() override {
            TRACED();
            ticker.end(); // it does not hurt to call this even if it has not been started
            is_timer_started = false;
            for(auto pin : pins) {
                if (pin.gpio!=-1){
                    pwm_set_enabled(pin.slice, false);
                }
            } 
        }

    protected:
        Vector<PicoChannelOut> pins;      
        TimerAlarmRepeating ticker;

        virtual void startTimer() override {
            if (!is_timer_started){
                TRACED();
                long wait_time = 1000000l / audio_config.sample_rate;
                ticker.setCallbackParameter(this);
                ticker.begin(defaultPWMAudioOutputCallbackPico, wait_time, US);
                is_timer_started = true;
            }
            is_timer_started = true;
        }

        // setup pwm config and all pins
        void setupPWM() override {
            TRACED();
            pwm_config cfg = setupPWMConfig();
            
            // initialize empty pins
            PicoChannelOut empty;
            pins.resize(audio_config.channels, empty);

            // setup pin values
            for (int j=0;j< audio_config.channels;j++) {
                int channel = j;
                int gpio = audio_config.pins()[j];
                LOGI("PWM pin %d",gpio);
                pins[channel].slice = pwm_gpio_to_slice_num(gpio);
                pins[channel].channel = pwm_gpio_to_channel(gpio);
                pins[channel].audioChannel = j;
                pins[channel].gpio = gpio;

                setupPWMPin(cfg, pins[channel]);
            }
        }

        // defines the pwm_config which will be used to drive the pins
        pwm_config setupPWMConfig() {
             TRACED();
            // setup pwm frequency
            pwm_config pico_pwm_config = pwm_get_default_config();
            int wrap_value = maxOutputValue(); // amplitude of square wave (pwm values -amplitude to amplitude) for one byte
            float pwmClockDivider = static_cast<float>(clock_get_hz(clk_sys)) / (audio_config.pwm_frequency * wrap_value);
            float clock_speed = static_cast<float>(clock_get_hz(clk_sys));
            LOGI("->wrap_value = %d", wrap_value);
            LOGI("->max clock speed = %f", clock_speed);
            LOGI("->divider = %f", pwmClockDivider);
            LOGI("->clock speed = %f", clock_speed/pwmClockDivider);
            pwm_config_set_clkdiv(&pico_pwm_config, pwmClockDivider);
            pwm_config_set_clkdiv_mode(&pico_pwm_config, PWM_DIV_FREE_RUNNING);
            //pwm_config_set_phase_correct(&pico_pwm_config, false);
            pwm_config_set_wrap (&pico_pwm_config, wrap_value);
            return pico_pwm_config;
        } 

        // set up pwm 
        void setupPWMPin(pwm_config &cfg, PicoChannelOut &pinInfo)  {
             LOGD("%s for gpio %d",LOG_METHOD, pinInfo.gpio);
            // setup pwm pin  
            int gpio = pinInfo.gpio;
            gpio_set_function(gpio, GPIO_FUNC_PWM);
            pinInfo.slice = pwm_gpio_to_slice_num(gpio);
            pinInfo.channel = pwm_gpio_to_channel(gpio);
            pwm_init(pinInfo.slice, &cfg, true);

            // set initial output value 
            pwm_set_chan_level(pinInfo.slice, pinInfo.channel, 0); 
        }

        void setupTimer() override {
        }

        /// The pico supports max 16 pwm pins
        virtual int maxChannels() override{
            return 16;
        };

        /// Max pwm output value
        virtual int maxOutputValue()override{
            return std::pow(audio_config.resolution,2)-1;
        }
        
        /// write a pwm value to the indicated channel. The values are between 0 and 255
        void pwmWrite(int audioChannel, int value)override{
            pwm_set_chan_level(pins[audioChannel].slice, pins[audioChannel].channel, value);
        }

        // timed output executed at the sampleRate
        static void defaultPWMAudioOutputCallbackPico(void* ptr) {
            PWMDriverRP2040 *self = (PWMDriverRP2040*)  ptr;
            self->playNextFrame();
        }

};



} // Namespace


#endif

