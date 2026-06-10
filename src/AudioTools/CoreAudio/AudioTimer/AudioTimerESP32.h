#pragma once

#if defined(ESP32) 
#include <esp_task_wdt.h>
#include "esp_timer.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerBase.h"

namespace audio_tools {

/**
 * @brief Repeating Timer functions for simple scheduling of repeated execution
 * using esp_timer_create()
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class TimerAlarmRepeatingDriverESP32
    : public TimerAlarmRepeatingDriverBase {
 public:
  virtual ~TimerAlarmRepeatingDriverESP32() {
    end();
    if (rtsp_timer != nullptr) {
      LOGD("esp_timer_delete");
      esp_timer_delete(rtsp_timer);
      rtsp_timer = nullptr;
    }
  }

  void setIsSave(bool is_save) override {
    setTimerFunction(is_save ? TimerCallbackInThread : DirectTimerCallback);
  }

  void setTimerFunction(TimerFunction function) override {
    timer_function = function;
  }

  /// Starts the alarm timer
  bool begin(repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    TRACED();

    // we determine the time in microseconds
    switch (unit) {
      case MS:
        time_us = time * 1000;
        break;
      case US:
        time_us = time;
        break;
      case HZ:
        // convert hz to time in us
        time_us = AudioTime::toTimeUs(time);
        break;
    }
    LOGI("Timer every: %u us", time_us);

    if (rtsp_timer == nullptr) {
      esp_timer_create_args_t timer_args;
      timer_args.callback = callback_f;
      timer_args.name = "rtsp_timer";
      timer_args.arg = callbackParameter();
      // select the dispatch method
      if (timer_function == DirectTimerCallback) {
#if CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
        timer_args.dispatch_method = ESP_TIMER_ISR;
#else
        LOGW("ESP_TIMER_ISR dispatch method not supported, using ESP_TIMER_TASK");
        timer_args.dispatch_method = ESP_TIMER_TASK;    
#endif
      } else {
        timer_args.dispatch_method = ESP_TIMER_TASK;
      }

      LOGD("esp_timer_create: %p", timer_args.arg);
      esp_err_t rc = esp_timer_create(&timer_args, &rtsp_timer);
      if (rc != ESP_OK) {
        LOGE("Could not create timer: %d", rc);
        return false;
      }
    }

    esp_err_t rc = ESP_OK;
    if (esp_timer_is_active(rtsp_timer)) {
      rc = esp_timer_stop(rtsp_timer);
    }

    LOGD("esp_timer_start_periodic");
    rc = esp_timer_start_periodic(rtsp_timer, time_us);

    if (rc != ESP_OK) {
      LOGE("Could not start timer: %d", rc);
      return false;
    }

    return true;
  }

  /// ends the timer and if necessary the task
  bool end() override {
    TRACED();
    if (esp_timer_is_active(rtsp_timer)) {
      LOGD("esp_timer_stop");
      esp_timer_stop(rtsp_timer);
    }
    return true;
  }

 protected:
  esp_timer_handle_t rtsp_timer = nullptr;
  uint64_t time_us = 0;
  TimerFunction timer_function = TimerCallbackInThread;
};

/// @brief  use TimerAlarmRepeating!  @ingroup timer_esp32
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverESP32;


}  // namespace audio_tools

#endif

