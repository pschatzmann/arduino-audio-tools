#pragma once

#if defined(ESP32) && defined(ARDUINO)
#include <esp_task_wdt.h>

#include "AudioTools/CoreAudio/AudioTimer/AudioTimerBase.h"

namespace audio_tools {

/**
 * @brief Internal class to manage User callbacks. An optinal parameter can be
 * passed to the callback method We support 4 timers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class UserCallback {
 public:
  void setup(repeating_timer_callback_t my_callback, void *user_data,
             bool lock) {
    TRACED();
    this->my_callback = my_callback;
    this->user_data = user_data;
    this->lock = lock;  // false when called from Task
  }

  IRAM_ATTR void call() {
    if (my_callback != nullptr) {
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

} static *simpleUserCallback = nullptr;

static IRAM_ATTR void userCallback0() { simpleUserCallback[0].call(); }
static IRAM_ATTR void userCallback1() { simpleUserCallback[1].call(); }
static IRAM_ATTR void userCallback2() { simpleUserCallback[2].call(); }
static IRAM_ATTR void userCallback3() { simpleUserCallback[3].call(); }

/**
 * @brief Internal class to manage the different timer callbacks for the 4
 * hardware timers
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

  void setup(TaskHandle_t handler_task) {
    TRACED();
    p_handler_task = handler_task;
    assert(handler_task != nullptr);
  }

  IRAM_ATTR void call() {
    if (p_handler_task != nullptr) {
      // A mutex protects the handler from reentry (which shouldn't happen, but
      // just in case)
      portENTER_CRITICAL_ISR(&timerMux);
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      vTaskNotifyGiveFromISR(p_handler_task, &xHigherPriorityTaskWoken);
      if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
      }
      portEXIT_CRITICAL_ISR(&timerMux);
    }
  }

 protected:
  portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
  TaskHandle_t p_handler_task = nullptr;

} static *timerCallbackArray = nullptr;

static IRAM_ATTR void timerCallback0() { timerCallbackArray[0].call(); }
static IRAM_ATTR void timerCallback1() { timerCallbackArray[1].call(); }
static IRAM_ATTR void timerCallback2() { timerCallbackArray[2].call(); }
static IRAM_ATTR void timerCallback3() { timerCallbackArray[3].call(); }

/**
 * @brief Repeating Timer functions for simple scheduling of repeated execution.
 * The basic logic is taken from
 * https://www.toptal.com/embedded/esp32-audio-sampling. Plaease use the typedef
 * TimerAlarmRepeating.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class TimerAlarmRepeatingDriverESP32 : public TimerAlarmRepeatingDriverBase {
 public:
  TimerAlarmRepeatingDriverESP32() {
    setTimerFunction(DirectTimerCallback);
    setTimer(0);
  }

  TimerAlarmRepeatingDriverESP32(int timer, TimerFunction function) {
    setTimerFunction(function);
    setTimer(timer);
  }

  void setTimer(int id) override {
    if (id >= 0 && id < 4) {
      this->timer_id = id;
      handler_task = nullptr;
    } else {
      LOGE("Invalid timer id %d", timer_id);
    }
  }

  void setTimerFunction(TimerFunction function = DirectTimerCallback) override {
    this->function = function;
  }

  /// Starts the alarm timer
  bool begin(repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    TRACED();

    // we determine the time in microseconds
    switch (unit) {
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
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
    LOGI("Timer every: %u us", timeUs);
    adc_timer = timerBegin(timer_id, 80,
                           true);  // divider=80 -> 1000000 calls per second
#else
    uint32_t freq = AudioTime::AudioTime::toRateUs(timeUs);
    LOGI("Timer freq: %u hz", (unsigned)freq);
    adc_timer = timerBegin(1000000);  // divider=80 -> 1000000 calls per second
#endif
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
    if (started) {
      timerDetachInterrupt(adc_timer);
      timerEnd(adc_timer);
      if (handler_task != nullptr) {
        vTaskDelete(handler_task);
        handler_task = nullptr;
      }
    }
    started = false;
    return true;
  }

  void setCore(int core) { this->core = core; }

  /// Selects the TimerFunction: default is DirectTimerCallback, when set to
  /// save we use TimerCallbackInThread
  void setIsSave(bool is_save) override {
    setTimerFunction(is_save ? TimerCallbackInThread : DirectTimerCallback);
  }

 protected:
  int timer_id = 0;
  volatile bool started = false;
  TaskHandle_t handler_task = nullptr;
  hw_timer_t *adc_timer = nullptr;  // our timer
  UserCallback user_callback;       // for task
  TimerFunction function;
  int core = 1;
  int priority = 1;  // configMAX_PRIORITIES - 1;
  uint32_t timeUs;

  /// call timerAttachInterrupt
  void attach(hw_timer_t *timer, void (*cb)()) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    timerAttachInterrupt(timer, cb);
#else
    timerAttachInterrupt(timer, cb, false);
#endif
  }

  /// direct timer callback
  void setupDirectTimerCallback(repeating_timer_callback_t callback_f) {
    TRACED();
    // We start the timer which executes the callbacks directly
    if (simpleUserCallback == nullptr) {
      simpleUserCallback = new UserCallback[4];
    }
    simpleUserCallback[timer_id].setup(callback_f, object, true);

    if (timer_id == 0)
      attach(adc_timer, userCallback0);
    else if (timer_id == 1)
      attach(adc_timer, userCallback1);
    else if (timer_id == 2)
      attach(adc_timer, userCallback2);
    else if (timer_id == 3)
      attach(adc_timer, userCallback3);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
    timerAlarmWrite(adc_timer, timeUs, true);
    timerAlarmEnable(adc_timer);

#else
    // timerStart(adc_timer);
    // timerWrite(adc_timer,timeUs);
    timerAlarm(adc_timer, timeUs, true, 0);
#endif
  }

  /// timer callback is notifiying task: supports functionality which can not be
  /// called in an interrupt e.g. i2c
  void setupTimerCallbackInThread(repeating_timer_callback_t callback_f) {
    TRACED();
    // we start the timer which runs the callback in a seprate task
    if (timerCallbackArray == nullptr) {
      timerCallbackArray = new TimerCallback[4];
    }

    if (timer_id == 0)
      attach(adc_timer, timerCallback0);
    else if (timer_id == 1)
      attach(adc_timer, timerCallback1);
    else if (timer_id == 2)
      attach(adc_timer, timerCallback2);
    else if (timer_id == 3)
      attach(adc_timer, timerCallback3);

    // we record the callback method and user data
    user_callback.setup(callback_f, object, false);
    // setup the timercallback
    xTaskCreatePinnedToCore(complexTaskHandler, "TimerAlarmRepeatingTask",
                            configMINIMAL_STACK_SIZE + 10000, &user_callback,
                            priority, &handler_task, core);
    LOGI("Task created on core %d", core);
    timerCallbackArray[timer_id].setup(handler_task);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
    timerAlarmWrite(adc_timer, timeUs, true);
    timerAlarmEnable(adc_timer);
#else
    timerAlarm(adc_timer, timeUs, true, 0);  // set time in us
#endif
  }

  /// No timer - just a simple task loop
  void setupSimpleThreadLoop(repeating_timer_callback_t callback_f) {
    TRACED();
    user_callback.setup(callback_f, object, false);
    xTaskCreatePinnedToCore(simpleTaskLoop, "TimerAlarmRepeatingTask",
                            configMINIMAL_STACK_SIZE + 10000, this, priority,
                            &handler_task, core);
    LOGI("Task created on core %d", core);
  }

  /// We can not do any I2C calls in the interrupt handler so we need to do this
  /// in a separate task
  static void complexTaskHandler(void *param) {
    TRACEI();
    UserCallback *cb = (UserCallback *)param;

    while (true) {
      // Sleep until the ISR gives us something to do
      uint32_t thread_notification =
          ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
      // Do something complex and CPU-intensive
      if (thread_notification) {
        cb->call();
      }
      delay(0);
    }
  }

  /// We can not do any I2C calls in the interrupt handler so we need to do this
  /// in a separate task.
  static void simpleTaskLoop(void *param) {
    TRACEI();
    TimerAlarmRepeatingDriverESP32 *ta =
        (TimerAlarmRepeatingDriverESP32 *)param;

    while (true) {
      unsigned long end = micros() + ta->timeUs;
      ta->user_callback.call();
      long waitUs = end - micros();
      long waitMs = waitUs / 1000;
      waitUs = waitUs - (waitMs * 1000);
      delay(waitMs);
      waitUs = end - micros();
      if (waitUs > 0) {
        delayMicroseconds(waitUs);
      }
    }
  }
};

/// @brief  use TimerAlarmRepeating!  @ingroup timer_esp32
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverESP32;

}  // namespace audio_tools

#endif
