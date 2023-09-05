#pragma once
/**
 * @defgroup timer Timers
 * @ingroup tools
 * @brief Platform specific timers
 */
#include "AudioConfig.h"
#if defined(USE_TIMER)  
#include "AudioTools/AudioLogger.h"
#include "AudioTimer/AudioTimerBase.h"
#include "AudioTimer/AudioTimerESP32.h"
#include "AudioTimer/AudioTimerESP8266.h"
#include "AudioTimer/AudioTimerRP2040.h"
#include "AudioTimer/AudioTimerMBED.h"
#include "AudioTimer/AudioTimerAVR.h"
#include "AudioTimer/AudioTimerSTM32.h"
#include "AudioTimer/AudioTimerRenesas.h"

namespace audio_tools {

/**
 * @brief Common Interface definition for TimerAlarmRepeating 
 * @ingroup timer
 */
class TimerAlarmRepeating {
    public:
        /// @brief Default constructor
        TimerAlarmRepeating() = default;
        /**
         * @brief Construct a new Timer Alarm Repeating object by passing your object
         * which has been customized with some special platform specific parameters
         * @param timer 
         */
        TimerAlarmRepeating(TimerAlarmRepeatingDriverBase &timer) {
            p_timer = &timer;
        };
        virtual ~TimerAlarmRepeating() = default;

        bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) {
            is_active = p_timer->begin(callback_f, time, unit);
            return is_active;
        }
        bool end() { 
            is_active = false;
            return p_timer->end(); 
        };

        void setCallbackParameter(void* obj){
          p_timer->setCallbackParameter(obj);
        }

        void *callbackParameter(){
            return p_timer->callbackParameter();
        }

        virtual void setTimer(int timer){
            p_timer->setTimer(timer);
        }

        virtual void setTimerFunction(TimerFunction function=DirectTimerCallback){
            p_timer->setTimerFunction(function);
        }

        void setIsSave(bool is_save){
            p_timer->setIsSave(is_save);
        }

        /// @brief Returns true if the timer is active
        operator bool() { return is_active; }

        /// Provides access to the driver
        TimerAlarmRepeatingDriverBase* driver() {
            return p_timer;
        }

    protected:
        void* object=nullptr;
        bool is_active = false;;
        TimerAlarmRepeatingDriver timer; // platform specific timer
        TimerAlarmRepeatingDriverBase *p_timer=&timer;

};

} // namespace

#endif

