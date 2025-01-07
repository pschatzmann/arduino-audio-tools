#pragma once
#include "AudioConfig.h"
#include "Mutex.h"

namespace audio_tools {

/**
 * @brief RAII implementaion using a Mutex: Only a few microcontrollers provide
 * lock guards, so I decided to roll my own solution where we can just use a
 * dummy Mutex implementation that does nothing for the cases where this is not
 * needed.
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */

class LockGuard {
 public:
  LockGuard(MutexBase &mutex) {
    p_mutex = &mutex;
    p_mutex->lock();
  }

  LockGuard(MutexBase *mutex) {
    p_mutex = mutex;
    if (p_mutex != nullptr) p_mutex->lock();
  }
  
  ~LockGuard() {
    if (p_mutex != nullptr) p_mutex->unlock();
  }

 protected:
  MutexBase *p_mutex = nullptr;
};

}  // namespace audio_tools
