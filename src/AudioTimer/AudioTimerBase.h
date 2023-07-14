#pragma once
#include "AudioConfig.h"
#ifdef USE_TIMER
#include "AudioTools/AudioTypes.h"

/**
 * @defgroup timer Timer 
 * @ingroup tools
 * @brief Timer
 */

namespace audio_tools {

typedef void (*repeating_timer_callback_t )(void* obj);

enum TimerFunction {DirectTimerCallback, TimerCallbackInThread, SimpleThreadLoop };


class TimerAlarmRepeatingDriverBase {
    public:   
        virtual ~TimerAlarmRepeatingDriverBase(){
            end();
        }
     
        virtual bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) = 0;
        virtual bool end() { return false;};

        void setCallbackParameter(void* obj){
          object = obj;
        }

        void *callbackParameter(){
            return object;
        }

        virtual void setTimer(int timer){}
        virtual void setTimerFunction(TimerFunction function=DirectTimerCallback){}
        /// @brief Not used
        virtual void setIsSave(bool is_save){}

    protected:
        void* object=nullptr;
};

} // namespace

#endif

