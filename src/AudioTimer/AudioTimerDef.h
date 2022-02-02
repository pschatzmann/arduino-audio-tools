#pragma once
#include "AudioTools/AudioTypes.h"

namespace audio_tools {

typedef void (*repeating_timer_callback_t )(void* obj);

enum TimerFunction {DirectTimerCallback, TimerCallbackInThread, SimpleThreadLoop };

/**
 * @brief Common Interface definition for TimerAlarmRepeating 
 * 
 */
class TimerAlarmRepeatingDef {
    public:
        virtual  ~TimerAlarmRepeatingDef() = default;
        virtual bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) = 0;
        virtual bool end() = 0;

        void setCallbackParameter(void* obj){
          object = obj;
        }

        void *callbackParameter(){
            return object;
        }

    protected:
        void* object;

};

/**
 * @brief Tools for calculating timer values
 * 
 */
class AudioUtils {
    public:
        /// converts sampling rate to delay in microseconds (μs)
        static uint32_t toTimeUs(uint32_t samplingRate, uint8_t limit=10){
            uint32_t result = 1000000l / samplingRate;
            if (result <= limit){
                LOGW("Time for samplingRate %u -> %lu is < %u μs - we rounded up", (unsigned int)samplingRate, result, limit);
                result = limit;
            }
            return result;
        }

        static uint32_t toTimeMs(uint32_t samplingRate, uint8_t limit=1){
            uint32_t result = 1000l / samplingRate;
            if (result <= limit){
                LOGW("Time for samplingRate %u -> %lu is < %u μs - we rounded up", (unsigned int)samplingRate, result, limit);
                result = limit;
            }
            return result;
        }
};


}

