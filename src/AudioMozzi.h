#pragma once

#include "mozzi_config.h"
#include "hardware_defines.h"
#include "mozzi_analog.h"
#include "MozziGuts.h"

unsigned long total_tick_count = 0;

unsigned long audioTicks() {
    return total_tick_count;
}

namespace audio_tools {

/**
 * @brief Support for  https://sensorium.github.io/Mozzi/ 
 * Define your updateControl() method.
 * Define your updateAudio() method.
 * Start by calling begin(control_rate)
 * do not call audioHook(); in the loop !

 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MozziGenerator : public SoundGenerator<int16_t> {
    public:
        MozziGenerator(int control_rate){
            info.sample_rate = AUDIO_RATE; 
            info.bits_per_sample = 16;
            info.channels = 2;
            control_counter_max = info.sample_rate / control_rate;
            if (control_counter_max <= 0){
                control_counter_max = 1;
            }
            control_counter = control_counter_max;
        }

        /// Provides some key audio information
        AudioBaseInfo config() {
            return info;
        }

        /// Provides a single sample
        virtual int16_t readSample() {
            if (--control_counter<0){
                control_counter = control_counter_max;
                ::updateControl();
            }
            int result = (int) ::updateAudio();
            total_tick_count++;
            return result;
        }

    protected:
        AudioBaseInfo info;
        int control_counter_max;
        int control_counter;

};


} // Namespace
