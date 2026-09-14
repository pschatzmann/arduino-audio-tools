#pragma once

#include "AudioTools/CoreAudio/AudioLogger.h"

#include <functional>
#include <zephyr/kernel.h>

namespace audio_tools {

using TaskHandle_t = k_tid_t;

/**
 * @brief Zephyr thread based task.
 * 
 * @note Supported by all Zephyr platforms
 * 
 * @ingroup concurrency
 */
class TaskZephyr {
 public:
  /// Defines and creates a task
  TaskZephyr(const char *name, int stackSize, int priority = 1, int core = -1) {
    create(name, stackSize, priority, core);
  }

  TaskZephyr() = default;
  ~TaskZephyr() { remove(); }

  /// If you used the empty constructor, you need to call create!
  bool create(const char *name, int stackSize, int priority = 1,
              int core = -1) {
    (void)core;

    if (xHandle != nullptr) return false;

    stack_size = stackSize > 0 ? stackSize : 2048;
    p_stack = static_cast<k_thread_stack_t *>(k_malloc(stack_size));
    if (p_stack == nullptr) {
      LOGE("Task stack allocation failed");
      return false;
    }

    xHandle = k_thread_create(&thread_data, p_stack, stack_size, task_loop, this,
                              nullptr, nullptr, priority, 0, K_FOREVER);
    if (xHandle == nullptr) {
      k_free(p_stack);
      p_stack = nullptr;
      return false;
    }

    if (name != nullptr) {
      k_thread_name_set(xHandle, name);
    }

    is_started = false;
    return true;
  }

  /// deletes the task
  void remove() {
    if (xHandle != nullptr) {
      k_thread_abort(xHandle);
      xHandle = nullptr;
      is_started = false;
    }
    if (p_stack != nullptr) {
      k_free(p_stack);
      p_stack = nullptr;
    }
  }

  bool begin(std::function<void()> process) {
    LOGI("starting task");
    loop_code = process;
    resume();
    return true;
  }

  /// suspends the task
  void end() { suspend(); }

  void suspend() {
    if (xHandle != nullptr && is_started) {
      k_thread_suspend(xHandle);
    }
  }

  void resume() {
    if (xHandle == nullptr) return;

    if (!is_started) {
      k_thread_start(xHandle);
      is_started = true;
    } else {
      k_thread_resume(xHandle);
    }
  }

  TaskHandle_t &getTaskHandle() { return xHandle; }

  void setReference(void *r) { ref = r; }

  void *getReference() { return ref; }

 protected:
  struct k_thread thread_data;
  TaskHandle_t xHandle = nullptr;
  std::function<void()> loop_code = nop;
  void *ref = nullptr;
  bool is_started = false;
  size_t stack_size = 0;
  k_thread_stack_t *p_stack = nullptr;

  static void nop() { delay(100); }

  static void task_loop(void *arg, void *p2, void *p3) {
    (void)p2;
    (void)p3;

    TaskZephyr *self = (TaskZephyr *)arg;
    while (true) {
      self->loop_code();
    }
  }
};

/// @brief Compatibility typedef for RTOS-based task naming
using Task = TaskZephyr;

}  // namespace audio_tools
