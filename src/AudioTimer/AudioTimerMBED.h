#pragma once

#if defined(__arm__)  && __has_include("mbed.h") 
#include "AudioTimer/AudioTimerBase.h"
#include "mbed.h"

namespace audio_tools {

class TimerAlarmRepeatingDriverMBED;
static TimerAlarmRepeatingDriverMBED *timerAlarmRepeating = nullptr;

/**
 * @brief Repeating Timer functions for repeated execution: Plaease use the typedef TimerAlarmRepeating
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class TimerAlarmRepeatingDriverMBED : public TimerAlarmRepeatingDriverBase {
    public:

        TimerAlarmRepeatingDriverMBED() {
            timerAlarmRepeating = this;
        }

        /**
         * Starts the alarm timer
         */
        bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) override {
            callback = callback_f;

            // we determine the time in microseconds
            switch(unit){
                case MS:
                    ticker.attach_us(tickerCallback, (us_timestamp_t) time * 1000);
                    break;
                case US:
                    ticker.attach_us(tickerCallback,(us_timestamp_t) time);
                    break;
            }
            return true;
        }

        // ends the timer and if necessary the task
        bool end(){
            ticker.detach();
            return true;
        }

    protected:
        mbed::Ticker ticker;
        repeating_timer_callback_t callback;

        inline static void tickerCallback(){
            timerAlarmRepeating->callback(timerAlarmRepeating->object);
        }

};

/// @brief  use TimerAlarmRepeating!  @ingroup timer_mbed
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverMBED;


}



#endif