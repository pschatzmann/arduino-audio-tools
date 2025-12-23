#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/Concurrency/Mutex.h"

#ifdef ESP32
#  include "freertos/FreeRTOS.h"
#  include "freertos/semphr.h"
#elif defined(__linux__)
#else
#  include "FreeRTOS.h"
#  include "semphr.h"
#endif

namespace audio_tools {

/**
 * @brief Mutex implemntation using FreeRTOS
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */
class MutexRTOS : public MutexBase {
public:
  MutexRTOS() {
    xSemaphore = xSemaphoreCreateBinary();
    unlock();
  }
  virtual ~MutexRTOS() {
    vSemaphoreDelete(xSemaphore);
  }
  void lock() override {
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
  }
  void unlock() override {
    xSemaphoreGive(xSemaphore);
  }

protected:
  SemaphoreHandle_t xSemaphore = NULL;
};

/**
 * @brief Recursive Mutex implemntation using FreeRTOS
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */

class MutexRecursiveRTOS : public MutexBase {
public:
  MutexRecursiveRTOS() {
    xSemaphore = xSemaphoreCreateBinary();
    unlock();
  }
  virtual ~MutexRecursiveRTOS() {
    vSemaphoreDelete(xSemaphore);
  }
  void lock() override {
    xSemaphoreTakeRecursive(xSemaphore, portMAX_DELAY);
  }
  void unlock() override {
    xSemaphoreGiveRecursive(xSemaphore);
  }

protected:
  SemaphoreHandle_t xSemaphore = NULL;
};


/// @brief Default Mutex implementation using RTOS semaphores
/// @ingroup concurrency
using Mutex = MutexRTOS;

}