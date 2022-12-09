#pragma once
/**
 * @defgroup timer Timers
 * @ingroup tools
 * @brief Platform specific timers
 */
#include "AudioConfig.h"
#ifdef USE_TIMER
#include "AudioTools/AudioLogger.h"
#include "AudioTimer/AudioTimerBase.h"
#include "AudioTimer/AudioTimerESP32.h"
#include "AudioTimer/AudioTimerESP8266.h"
#include "AudioTimer/AudioTimerRP2040.h"
#include "AudioTimer/AudioTimerMBED.h"
#include "AudioTimer/AudioTimerAVR.h"
#include "AudioTimer/AudioTimerSTM32.h"

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
         * @param driver 
         */
        TimerAlarmRepeating(TimerAlarmRepeatingDriverBase &driver) {
            p_driver = &driver;
        };
        virtual ~TimerAlarmRepeating() = default;

        bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) {
            return p_driver->begin(callback_f, time, unit);
        }
        bool end() { 
            return p_driver->end(); 
        };

        void setCallbackParameter(void* obj){
          p_driver->setCallbackParameter(obj);
        }

        void *callbackParameter(){
            return p_driver->callbackParameter();
        }

        virtual void setTimer(int timer){
            p_driver->setTimer(timer);
        }

        virtual void setTimerFunction(TimerFunction function=DirectTimerCallback){
            p_driver->setTimerFunction(function);
        }

    protected:
        void* object=nullptr;
        TimerAlarmRepeatingDriver driver; // platform specific driver
        TimerAlarmRepeatingDriverBase *p_driver=&driver;

};

} // namespace

#endif

