#pragma once

#include "AudioTimerBase.h"

#if defined(USE_TIMER) && defined(USE_CPP_TASK)

#include <atomic>
#include <chrono>
#include <thread>

namespace audio_tools {

/**
 * @brief Repeating software timer driver for Desktop using std::thread.
 *
 * Provides a lightweight implementation of `TimerAlarmRepeatingDriverBase` that
 * spawns a dedicated thread and invokes the supplied callback at a fixed
 * interval using `sleep_until` for drift minimization.
 *
 * Characteristics:
 * - Supports US / MS / SEC periods (converted to microseconds)
 * - Safe re-start (calling begin() again stops the previous thread)
 * - Thread joins cleanly on `end()` and in destructor
 * - Ignores timer function mode (all modes map to same behavior on Desktop)
 *
 * @ingroup timer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TimerAlarmRepeatingDriverLinux : public TimerAlarmRepeatingDriverBase {
 public:
  TimerAlarmRepeatingDriverLinux() = default;
  ~TimerAlarmRepeatingDriverLinux() { end(); }

  bool begin(repeating_timer_callback_t callback_f, uint32_t time,
             TimeUnit unit = MS) override {
    // Stop any existing timer first
    end();
    if (callback_f == nullptr || time == 0) return false;
    callback = callback_f;
    period_us = toMicroseconds(time, unit);
    running.store(true);
    worker = std::thread([this]() { this->runLoop(); });
    return true;
  }

  bool end() override {
    if (running.load()) {
      running.store(false);
      if (worker.joinable()) worker.join();
    }
    return true;
  }

 protected:
  std::thread worker;
  std::atomic<bool> running{false};
  repeating_timer_callback_t callback = nullptr;
  uint64_t period_us = 0;

  static uint64_t toMicroseconds(uint32_t value, TimeUnit unit) {
    switch (unit) {
      case US:
        return value;
      case MS:
        return static_cast<uint64_t>(value) * 1000ULL;
      default:
        return static_cast<uint64_t>(value) * 1000ULL;
    }
  }

  void runLoop() {
    auto next = std::chrono::steady_clock::now();
    while (running.load()) {
      next += std::chrono::microseconds(period_us);
      if (callback) {
        // Execute callback; pass stored object pointer
        callback(callbackParameter());
      }
      std::this_thread::sleep_until(next);
    }
  }
};

/// @brief  use TimerAlarmRepeating!  @ingroup timer_esp32
using TimerAlarmRepeatingDriver = TimerAlarmRepeatingDriverLinux;


}  // namespace audio_tools

#endif  // USE_TIMER