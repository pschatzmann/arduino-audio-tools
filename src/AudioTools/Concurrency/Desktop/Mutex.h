#pragma once

#include "AudioToolsConfig.h"
#include "AudioTools/Concurrency/Mutex.h"
#include <mutex>

namespace audio_tools {


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

/**
 * @brief Mutex implemntation based on std::mutex
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StdRecursiveMutex : public MutexBase {
 public:
  void lock() override { std_mutex.lock(); }
  void unlock() override { std_mutex.unlock(); }

 protected:
  std::recursive_mutex std_mutex;
};

/// @brief Default Mutex implementation using std::mutex
/// @ingroup concurrency
using Mutex = StdMutex;


}  // namespace audio_tools
