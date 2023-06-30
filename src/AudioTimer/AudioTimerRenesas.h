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
      LOGE("Unsupported rate: %d", rate);
    } else {
      LOGI("rate is %f hz", rate);
    }
    return startTimer(rate);
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

  // setup timer
  bool startTimer(float rate) {
    uint8_t timer_type = GPT_TIMER;
    int8_t tindex = FspTimer::get_available_timer(timer_type);
    if (tindex==0){
      LOGE("Using pwm reserved timer");
      FspTimer::force_use_of_pwm_reserved_timer();
      int8_t tindex = FspTimer::get_available_timer(timer_type);
    }
    if (tindex==0){
      LOGE("no timer");
      return false;
    }
    LOGI("timer idx: %d", tindex);
    if(!audio_timer.begin(TIMER_MODE_PERIODIC, timer_type, tindex, rate, 0.0f, staticCallback)){
      LOGE("error:begin");
      return false;
    }
    
    TimerIrqCfg_t cfg;
    cfg.base_cfg = audio_timer.get_cfg();
    if (!IRQManager::getInstance().addTimerOverflow(cfg)){
      LOGE("error:addPeripheral");
    }
    if (!audio_timer.open()){
      LOGE("error:open");
      return false;
    }
    if (!audio_timer.start()){
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