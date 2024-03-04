#pragma once

#ifdef ESP32
#  include "freertos/FreeRTOS.h"
#  include "freertos/task.h"
#else
#  include "FreeRTOS.h"
#  include "task.h"
#endif
namespace audio_tools {

/**
 * FreeRTOS task
 */
class Task {
 public:
  Task(const char* name, int stackSize, int priority = 1, int core = -1) {
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
  }

  ~Task() {
    if (xHandle != NULL) {
      vTaskDelete(xHandle);
    }
  }

  void begin(void (*process)()) {
    LOGI("staring task");
    loop_code = process;
    resume();
  }

  void suspend() { vTaskSuspend(xHandle); }

  void resume() { vTaskResume(xHandle); }

 protected:
  TaskHandle_t xHandle = nullptr;
  void (*loop_code)() = nop;

  static void nop() { delay(100); }

  static void task_loop(void* arg) {
    Task* self = (Task*)arg;
    while (true) {
      self->loop_code();
    }
  }
};

}