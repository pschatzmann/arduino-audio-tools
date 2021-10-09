#pragma once

#ifdef ESP32
#include "AudioTimer/AudioTimerDef.h"

namespace audio_tools {

typedef void (* simple_callback )(void);


/**
 * @brief Internal class to manage User callbacks. An optinal parameter can be passed to the callback method
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class UserCallback {
  public:
    void setup(repeating_timer_callback_t my_callback, void *user_data ){
      this->my_callback = my_callback;
      this->user_data = user_data;
    }
    
    IRAM_ATTR void call() {
      if (my_callback!=nullptr){
        portENTER_CRITICAL_ISR(&timerMux);
        my_callback(user_data);
        portEXIT_CRITICAL_ISR(&timerMux);
      }
    }

  protected:
    repeating_timer_callback_t my_callback = nullptr;
    void *user_data = nullptr;
    portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
} *simpleUserCallback = nullptr;


/**
 * @brief Internal class to manage the different timer callbacks for the 4 hardware timers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TimerCallback {
  public:
      TimerCallback() {
          timerMux = portMUX_INITIALIZER_UNLOCKED;
          handler_task = nullptr;
      }

      void setup(TaskHandle_t handler_task){
        this->handler_task = handler_task;
      }
      
      IRAM_ATTR void onTimerCb() {
        if (handler_task!=nullptr) {
          // A mutex protects the handler from reentry (which shouldn't happen, but just in case)
          portENTER_CRITICAL_ISR(&timerMux);
          BaseType_t xHigherPriorityTaskWoken = pdFALSE;
          vTaskNotifyGiveFromISR(handler_task, &xHigherPriorityTaskWoken);
          if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
          }
          portEXIT_CRITICAL_ISR(&timerMux);
        }
      }
      
  protected:
      portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
      TaskHandle_t handler_task;

} *timerCallbackArray = nullptr;


/**
 * @brief Repeating Timer functions for simple scheduling of repeated execution.
 * The basic logic is taken from https://www.toptal.com/embedded/esp32-audio-sampling.
 * Plaease use the typedef TimerAlarmRepeating.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class TimerAlarmRepeatingESP32 : public TimerAlarmRepeatingDef {
    public:
    
        TimerAlarmRepeatingESP32(bool withTask=false, int id=0){
          if (id>=0 && id<4) {
            this->timer_id = id;
            this->with_task = withTask;
            handler_task = nullptr;

            // setup the callback method
            if (withTask){
            } else {

            }
          } else {
            LOGE("Invalid timer id %d", timer_id);
          }
        }

        ~TimerAlarmRepeatingESP32(){
          end();
          if (simpleUserCallback!=nullptr){
            delete [] simpleUserCallback;
          }
          if (timerCallbackArray!=nullptr){
            delete [] timerCallbackArray;
          }
        }


        /// Starts the alarm timer
        bool begin(repeating_timer_callback_t callback_f, uint32_t time, TimeUnit unit = MS) override {
            LOGD(LOG_METHOD);
            uint32_t timeUs;

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
            adc_timer = timerBegin(0, 80, true);  // divider=80 -> 1000000 calls per second

            if (with_task) {
                // we start the timer which runs the callback in a seprate task
                if (timerCallbackArray==nullptr){
                  timerCallbackArray = new TimerCallback[4];
                }
                // setup the timercallback
                timerCallbackArray[timer_id].setup(handler_task);

                if (timer_id==0) current_timer_callback = []() {timerCallbackArray[0].onTimerCb();};
                if (timer_id==1) current_timer_callback = []() {timerCallbackArray[1].onTimerCb();};
                if (timer_id==2) current_timer_callback = []() {timerCallbackArray[2].onTimerCb();};
                if (timer_id==3) current_timer_callback = []() {timerCallbackArray[3].onTimerCb();};

                // we record the callback method and user data
                user_callback.setup(callback_f, object);
                timerAttachInterrupt(adc_timer, current_timer_callback, true);
                timerAlarmWrite(adc_timer, timeUs, true);
                timerAlarmEnable(adc_timer);
                xTaskCreate(complexHandler, "TimerAlarmRepeatingTask", configMINIMAL_STACK_SIZE+10000, &user_callback, 1, &handler_task);

            } else {
                // We start the timer which executes the callbacks directly
                if (simpleUserCallback==nullptr){
                  simpleUserCallback = new UserCallback[4];
                }
                simpleUserCallback[timer_id].setup(callback_f, object);
                if (timer_id==0) current_timer_callback = []() {simpleUserCallback[0].call();};
                if (timer_id==1) current_timer_callback = []() {simpleUserCallback[1].call();};
                if (timer_id==2) current_timer_callback = []() {simpleUserCallback[2].call();};
                if (timer_id==3) current_timer_callback = []() {simpleUserCallback[3].call();};

                timerAttachInterrupt(adc_timer, current_timer_callback, true);
                timerAlarmWrite(adc_timer, timeUs, true);
                timerAlarmEnable(adc_timer);
            }

            started = true;
            return true;
        }

        /// ends the timer and if necessary the task
        bool end() override {
            LOGD(LOG_METHOD);
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

        void testCallback() {
          if (current_timer_callback!=nullptr){
            current_timer_callback();
          }
        }

    protected:
      int timer_id;
      TaskHandle_t handler_task = nullptr;
      hw_timer_t* adc_timer = nullptr; // our timer
      UserCallback user_callback;
      bool started = false;
      bool with_task = false;
      void (*current_timer_callback)() = nullptr;   
      void* object = this;

      /// We can not do any I2C calls in the interrupt handler so we need to do this in a separate task
      static void complexHandler(void *param) {
        UserCallback* cb = (UserCallback*) param;
        uint32_t thread_notification;

        while (true) {
          // Sleep until the ISR gives us something to do
          thread_notification = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
          // Do something complex and CPU-intensive
          //esp_task_wdt_reset();
          if (thread_notification){
              cb->call();
          }
          yield();
        }
      }
};

// for User API
typedef  TimerAlarmRepeatingESP32 TimerAlarmRepeating;


}

#endif
