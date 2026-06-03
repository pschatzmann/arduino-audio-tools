#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h"
#include "AudioToolsConfig.h"

#include <limits.h>
#include <zephyr/kernel.h>

#ifndef AUDIO_TOOLS_RTOS_TICK_TYPES_DEFINED
#define AUDIO_TOOLS_RTOS_TICK_TYPES_DEFINED
using TickType_t = uint32_t;
using BaseType_t = int;
#ifndef portMAX_DELAY
#define portMAX_DELAY UINT32_MAX
#endif
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) (ms)
#endif
#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef pdFALSE
#define pdFALSE 0
#endif
#endif

namespace audio_tools {

inline k_timeout_t rtosTimeoutFromTicks(TickType_t ticks) {
  return ticks == portMAX_DELAY ? K_FOREVER : K_MSEC(ticks);
}

/**
 * @brief FIFO Queue implementation based on Zephyr message queues.
 * @ingroup collections
 * @ingroup concurrency
 */
template <class T>
class QueueRTOS {
 public:
  QueueRTOS(int size, TickType_t writeMaxWait = portMAX_DELAY,
            TickType_t readMaxWait = portMAX_DELAY,
            Allocator &allocator = DefaultAllocator) {
    p_allocator = &allocator;
    read_max_wait = readMaxWait;
    write_max_wait = writeMaxWait;
    queue_size = size;
    setup();
  };

  ~QueueRTOS() { end(); }

  void setReadMaxWait(TickType_t ticks) { read_max_wait = ticks; }

  void setWriteMaxWait(TickType_t ticks) { write_max_wait = ticks; }

  /// (Re-)defines the size
  bool resize(int size) {
    bool result = true;
    if (size != queue_size) {
      end();
      queue_size = size;
      result = setup();
    }
    return result;
  }

  bool enqueue(T &data) {
    if (!is_active) return false;
    return k_msgq_put(&msgq, &data, rtosTimeoutFromTicks(write_max_wait)) == 0;
  }

  bool peek(T &data) {
    if (!is_active) return false;
    return k_msgq_peek(&msgq, &data) == 0;
  }

  bool dequeue(T &data) {
    if (!is_active) return false;
    return k_msgq_get(&msgq, &data, rtosTimeoutFromTicks(read_max_wait)) == 0;
  }

  size_t size() { return queue_size; }

  bool clear() {
    if (!is_active) return false;
    k_msgq_purge(&msgq);
    return true;
  }

  bool empty() {
    return k_msgq_num_used_get(&msgq) == 0;
  }

 protected:
  struct k_msgq msgq;
  TickType_t write_max_wait = portMAX_DELAY;
  TickType_t read_max_wait = portMAX_DELAY;
  Allocator *p_allocator = nullptr;
  int queue_size = 0;
  uint8_t *p_data = nullptr;
  bool is_active = false;

  bool setup() {
    if (queue_size <= 0) {
      is_active = false;
      return true;
    }
    p_data = (uint8_t *)p_allocator->allocate(queue_size * sizeof(T));
    if (p_data == nullptr) return false;

    k_msgq_init(&msgq, p_data, sizeof(T), queue_size);
    is_active = true;
    return true;
  }

  void end() {
    if (p_data != nullptr) {
      p_allocator->free(p_data);
      p_data = nullptr;
    }
    is_active = false;
  }
};

}  // namespace audio_tools
