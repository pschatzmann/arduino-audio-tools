#pragma once

#if defined(STM32)
#include "AudioTimer/AudioTimerBase.h"

namespace audio_tools {

class TimerAlarmRepeatingDriverSTM32;
static TimerAlarmRepeatingDriverSTM32 *timerAlarmRepeating = nullptr;
typedef void (* repeating_timer_callback_t )(void* obj);

/**
 * @brief STM32 Repeating Timer functions for repeated execution: Plaease use the typedef TimerAlarmRepeating
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TimerAlarmRepeatingDriverSTM32 : public TimerAlarmRepeatingDriverBase {
    public:
        TimerAlarmRepeatingDriverSTM32(){
            setTimer(1);
        }

        ~TimerAlarmRepeatingDriverSTM32(){
            end();
            delete this->timer;
        }
        /// selects the timer: 0 = TIM1, 1 = TIM2,2 = TIM3, 3 = TIM4, 4 = TIM5 
        void setTimer(int timerIdx) override {
            if (this->timer!=nullptr){
                delete this->timer;
            }
            this->timer = new HardwareTimer(timers[timerIdx]);
            timer_index = timerIdx;
            timer->pause();
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
        HardwareTimer *timer=nullptr; 
        int timer_index;
        TIM_TypeDef *timers[6] = {TIM1, TIM2, TIM3, TIM4, TIM5 };

};

///  @brief use TimerAlarmRepeating!  @ingroup timer_stm32
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverSTM32;

}



#endif