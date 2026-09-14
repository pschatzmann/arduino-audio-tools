#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/Concurrency/Mutex.h"

#include <zephyr/kernel.h>

namespace audio_tools {

/**
 * @brief Mutex implementation using Zephyr kernel mutex.
 * 
 * @note Supported by all Zephyr platforms
 * 
 * @ingroup concurrency
 */
class MutexZephyr : public MutexBase {
 public:
  MutexZephyr() { k_mutex_init(&mutex); }
  virtual ~MutexZephyr() = default;

  void lock() override { k_mutex_lock(&mutex, K_FOREVER); }

  void unlock() override { k_mutex_unlock(&mutex); }

 protected:
  struct k_mutex mutex;
};

/**
 * @brief Recursive Mutex implementation using Zephyr kernel mutex.
 * @ingroup concurrency
 */
class MutexRecursiveZephyr : public MutexBase {
 public:
  MutexRecursiveZephyr() { k_mutex_init(&mutex); }
  virtual ~MutexRecursiveZephyr() = default;

  void lock() override { k_mutex_lock(&mutex, K_FOREVER); }

  void unlock() override { k_mutex_unlock(&mutex); }

 protected:
  struct k_mutex mutex;
};

/// @brief Compatibility typedefs for RTOS-based mutex naming
using MutexRTOS = MutexZephyr;
using MutexRecursiveRTOS = MutexRecursiveZephyr;

/// @brief Default Mutex implementation using Zephyr mutexes
/// @ingroup concurrency
using Mutex = MutexZephyr;

}  // namespace audio_tools
