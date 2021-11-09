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
      LOGD(LOG_METHOD);
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
          LOGD(LOG_METHOD);
          timerMux = portMUX_INITIALIZER_UNLOCKED;
          handler_task = nullptr;
      }

      void setup(TaskHandle_t handler_task){
        LOGD(LOG_METHOD);
        this->handler_task = handler_task;
      }
      
      IRAM_ATTR void call() {
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
      TaskHandle_t handler_task=nullptr;

} *timerCallbackArray = nullptr;

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
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class TimerAlarmRepeatingESP32 : public TimerAlarmRepeatingDef {
    public:
    
        TimerAlarmRepeatingESP32(bool withTask=false, int id=0){
          LOGI("%s: %s, id=%d",LOG_METHOD, withTask?"withTask":"noTask", id);
          if (id>=0 && id<4) {
            this->timer_id = id;
            this->with_task = withTask;
            handler_task = nullptr;
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
            uint32_t cpu_freq = getCpuFrequencyMhz();  // 80 ? 
            adc_timer = timerBegin(0, cpu_freq, true);  // divider=80 -> 1000000 calls per second

            if (with_task) {
                // we start the timer which runs the callback in a seprate task
                if (timerCallbackArray==nullptr){
                  timerCallbackArray = new TimerCallback[4];
                }

                if (timer_id==0) timerAttachInterrupt(adc_timer, timerCallback0, true); 
                else if (timer_id==1) timerAttachInterrupt(adc_timer, timerCallback1, true); 
                else if (timer_id==2) timerAttachInterrupt(adc_timer, timerCallback2, true); 
                else if (timer_id==3) timerAttachInterrupt(adc_timer, timerCallback3, true); 

                // we record the callback method and user data
                user_callback.setup(callback_f, object);
                timerAlarmWrite(adc_timer, timeUs, true);

                // setup the timercallback
                xTaskCreate(complexHandler, "TimerAlarmRepeatingTask", configMINIMAL_STACK_SIZE+10000, &user_callback, 1, &handler_task);
                timerCallbackArray[timer_id].setup(handler_task);

                timerAlarmEnable(adc_timer);

            } else {
                // We start the timer which executes the callbacks directly
                if (simpleUserCallback==nullptr){
                  simpleUserCallback = new UserCallback[4];
                }
                simpleUserCallback[timer_id].setup(callback_f, object);
                if (timer_id==0) timerAttachInterrupt(adc_timer, userCallback0, true); 
                else if (timer_id==1) timerAttachInterrupt(adc_timer, userCallback1, true); 
                else if (timer_id==2) timerAttachInterrupt(adc_timer, userCallback2, true); 
                else if (timer_id==3) timerAttachInterrupt(adc_timer, userCallback3, true); 

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


    protected:
      int timer_id=0;
      volatile bool started = false;
      TaskHandle_t handler_task = nullptr;
      hw_timer_t* adc_timer = nullptr; // our timer
      UserCallback user_callback;
      bool with_task = false;


      /// We can not do any I2C calls in the interrupt handler so we need to do this in a separate task
      static void complexHandler(void *param) {
        LOGI(LOG_METHOD);
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
