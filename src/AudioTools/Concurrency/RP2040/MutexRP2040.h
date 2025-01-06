#pragma once
#include "AudioLogger.h"
#include "AudioTools/Concurrency/Mutex.h"

namespace audio_tools {

/**
 * @brief Disable, enable interrupts (only on the actual core)
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */
class NoInterruptHandler : public MutexBase {
 public:
  void lock() override {
    TRACED();
    noInterrupts();
  }
  void unlock() override {
    TRACED();
    interrupts();
  }
};

/**
 * @brief Mutex API for non IRQ mutual exclusion between cores.
 * Mutexes are application level locks usually used protecting data structures
 * that might be used by multiple threads of execution. Unlike critical
 * sections, the mutex protected code is not necessarily required/expected
 * to complete quickly, as no other sytem wide locks are held on account of
 * an acquired mutex.
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 
 */

class MutexRP2040 : public MutexBase {
 public:
  MutexRP2040() {
    TRACED();
    mutex_init(&mtx);
  }
  virtual ~MutexRP2040() = default;

  void lock() override {
    TRACED();
    mutex_enter_blocking(&mtx);
  }
  void unlock() override {
    TRACED();
    mutex_exit(&mtx);
  }

 protected:
  mutex_t mtx;
};

using Mutex = MutexRP2040;

}  // namespace audio_tools