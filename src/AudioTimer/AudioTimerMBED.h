#pragma once

#if defined(ARDUINO_ARCH_MBED)
#include "AudioTimer/AudioTimerBase.h"
#include "mbed.h"

namespace audio_tools {

class TimerAlarmRepeatingDriverMBED;
static TimerAlarmRepeatingDriverMBED *timerAlarmRepeating = nullptr;

/**
 * @brief Repeating Timer functions for repeated execution: Plaease use the
 * typedef TimerAlarmRepeating
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class TimerAlarmRepeatingDriverMBED : public TimerAlarmRepeatingDriverBase {
 public:
  TimerAlarmRepeatingDriverMBED() { timerAlarmRepeating = this; }

  /**
   * Starts the alarm timer
   */
  bool begin(repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    callback = callback_f;

    // we determine the time in microseconds
    switch (unit) {
      case MS:
        ticker.attach(tickerCallback, std::chrono::microseconds(time * 1000));
        break;
      case US:
        ticker.attach(tickerCallback, std::chrono::microseconds(time));
        break;
      case HZ:
        // convert hz to time in us
        uint64_t time_us = AudioTime::toTimeUs(time);
        ticker.attach(tickerCallback, std::chrono::microseconds(time_us));
        break;
    }
    return true;
  }

  // ends the timer and if necessary the task
  bool end() {
    ticker.detach();
    return true;
  }

 protected:
  mbed::Ticker ticker;
  repeating_timer_callback_t callback;

  inline static void tickerCallback() {
    timerAlarmRepeating->callback(timerAlarmRepeating->object);
  }
};

/// @brief  use TimerAlarmRepeating!  @ingroup timer_mbed
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverMBED;

}  // namespace audio_tools

#endif