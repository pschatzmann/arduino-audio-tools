#pragma once

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
#include <time.h>

#include <functional>

#include "AudioTimer/AudioTimerBase.h"
#include "hardware/timer.h"
#include "pico/time.h"

namespace audio_tools {

typedef void (*my_repeating_timer_callback_t)(void *obj);

/**
 * @brief Repeating Timer functions for repeated execution: Plaease use the
 * typedef TimerAlarmRepeating
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class TimerAlarmRepeatingDriverRP2040 : public TimerAlarmRepeatingDriverBase {
 public:
  TimerAlarmRepeatingDriverRP2040() {
    alarm_pool_init_default();
    ap = alarm_pool_get_default();
  }

  /**
   * Starts the alarm timer
   */
  bool begin(const my_repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    bool result = false;
    LOGI("timer time: %u %s", (unsigned int)time, toString(unit));
    this->instanceCallback = callback_f;

    // we determine the time in microseconds
    switch (unit) {
      case MS:
        result = alarm_pool_add_repeating_timer_ms(ap, time, &staticCallback,
                                                   this, &timer);
        break;
      case US:
        result = alarm_pool_add_repeating_timer_us(ap, time, &staticCallback,
                                                  this, &timer);
        break;
      case HZ:
        // convert hz to time in us
        uint64_t time_us = AudioTime::toTimeUs(time);
        result = alarm_pool_add_repeating_timer_us(ap, time_us, &staticCallback,
                                                   this, &timer);
        break;
    }

    return result;
  }

  inline static bool staticCallback(repeating_timer *ptr) {
    TimerAlarmRepeatingDriverRP2040 *self =
        (TimerAlarmRepeatingDriverRP2040 *)ptr->user_data;
    self->instanceCallback(self->object);
    return true;
  }

  // ends the timer and if necessary the task
  bool end() { return cancel_repeating_timer(&timer); }

 protected:
  alarm_pool_t *ap = nullptr;
  repeating_timer_t timer;
  my_repeating_timer_callback_t instanceCallback = nullptr;
};

/// @brief use TimerAlarmRepeating! @ingroup timer_rp2040
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverRP2040;

}  // namespace audio_tools

#endif