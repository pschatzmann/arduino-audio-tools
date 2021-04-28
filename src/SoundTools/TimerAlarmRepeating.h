#pragma once

#ifdef ESP32

namespace sound_tools {

typedef void (* repeating_timer_callback_t )(void* obj);
typedef void (* simple_callback )(void);


/**
 * @brief Internal class to manage User callbacks. An optinal parameter can be passed to the callback method
 */
class UserCallback {
  public:
    void setup(repeating_timer_callback_t my_callback, void *user_data ){
      this->my_callback = my_callback;
      this->user_data = user_data;
    }
    
    IRAM_ATTR void call() {
      my_callback(user_data);
    }

  protected:
    repeating_timer_callback_t my_callback;
    void *user_data;
};


/**
 * @brief Internal class to manage the different timer callbacks for the 4 hardware timers
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
      portMUX_TYPE timerMux; 
      TaskHandle_t handler_task;

} timerCallbackArray[4];



/**
 * @brief Repeating Timer functions for simple scheduling of repeated execution.
 * The basic logic is taken from https://www.toptal.com/embedded/esp32-audio-sampling.
 * 
 * 
 */
class TimerAlarmRepeating {
    public:
        enum TimeUnit {MS, US};
    
        TimerAlarmRepeating(int id=0){
          if (id<4) {
            this->timer_id = id;
            handler_task = nullptr;

            // setup the callback method
            if (id==0) current_timer_callback = []() {timerCallbackArray[0].onTimerCb();};
            if (id==1) current_timer_callback = []() {timerCallbackArray[1].onTimerCb();};
            if (id==2) current_timer_callback = []() {timerCallbackArray[2].onTimerCb();};
            if (id==3) current_timer_callback = []() {timerCallbackArray[3].onTimerCb();};
            
          }
        }

        ~TimerAlarmRepeating(){
            stop();
        }


        /**
         * We can not do any I2C calls in the interrupt handler so we need to do this in a separate task
         */
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
          }
        }

        /**
         * Starts the alarm timer
         */
        bool start(repeating_timer_callback_t callback_f, uint64_t time, TimeUnit unit = TimerAlarmRepeating::MS, void *user_data=nullptr){
            uint64_t timeUs;
            // we record the callback method and user data
            user_callback.setup(callback_f, user_data);

            // we determine the time in microseconds
            switch(unit){
                case MS:
                    timeUs = time / 1000;
                    break;
                case US:
                    timeUs = time;
                    break;
            }

            // we start the timer which runs the callback in a seprate task
            adc_timer = timerBegin(0, 80, true);  // divider=80 -> 1000000 calls per second
            timerAttachInterrupt(adc_timer, current_timer_callback, true);
            timerAlarmWrite(adc_timer, timeUs, true);
            timerAlarmEnable(adc_timer);
            xTaskCreate(complexHandler, "TimerAlarmRepeatingTask", configMINIMAL_STACK_SIZE+10000, &user_callback, 1, &handler_task);

            // setup the timercallback
            timerCallbackArray[timer_id].setup(handler_task);
            
            return true;
        }

        // stops the timer and if necessary the task
        bool stop(){
            timerEnd(adc_timer);
            if (handler_task!=nullptr){
              vTaskDelete(handler_task);
            }
            return true;
        }

    protected:
      int timer_id;
      TaskHandle_t handler_task;
      hw_timer_t* adc_timer; // our timer
      UserCallback user_callback;
      
      void (*current_timer_callback)();     
};

}

#endif
