#pragma once

#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include "FreeRTOS.h"
#include "task.h"
#endif
#include <functional>

namespace audio_tools {

/**
 * @brief FreeRTOS task
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */
class Task {
 public:
  /// Defines and creates a FreeRTOS task
  Task(const char* name, int stackSize, int priority = 1, int core = -1) {
    create(name, stackSize, priority, core);
  }

  Task() = default;
  ~Task() { remove(); }

  /// If you used the empty constructor, you need to call create!
  bool create(const char* name, int stackSize, int priority = 1,
              int core = -1) {
    if (xHandle != 0) return false;
#ifdef ESP32
    if (core >= 0)
      xTaskCreatePinnedToCore(task_loop, name, stackSize, this, priority,
                              &xHandle, core);
    else
      xTaskCreate(task_loop, name, stackSize, this, priority, &xHandle);
#else
    xTaskCreate(task_loop, name, stackSize, this, priority, &xHandle);
#endif
    suspend();
    return true;
  }

  /// deletes the FreeRTOS task
  void remove() {
    if (xHandle != nullptr) {
      suspend();
      vTaskDelete(xHandle);
      xHandle = nullptr;
    }
  }

  bool begin(std::function<void()> process) {
    LOGI("staring task");
    loop_code = process;
    resume();
    return true;
  }

  /// suspends the task
  void end() { suspend(); }

  void suspend() { vTaskSuspend(xHandle); }

  void resume() { vTaskResume(xHandle); }

  TaskHandle_t &getTaskHandle() {
    return xHandle;
  }

  void setReference(void *r){
    ref = r;
  }

  void *getReference(){
    return ref;
  }

#ifdef ESP32
  int getCoreID() {
    return xPortGetCoreID();
  }
#endif

 protected:
  TaskHandle_t xHandle = nullptr;
  std::function<void()> loop_code = nop;
  void *ref;

  static void nop() { delay(100); }

  static void task_loop(void* arg) {
    Task* self = (Task*)arg;
    while (true) {
      self->loop_code();
    }
  }
};

}  // namespace audio_tools

