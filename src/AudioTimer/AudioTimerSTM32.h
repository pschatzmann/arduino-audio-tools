#pragma once

#if defined(STM32)
#include "AudioTimer/AudioTimerDef.h"

namespace audio_tools {

class TimerAlarmRepeatingSTM32;
INLINE_VAR TimerAlarmRepeatingSTM32 *timerAlarmRepeating = nullptr;
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
            timer_index = timerIdx;
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
            TRACEI();
            LOGI("Using timer TIM%d", timer_index+1);
            timer->attachInterrupt(std::bind(callback_f, object)); 
          
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
            TRACEI();
            timer->pause();
            return true;
        }

    protected:
        HardwareTimer *timer; 
        int timer_index;
        TIM_TypeDef *timers[6] = {TIM1, TIM2, TIM3, TIM4, TIM5 };

};

typedef  TimerAlarmRepeatingSTM32 TimerAlarmRepeating;


}



#endif