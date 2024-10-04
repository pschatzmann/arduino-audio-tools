#pragma once
/**
 * @defgroup timer Timers
 * @ingroup tools
 * @brief Platform independent timer API
 */
#include "AudioConfig.h"
#if defined(USE_TIMER)
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerAVR.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerBase.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerESP32.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerESP8266.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerMBED.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerRP2040.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerRenesas.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerSTM32.h"
#include "AudioTools/CoreAudio/AudioLogger.h"

namespace audio_tools {

/**
 * @brief Common Interface definition for TimerAlarmRepeating
 * @ingroup timer
 */
class TimerAlarmRepeating {
 public:
  /// @brief Default constructor
  TimerAlarmRepeating() = default;
  /**
   * @brief Construct a new Timer Alarm Repeating object by passing your object
   * which has been customized with some special platform specific parameters
   * @param timer
   */
  TimerAlarmRepeating(TimerAlarmRepeatingDriverBase& timer) {
    p_timer = &timer;
  };
  virtual ~TimerAlarmRepeating() = default;

  bool begin(repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) {
    // stop timer if it is already active          
    if (is_active) end();
    // start timer
    is_active = p_timer->begin(callback_f, time, unit);
    return is_active;
  }
  bool end() {
    is_active = false;
    return p_timer->end();
  };

  void setCallbackParameter(void* obj) { p_timer->setCallbackParameter(obj); }

  void* callbackParameter() { return p_timer->callbackParameter(); }

  virtual void setTimer(int timer) { p_timer->setTimer(timer); }

  virtual void setTimerFunction(TimerFunction function = DirectTimerCallback) {
    p_timer->setTimerFunction(function);
  }

  void setIsSave(bool is_save) { p_timer->setIsSave(is_save); }

  /// @brief Returns true if the timer is active
  operator bool() { return is_active; }

  /// Provides access to the driver
  TimerAlarmRepeatingDriverBase* driver() { return p_timer; }

 protected:
  void* object = nullptr;
  bool is_active = false;
  TimerAlarmRepeatingDriver timer;  // platform specific timer
  TimerAlarmRepeatingDriverBase* p_timer = &timer;
};

}  // namespace audio_tools

#endif
