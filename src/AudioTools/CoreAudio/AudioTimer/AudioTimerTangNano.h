#pragma once

#if defined(ARDUINO_ARCH_TANGNANO20K)
#include "AudioTools/CoreAudio/AudioTimer/AudioTimerBase.h"
#include "tangnano20k_soc.h"
#include "tangnano20k_timer.h"

namespace audio_tools {

/**
 * @brief Repeating Timer functions for the Tang Nano 20K (PicoRV32 soft-SoC):
 * Please use the typedef AudioTimer. We use the software timer engine of the
 * core which provides up to 5 timers (one is reserved for tone()). The
 * callback is executed in interrupt context.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioTimerDriverTangNano : public AudioTimerDriverBase {
 public:
  AudioTimerDriverTangNano() = default;

  bool begin(repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    LOGI("timer time: %u %s", (unsigned int)time, toString(unit));
    if (time == 0) {
      LOGE("Invalid time: 0");
      return false;
    }
    instanceCallback = callback_f;

    uint32_t ticks = 0;
    switch (unit) {
      case MS:
        ticks = (uint64_t)time * TANGNANO20K_CLK_FREQ / 1000ULL;
        break;
      case US:
        ticks = (uint64_t)time * TANGNANO20K_CLK_FREQ / 1000000ULL;
        break;
      case HZ:
        ticks = TANGNANO20K_CLK_FREQ / time;
        break;
      default:
        LOGE("Undefined Unit");
        return false;
    }
    if (ticks == 0) ticks = 1;

    if (handle < 0) {
      handle = tangnano20k_sw_timer_alloc();
      if (handle < 0 || handle >= TANGNANO20K_SW_TIMER_COUNT) {
        LOGE("No free timer available");
        handle = -1;
        return false;
      }
    }
    self()[handle] = this;
    if (!tangnano20k_sw_timer_start(handle, trampolines()[handle], ticks,
                                    true)) {
      LOGE("Could not start timer");
      end();
      return false;
    }
    return true;
  }

  bool end() override {
    if (handle < 0) return false;
    tangnano20k_sw_timer_release(handle);
    self()[handle] = nullptr;
    handle = -1;
    return true;
  }

 protected:
  int handle = -1;
  repeating_timer_callback_t instanceCallback = nullptr;
  // the core callback has no parameter: so we map the timer slot to the
  // driver instance
  static AudioTimerDriverTangNano **self() {
    static AudioTimerDriverTangNano *result[TANGNANO20K_SW_TIMER_COUNT] = {};
    return result;
  }

  template <int N>
  static void trampoline() {
    AudioTimerDriverTangNano *p = self()[N];
    if (p != nullptr && p->instanceCallback != nullptr)
      p->instanceCallback(p->object);
  }

  static_assert(TANGNANO20K_SW_TIMER_COUNT == 6,
                "trampolines() must match TANGNANO20K_SW_TIMER_COUNT");

  static void (**trampolines())(void) {
    static void (*result[TANGNANO20K_SW_TIMER_COUNT])(void) = {
        trampoline<0>, trampoline<1>, trampoline<2>,
        trampoline<3>, trampoline<4>, trampoline<5>};
    return result;
  }
};

/// @brief use AudioTimer! @ingroup timer_tangnano
using AudioTimerDriver = AudioTimerDriverTangNano;

}  // namespace audio_tools

#endif
