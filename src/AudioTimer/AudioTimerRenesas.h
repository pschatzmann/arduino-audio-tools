#pragma once

#if defined(ARDUINO_ARCH_RENESAS)
#include "AudioTimer/AudioTimerBase.h"
#include "FspTimer.h"
#include "IRQManager.h"

namespace audio_tools {

typedef void (*my_repeating_timer_callback_t)(void *obj);

/**
 * @brief Repeating Timer functions for repeated execution: Plaease use the
 * typedef TimerAlarmRepeating. By default we use a new GPT timer. You can
 * also request 1 AGT timer by calling setTimer(1);
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
        LOGE("Undefined Unit");
        return false;
    }
    if (rate < 550 || rate > 100000) {
      LOGE("Unsupported rate: %f hz", rate);
      return false;
    } else {
      LOGI("rate is %f hz", rate);
    }

    // stop timer if it is active
    if (timer_active) {
      end();
    }

    LOGI("Using %s", timer_type == 1 ? "AGT" : "GPT")
    timer_active = timer_type == 1 ? startAGTTimer(rate) : startGPTTimer(rate);
    return timer_active;
  }

  inline static void staticCallback(timer_callback_args_t *ptr) {
    TimerAlarmRepeatingDriverRenesas *self =
        (TimerAlarmRepeatingDriverRenesas *)ptr->p_context;
    self->instanceCallback(self->object);
  }

  /// ends the timer and if necessary the task
  bool end() {
    TRACED();
    audio_timer.end();
    timer_active = false;
    return true;
  }

  /// Selects the timer type: 0=GPT and 1=AGT
  void setTimer(int timer) override { timer_type = timer; }

 protected:
  FspTimer audio_timer;
  my_repeating_timer_callback_t instanceCallback = nullptr;
  uint8_t timer_type = 0;  // Should be 0 - but this is currently not working
  bool timer_active = false;

  // starts Asynchronous General Purpose Timer (AGT) timer
  bool startAGTTimer(float rate) {
    TRACED();
    uint8_t timer_type = AGT_TIMER;
    // only channel 1 is available
    int timer_channel = 1;
    audio_timer.begin(TIMER_MODE_PERIODIC, timer_type, timer_channel,
                      rate * 2.0, 0.0f, staticCallback, this);
    IRQManager::getInstance().addPeripheral(IRQ_AGT, audio_timer.get_cfg());
    audio_timer.open();
    bool result = audio_timer.start();
    return result;
  }

  // setup General PWM Timer (GPT timer
  bool startGPTTimer(float rate) {
    TRACED();
    uint8_t timer_type = GPT_TIMER;
    int8_t tindex = FspTimer::get_available_timer(timer_type);
    if (tindex < 0) {
      LOGE("Using pwm reserved timer");
      tindex = FspTimer::get_available_timer(timer_type, true);
    }
    if (tindex < 0) {
      LOGE("no timer");
      return false;
    }
    FspTimer::force_use_of_pwm_reserved_timer();
    LOGI("timer idx: %d", tindex);
    if (!audio_timer.begin(TIMER_MODE_PERIODIC, timer_type, tindex, rate, 0.0f,
                           staticCallback, this)) {
      LOGE("error:begin");
      return false;
    }

    if (!audio_timer.setup_overflow_irq()) {
      LOGE("error:setup_overflow_irq");
      return false;
    }

    if (!audio_timer.open()) {
      LOGE("error:open");
      return false;
    }
    if (!audio_timer.start()) {
      LOGE("error:start");
      return false;
    }
    return true;
  }
};

/// @brief use TimerAlarmRepeating! @ingroup timer_rp2040
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverRenesas;

}  // namespace audio_tools

#endif