#pragma once

#ifdef ESP32
#include "AudioTimer/AudioTimerBase.h"
#include <esp_task_wdt.h>

namespace audio_tools {

typedef void (* simple_callback )(void);


/**
 * @brief Internal class to manage User callbacks. An optinal parameter can be passed to the callback method
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class UserCallback {

  public:
    void setup(repeating_timer_callback_t my_callback, void *user_data, bool lock ){
      TRACED();
      this->my_callback = my_callback;
      this->user_data = user_data;
      this->lock = lock; // false when called from Task
    }
    
    IRAM_ATTR void call() {
      if (my_callback!=nullptr){
        if (lock) portENTER_CRITICAL_ISR(&timerMux);
        my_callback(user_data);
        if (lock) portEXIT_CRITICAL_ISR(&timerMux);
      }
    }

  protected:
    repeating_timer_callback_t my_callback = nullptr;
    void *user_data = nullptr;
    portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
    bool lock;

} INLINE_VAR *simpleUserCallback = nullptr;


static IRAM_ATTR void userCallback0() {
  simpleUserCallback[0].call();
}
static IRAM_ATTR void userCallback1() {
  simpleUserCallback[1].call();
}
static IRAM_ATTR void userCallback2() {
  simpleUserCallback[2].call();
}
static IRAM_ATTR void userCallback3() {
  simpleUserCallback[3].call();
}


/**
 * @brief Internal class to manage the different timer callbacks for the 4 hardware timers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TimerCallback {
  public:
      TimerCallback() {
          TRACED();
          timerMux = portMUX_INITIALIZER_UNLOCKED;
          p_handler_task = nullptr;
      }

      void setup(TaskHandle_t &handler_task){
        TRACED();
        p_handler_task = &handler_task;
      }
      
      IRAM_ATTR void call() {
        if (p_handler_task!=nullptr && *p_handler_task!=nullptr) {
          // A mutex protects the handler from reentry (which shouldn't happen, but just in case)
          portENTER_CRITICAL_ISR(&timerMux);
          BaseType_t xHigherPriorityTaskWoken = pdFALSE;
          vTaskNotifyGiveFromISR(*p_handler_task, &xHigherPriorityTaskWoken);
          if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
          }
          portEXIT_CRITICAL_ISR(&timerMux);
        }
      }
      
  protected:
      portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
      TaskHandle_t *p_handler_task=nullptr;

} INLINE_VAR *timerCallbackArray = nullptr;


static IRAM_ATTR void timerCallback0() {
  timerCallbackArray[0].call();
}
static IRAM_ATTR void timerCallback1() {
  timerCallbackArray[1].call();
}
static IRAM_ATTR void timerCallback2() {
  timerCallbackArray[2].call();
}
static IRAM_ATTR void timerCallback3() {
  timerCallbackArray[3].call();
}



/**
 * @brief Repeating Timer functions for simple scheduling of repeated execution.
 * The basic logic is taken from https://www.toptal.com/embedded/esp32-audio-sampling.
 * Plaease use the typedef TimerAlarmRepeating.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class TimerAlarmRepeatingDriverESP32 : public TimerAlarmRepeatingDriverBase  {
   
    public:
        TimerAlarmRepeatingDriverESP32(){
          setTimerFunction(DirectTimerCallback);
          setTimer(0);
        }
    
        void setTimer(int id) override {
          if (id>=0 && id<4) {
            this->timer_id = id;
            handler_task = nullptr;
          } else {
            LOGE("Invalid timer id %d", timer_id);
          }
        }

        virtual void setTimerFunction(TimerFunction function=DirectTimerCallback) override{
            this->function = function;
        }

        /// Starts the alarm timer
        bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) override {
            TRACED();

            // we determine the time in microseconds
            switch(unit){
                case MS:
                    timeUs = time / 1000;
                    break;
                case US:
                    timeUs = time;
                    break;
            }
            LOGI("Timer every: %u us", timeUs);
            uint32_t cpu_freq = getCpuFrequencyMhz();  // 80 ? 
            adc_timer = timerBegin(0, cpu_freq, true);  // divider=80 -> 1000000 calls per second


            switch (function) {
                case DirectTimerCallback:
                  setupDirectTimerCallback(callback_f);
                  break;

                case TimerCallbackInThread:
                  setupTimerCallbackInThread(callback_f);
                  break;

                case SimpleThreadLoop:
                  setupSimpleThreadLoop(callback_f);
                  break;

            }

            started = true;
            return true;
        }


        /// ends the timer and if necessary the task
        bool end() override {
            TRACED();
            if (started){
              timerEnd(adc_timer);
              if (handler_task!=nullptr){
                vTaskDelete(handler_task);
                handler_task = nullptr;
              }
            }
            started = false;
            return true;
        }

        void setCore(int core){
          this->core = core;
        }

    protected:
      int timer_id=0;
      volatile bool started = false;
      TaskHandle_t handler_task = nullptr;
      hw_timer_t* adc_timer = nullptr; // our timer
      UserCallback user_callback;
      TimerFunction function;
      int core = 1;
      int priority = configMAX_PRIORITIES -1;
      uint32_t timeUs;


      /// direct timer callback 
      void setupDirectTimerCallback(repeating_timer_callback_t callback_f){
        TRACED();
        // we start the timer which runs the callback in a seprate task
        if (timerCallbackArray==nullptr){
          timerCallbackArray = new TimerCallback[4];
        }

        if (timer_id==0) timerAttachInterrupt(adc_timer, timerCallback0, true); 
        else if (timer_id==1) timerAttachInterrupt(adc_timer, timerCallback1, true); 
        else if (timer_id==2) timerAttachInterrupt(adc_timer, timerCallback2, true); 
        else if (timer_id==3) timerAttachInterrupt(adc_timer, timerCallback3, true); 

        // we record the callback method and user data
        user_callback.setup(callback_f, object, false);
        timerCallbackArray[timer_id].setup(handler_task);
        timerAlarmWrite(adc_timer, timeUs, true);

        // setup the timercallback
        xTaskCreatePinnedToCore(complexTaskHandler, "TimerAlarmRepeatingTask", configMINIMAL_STACK_SIZE+10000, &user_callback, priority, &handler_task, core);
        LOGI("Task created on core %d", core);

        timerAlarmEnable(adc_timer);
      }

      // timer callback is notifiying task
      void setupTimerCallbackInThread(repeating_timer_callback_t callback_f){
        TRACED();
        // We start the timer which executes the callbacks directly
        if (simpleUserCallback==nullptr){
          simpleUserCallback = new UserCallback[4];
        }
        simpleUserCallback[timer_id].setup(callback_f, object, true);
        if (timer_id==0) timerAttachInterrupt(adc_timer, userCallback0, true); 
        else if (timer_id==1) timerAttachInterrupt(adc_timer, userCallback1, true); 
        else if (timer_id==2) timerAttachInterrupt(adc_timer, userCallback2, true); 
        else if (timer_id==3) timerAttachInterrupt(adc_timer, userCallback3, true); 

        timerAlarmWrite(adc_timer, timeUs, true);
        timerAlarmEnable(adc_timer);

      }

      /// No timer - just a simple task loop
      void setupSimpleThreadLoop(repeating_timer_callback_t callback_f){
        TRACED();
        user_callback.setup(callback_f, object, false);
        xTaskCreatePinnedToCore(simpleTaskLoop, "TimerAlarmRepeatingTask", configMINIMAL_STACK_SIZE+10000, this, priority, &handler_task, core);
        LOGI("Task created on core %d", core);
      }


      /// We can not do any I2C calls in the interrupt handler so we need to do this in a separate task
      static void complexTaskHandler(void *param) {
        TRACEI();
        UserCallback* cb = (UserCallback*) param;
        uint32_t thread_notification;

        while (true) {
          // Sleep until the ISR gives us something to do
          thread_notification = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
          // Do something complex and CPU-intensive
          if (thread_notification){
              cb->call();
          }
        }
      }

      /// We can not do any I2C calls in the interrupt handler so we need to do this in a separate task. 
      static void simpleTaskLoop(void *param) {
        TRACEI();
        TimerAlarmRepeatingDriverESP32* ta = (TimerAlarmRepeatingDriverESP32*) param;

        while (true) {
            unsigned long end = micros() + ta->timeUs;
            ta->user_callback.call();
            long waitUs = end - micros();
            if (waitUs>0){
              delayMicroseconds(waitUs);
            }
        }
      }      
};

/// @brief  use TimerAlarmRepeating!  @ingroup timer_esp32
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverESP32;


}

#endif
