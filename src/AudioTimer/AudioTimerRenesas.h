#pragma once

#if defined(ARDUINO_ARCH_RENESAS)
#include "AudioTimer/AudioTimerBase.h"
#include "FspTimer.h"
#include "IRQManager.h"

namespace audio_tools {

typedef void (*my_repeating_timer_callback_t)(void *obj);

/**
 * @brief Repeating Timer functions for repeated execution: Plaease use the
 * typedef TimerAlarmRepeating. Please note that in Renesas we only have one
 * high resolution timer (channel 1) available!
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class TimerAlarmRepeatingDriverRenesas : public TimerAlarmRepeatingDriverBase {
 public:
  TimerAlarmRepeatingDriverRenesas() {}

  /**
   * Starts the alarm timer
   */
  bool begin(const my_repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    bool result = false;
    LOGI("timer time: %u %s", (unsigned int)time, TimeUnitStr[unit]);
    this->instanceCallback = callback_f;

    float rate;
    // we determine the time in microseconds
    switch (unit) {
      case MS:
        rate = AudioTime::toRateMs(time);
        break;
      case US:
        rate = AudioTime::toRateUs(time);
        break;                                           
      case HZ:
        rate = time;
        break;                                           
      default:
        rate = 44100;
        LOGE("Undefined Unit");
    }
    if (rate < 550 || rate > 100000){
      LOGE("Unsopported rate: %d", rate);
    } else {
      LOGI("rate is %f hz", rate);
    }
    audio_timer.begin(TIMER_MODE_PERIODIC, type, timer_channel, rate * 2.0,
                      0.0f, staticCallback, this);
    IRQManager::getInstance().addPeripheral(IRQ_AGT, audio_timer.get_cfg());
    audio_timer.open();
    result = audio_timer.start();
    return result;
  }

  inline static void staticCallback(timer_callback_args_t *ptr) {
    TimerAlarmRepeatingDriverRenesas *self = (TimerAlarmRepeatingDriverRenesas *)ptr->p_context;
    self->instanceCallback(self->object);
  }

  // ends the timer and if necessary the task
  bool end() {
    audio_timer.end();
    return true;
  }

  void setTimer(int timer) override { timer_channel = timer; }

 protected:
  FspTimer audio_timer;
  my_repeating_timer_callback_t instanceCallback = nullptr;
  int timer_channel = 1;
  uint8_t type=AGT_TIMER;

};

/// @brief use TimerAlarmRepeating! @ingroup timer_rp2040
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverRenesas;

}  // namespace audio_tools

#endif