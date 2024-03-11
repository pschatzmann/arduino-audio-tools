#pragma once

#ifdef __AVR__
#include "AudioTimer/AudioTimerBase.h"

namespace audio_tools {
typedef void (* repeating_timer_callback_t )(void* obj);
class TimerAlarmRepeatingDriverAVR;
static TimerAlarmRepeatingDriverAVR *timerAlarmRepeatingRef = nullptr;


/**
 * @brief Repeating Timer functions for repeated execution: Plaease use the typedef TimerAlarmRepeating
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class TimerAlarmRepeatingDriverAVR : public TimerAlarmRepeatingDriverBase {
    public:

        TimerAlarmRepeatingDriverAVR() {
            timerAlarmRepeatingRef = this;
        }

        /**
         * Starts the alarm timer
         */
        bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) override {
            callback = callback_f;

            // we determine the time in microseconds
            uint32_t timeUs = 0;
            switch(unit){
                case MS:
                    timeUs = time * 1000;
                    break;
                case US:
                    timeUs = time;
                    break;
                case HZ:
                    // convert hz to time in us
                    timeUs = AudioTime::toTimeUs(time);
                    break;
            }
            // frequency = beats / second
            setupTimer(1000000 / time); 
            return true;
        }

        // ends the timer and if necessary the task
        bool end() override {
             TRACED();
            noInterrupts(); 
            // end timer callback
            TCCR1B = 0;
            interrupts(); // enable all interrupts
            return true;
        }

        static void tickerCallback(){
            if (timerAlarmRepeatingRef != nullptr && timerAlarmRepeatingRef->callback!=nullptr)
                timerAlarmRepeatingRef->callback(timerAlarmRepeatingRef);
        }

    protected:
        repeating_timer_callback_t callback = nullptr;

        void setupTimer(uint64_t sample_rate) {
             TRACED();
            // CPU Frequency 16 MHz
            // prescaler 1, 256 or 1024 => no prescaling
            uint32_t steps = F_CPU / 8 / sample_rate;  // e.g. (16000000/8/44100=>45)
            if (steps>65535){
                LOGE("requested sample rate not supported: %d - we use %d",sample_rate, F_CPU / 65536);
                steps = 65535;
            } else {
                LOGD("compare match register set to %d",steps);
            }

            // setup timer intterupt
            noInterrupts(); 
            TCCR1B = 0;
            //compare match register 
            OCR1A = steps;
            TCCR1B |= (1 << WGM12); // CTC mode
            //TCCR1B |= (1 << CS10); // prescaler 1
            TCCR1B |= (1 << CS11); // prescaler 8
            TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
            interrupts(); // enable all interrupts
        } 

};

using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverAVR;

} // namespace

#endif