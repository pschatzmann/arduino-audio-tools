#pragma once

#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#elif defined(__linux__)
#else
#include "FreeRTOS.h"
#include "task.h"
#endif
#include <functional>
#include "AudioTools/Concurrency/ITask.h"
#include "AudioLogger.h"

namespace audio_tools {

/**
 * @brief FreeRTOS task
 * 
 * @note Supported by all FreeRTOS platforms
 * 
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */
class Task : public ITask {
 public:
  /// Defines the task parameters; the actual FreeRTOS task is only created
  /// when begin() is called (typically from setup()), so that it is safe
  /// to declare a Task as a global object.
  /// @param name task name, used for debugging (e.g. in FreeRTOS trace
  /// tools); must stay valid for the lifetime of the task
  /// @param stackSizeWords stack depth in words, not bytes (stack size in
  /// bytes = stackSizeWords * sizeof(StackType_t)); too small a value can
  /// cause a stack overflow, which on some platforms (e.g.
  /// RP2040/arduino-pico) halts the whole system instead of failing
  /// gracefully
  /// @param priority FreeRTOS task priority; valid range and meaning are
  /// platform-specific (bounded by configMAX_PRIORITIES, which differs a
  /// lot between platforms, e.g. RP2040/arduino-pico only goes up to 7)
  /// @param core core to pin the task to (0 or 1), or -1 to let the
  /// scheduler decide; only relevant on multi-core targets
  Task(const char* name, int stackSizeWords, int priority = 1, int core = -1)
      : task_name(name),
        task_stack_size(stackSizeWords),
        task_priority(priority),
        task_core(core) {}

  Task() = default;
  ~Task() { remove(); }

  /// If you used the empty constructor, you need to call create!
  /// @param stackSizeWords stack depth in words, not bytes
  bool create(const char* name, int stackSizeWords, int priority = 1,
              int core = -1) {
    if (xHandle != 0) return false;
    // configMAX_PRIORITIES varies a lot between platforms (e.g.
    // RP2040/arduino-pico only goes up to 7); clamp to avoid undefined
    // behavior when a caller-supplied priority is out of range
    if (priority >= configMAX_PRIORITIES) {
       priority = configMAX_PRIORITIES - 1;
       LOGW("clamped task priority to %d ", priority);
    }
#ifdef ESP32
    if (core >= 0)
      xTaskCreatePinnedToCore(task_loop, name, stackSizeWords, this, priority,
                              &xHandle, core);
    else
      xTaskCreate(task_loop, name, stackSizeWords, this, priority, &xHandle);
    suspend();
#else
    xTaskCreate(task_loop, name, stackSizeWords, this, priority, &xHandle);
    suspend();
#if defined(configUSE_CORE_AFFINITY) && (configUSE_CORE_AFFINITY == 1) && \
    (configNUMBER_OF_CORES > 1)
    // e.g. RP2040 (Earle Philhower core): pin the task to a core after
    // creation via the FreeRTOS SMP core-affinity API
    if (core >= 0) vTaskCoreAffinitySet(xHandle, (1 << core));
#endif
#endif
    return true;
  }

  /// deletes the FreeRTOS task
  void remove() {
    if (xHandle != nullptr) {
      suspend();
      vTaskDelete(xHandle);
      xHandle = nullptr;
      task_suspended = false;
    }
  }

  bool begin(std::function<void()> process) {
    LOGI("staring task");
    if (xHandle == nullptr && task_name != nullptr) {
      create(task_name, task_stack_size, task_priority, task_core);
    }
    loop_code = process;
    resume();
    return true;
  }

  /// suspends the task
  void end() { suspend(); }

  void suspend() {
    vTaskSuspend(xHandle);
    task_suspended = true;
  }

  void resume() {
    vTaskResume(xHandle);
    task_suspended = false;
  }

  /// true if the task was created and is currently suspended
  bool isSuspended() { return xHandle != nullptr && task_suspended; }

  /// true if the task was never created or has been removed
  bool isEnded() { return xHandle == nullptr; }

  /// true if the task was created and is not suspended
  bool isRunning() { return xHandle != nullptr && !task_suspended; }

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
  const char* task_name = nullptr;
  int task_stack_size = 0;
  int task_priority = 1;
  int task_core = -1;
  bool task_suspended = false;

  static void nop() { delay(100); }

  static void task_loop(void* arg) {
    Task* self = (Task*)arg;
    while (true) {
      self->loop_code();
    }
  }
};

}  // namespace audio_tools

