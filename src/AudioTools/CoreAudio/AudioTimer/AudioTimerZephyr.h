#pragma once

#include "AudioTimerBase.h"

#if defined(IS_ZEPHYR) || defined(DOXYGEN)

#include <zephyr/kernel.h>

namespace audio_tools {

/**
 * @brief Repeating Timer driver for Zephyr using k_timer
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 * Implements `TimerAlarmRepeatingDriverBase` using Zephyr's kernel timer API
 * (k_timer). Supports both callback-based and thread-based execution modes.
 *
 * Characteristics:
 * - Supports US / MS / HZ time specifications
 * - Uses k_timer_init() and k_timer_start() for efficient scheduling
 * - Defers callback execution to system workqueue when TimerCallbackInThread mode is set
 * - Precision limited by Zephyr's kernel tick resolution
 *
 */
class TimerAlarmRepeatingDriverZephyr : public TimerAlarmRepeatingDriverBase {
 public:
  TimerAlarmRepeatingDriverZephyr() = default;

  ~TimerAlarmRepeatingDriverZephyr() { end(); }

  /// Starts the repeating timer
  bool begin(repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    TRACED();

    if (callback_f == nullptr || time == 0) {
      LOGE("Invalid callback or time");
      return false;
    }

    // Stop any existing timer
    if (is_running) {
      end();
    }

    // Convert time to microseconds for Zephyr
    switch (unit) {
      case MS:
        period_us = static_cast<uint64_t>(time) * 1000ULL;
        break;
      case US:
        period_us = time;
        break;
      case HZ:
        // Convert Hz to microseconds: period = 1'000'000 / frequency
        period_us = (time > 0) ? (1000000ULL / static_cast<uint64_t>(time))
                               : 1000ULL;
        break;
    }

    if (period_us == 0) {
      period_us = 1;
    }

    LOGI("Timer every: %llu us", period_us);

    callback = callback_f;

    // Create the timer if needed
    if (!timer_initialized) {
      // Initialize timer and work item
      k_timer_init(&timer, timerExpiredCallback, nullptr);
      k_timer_user_data_set(&timer, this);
      k_work_init(&work, workCallback);
      timer_initialized = true;
    }

    // Start the timer: first expiration after period, then repeat every period
    k_timer_start(&timer, K_USEC(period_us), K_USEC(period_us));
    is_running = true;

    LOGD("Timer started");
    return true;
  }

  /// Stops the timer
  bool end() override {
    TRACED();
    if (is_running) {
      k_timer_stop(&timer);
      is_running = false;
      LOGD("Timer stopped");
    }
    return true;
  }

  /// Sets the callback execution mode
  void setTimerFunction(TimerFunction function) override {
    timer_function = function;
    LOGD("Timer function mode set to: %d", function);
  }

  /// Sets whether timer callbacks should be safe (execute in thread context)
  void setIsSave(bool is_save) override {
    setTimerFunction(is_save ? TimerCallbackInThread : DirectTimerCallback);
  }

 protected:
  struct k_timer timer = {};
  struct k_work work = {};
  bool timer_initialized = false;
  bool is_running = false;
  repeating_timer_callback_t callback = nullptr;
  uint64_t period_us = 0;
  TimerFunction timer_function = DirectTimerCallback;

  inline void executeCallback() {
    if (callback != nullptr) {
      callback(callbackParameter());
    }
  }

  static void workCallback(struct k_work* work_ptr) {
    auto* self = CONTAINER_OF(work_ptr, TimerAlarmRepeatingDriverZephyr, work);
    if (self != nullptr) {
      self->executeCallback();
    }
  }

  /// Static callback wrapper for k_timer
  /// This is invoked in interrupt context, so we either call directly
  /// or defer to system workqueue.
  static void timerExpiredCallback(struct k_timer* timer_ptr) {
    auto* self = static_cast<TimerAlarmRepeatingDriverZephyr*>(
        k_timer_user_data_get(timer_ptr));
    if (self == nullptr) return;

    if (self->timer_function == TimerCallbackInThread) {
      k_work_submit(&self->work);
    } else {
      self->executeCallback();
    }
  }
};

/// @brief Default timer implementation for Zephyr
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverZephyr;

}  // namespace audio_tools

#endif
