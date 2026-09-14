#pragma once

#include "AudioToolsConfig.h"

#include <atomic>

namespace audio_tools {

/**
 * @brief Empty Mutex implementation which does nothing
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MutexBase {
 public:
  virtual void lock() {}
  virtual void unlock() {}
};

/**
 * @brief Busy-wait lock based on std::atomic - available on all platforms
 * that support <atomic>.
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class SpinLock : public MutexBase {
 public:
  void lock() override {
    for (;;) {
      // Optimistically assume the lock is free on the first try
      if (!lock_.exchange(true, std::memory_order_acquire)) {
        return;
      }
      // Wait for lock to be released without generating cache misses
      while (lock_.load(std::memory_order_relaxed)) {
        // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
        // hyper-threads
        //__builtin_ia32_pause();
        delay(1);
      }
    }
  }

  bool try_lock() {
    // First do a relaxed load to check if lock is free in order to prevent
    // unnecessary cache misses if someone does while(!try_lock())
    return !lock_.load(std::memory_order_relaxed) &&
           !lock_.exchange(true, std::memory_order_acquire);
  }

  void unlock() override { lock_.store(false, std::memory_order_release); }

 protected:
  std::atomic<bool> lock_ = {false};
};

}  // namespace audio_tools
