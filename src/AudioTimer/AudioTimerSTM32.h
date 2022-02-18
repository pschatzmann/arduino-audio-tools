#pragma once

#if defined(STM32)
#include "AudioTimer/AudioTimerDef.h"

namespace audio_tools {

class TimerAlarmRepeating;
TimerAlarmRepeating *timerAlarmRepeating = nullptr;
typedef void (* repeating_timer_callback_t )(void* obj);

/**
 * @brief STM32 Repeating Timer functions for repeated execution: Plaease use the typedef TimerAlarmRepeating
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class TimerAlarmRepeatingSTM32 : public TimerAlarmRepeatingDef {
    public:

        TimerAlarmRepeatingSTM32(TimerFunction function=DirectTimerCallback, int timerIdx=1){
            this->timer = new HardwareTimer(timers[timerIdx]);
            timer->pause();
        }

        ~TimerAlarmRepeatingSTM32(){
            end();
            delete this->timer;
        }

        /**
         * Starts the alarm timer
         */
        bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) override {
            timer->attachInterrupt(std::bind(callback_f, this)); 
          
            // we determine the time in microseconds
            switch(unit){
                case MS:
                    timer->setOverflow(time * 1000, MICROSEC_FORMAT); // 10 Hz
                    break;
                case US:
                    timer->setOverflow(time, MICROSEC_FORMAT); // 10 Hz
                    break;
            }
            timer->resume();
            return true;
        }

        // ends the timer and if necessary the task
        bool end() override {
            timer->pause();
            return true;
        }

    protected:
        HardwareTimer *timer; 
        TIM_TypeDef *timers[3] = {TIM1, TIM2, TIM3 };

};

typedef  TimerAlarmRepeatingSTM32 TimerAlarmRepeating;


}



#endif