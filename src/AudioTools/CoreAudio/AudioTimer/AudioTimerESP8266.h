#pragma once

#ifdef ESP8266
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerBase.h"
#include "Ticker.h"

namespace audio_tools {

typedef void (*repeating_timer_callback_t)(void* obj);

class TimerAlarmRepeatingDriverESP8266;

/**
 * @brief Repeating Timer functions for repeated execution: Plaease use the
 * typedef TimerAlarmRepeating
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class TimerAlarmRepeatingDriverESP8266 : public TimerAlarmRepeatingDriverBase {
 public:
  /**
   * We can not do any I2C calls in the interrupt handler so we need to do this
   * in a separate task
   */
  static void complexHandler(void* param) {}

  /**
   * Starts the alarm timer
   */
  bool begin(repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    uint32_t timeUs;

    // we determine the time in microseconds
    switch (unit) {
      case MS:
        timeUs = time / 1000;
        break;
      case US:
        timeUs = time;
        break;
      case HZ:
        // convert hz to time in us
        timeUs = AudioTime::toTimeUs(time);
        break;
    }

    ticker.attach_ms(timeUs / 1000, callback_f, (void*)this);

    return true;
  }

  // ends the timer and if necessary the task
  bool end() override {
    ticker.detach();
    return true;
  }

 protected:
  void (*current_timer_callback)();
  Ticker ticker;
};

/// @brief  use TimerAlarmRepeating!  @ingroup timer_esp8266
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverESP8266;

}  // namespace audio_tools

#endif