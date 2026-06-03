#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/Concurrency/Mutex.h"

#include <zephyr/kernel.h>

namespace audio_tools {

/**
 * @brief Mutex implementation using Zephyr kernel mutex.
 * @ingroup concurrency
 */
class MutexRTOS : public MutexBase {
 public:
  MutexRTOS() { k_mutex_init(&mutex); }
  virtual ~MutexRTOS() = default;

  void lock() override { k_mutex_lock(&mutex, K_FOREVER); }

  void unlock() override { k_mutex_unlock(&mutex); }

 protected:
  struct k_mutex mutex;
};

/**
 * @brief Recursive Mutex implementation using Zephyr kernel mutex.
 * @ingroup concurrency
 */
class MutexRecursiveRTOS : public MutexBase {
 public:
  MutexRecursiveRTOS() { k_mutex_init(&mutex); }
  virtual ~MutexRecursiveRTOS() = default;

  void lock() override { k_mutex_lock(&mutex, K_FOREVER); }

  void unlock() override { k_mutex_unlock(&mutex); }

 protected:
  struct k_mutex mutex;
};

/// @brief Default Mutex implementation using RTOS mutexes
/// @ingroup concurrency
using Mutex = MutexRTOS;

}  // namespace audio_tools
