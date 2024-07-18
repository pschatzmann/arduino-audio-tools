#pragma once
#include "AudioConfig.h"
#include "AudioTools/AudioLogger.h"

#ifdef USE_STD_CONCURRENCY
#  include <mutex>
#endif

#ifdef ESP32
#  include "freertos/FreeRTOS.h"
#  include "freertos/semphr.h"
#else
#  include "FreeRTOS.h"
#  include "semphr.h"
#endif

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

#if defined(USE_STD_CONCURRENCY) 

/**
 * @brief Mutex implemntation based on std::mutex
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StdMutex : public MutexBase {
public:
  void lock() override { std_mutex.lock(); }
  void unlock() override { std_mutex.unlock(); }

protected:
  std::mutex std_mutex;
};

#endif


/**
 * @brief Mutex implemntation using FreeRTOS
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */
class Mutex : public MutexBase {
public:
  Mutex() {
    TRACED();
    xSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xSemaphore);
  }
  ~Mutex() {
    TRACED();
    vSemaphoreDelete(xSemaphore);
  }
  void lock() override {
    TRACED();
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
  }
  void unlock() override {
    TRACED();
    xSemaphoreGive(xSemaphore);
  }

protected:
  SemaphoreHandle_t xSemaphore = NULL;
};


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
  LockGuard(Mutex &mutex) {
    TRACED();
    p_mutex = &mutex;
    p_mutex->lock();
  }
  LockGuard(Mutex *mutex) {
    TRACED();
    p_mutex = mutex;
    p_mutex->lock();
  }
  ~LockGuard() {
    TRACED();
    p_mutex->unlock();
  }

protected:
  Mutex *p_mutex = nullptr;
};

}

