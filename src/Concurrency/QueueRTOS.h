#pragma once
#include "AudioBasic/Collections/Allocator.h"
#include "AudioConfig.h"

#ifdef ESP32
#  include <freertos/queue.h>
#  include "freertos/FreeRTOS.h"
#else
#  include "FreeRTOS.h"
#  include "queue.h"
#endif

namespace audio_tools {

/**
 * @brief FIFO Queue whch is based on the FreeRTOS queue API.
 * The default allocator will allocate the memory from psram if available
 * @ingroup collections
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T>
class QueueRTOS {
 public:
  QueueRTOS(int size, TickType_t writeMaxWait = portMAX_DELAY,
            TickType_t readMaxWait = portMAX_DELAY,
            Allocator& allocator = DefaultAllocator) {
    TRACED();
    p_allocator = &allocator;
    read_max_wait = readMaxWait;
    write_max_wait = writeMaxWait;
    queue_size = size;
    setup();
  };

  ~QueueRTOS() {
    TRACED();
    end();
  }

  void setReadMaxWait(TickType_t ticks) { read_max_wait = ticks; }

  void setWriteMaxWait(TickType_t ticks) { write_max_wait = ticks; }

  /// (Re-)defines the size
  bool resize(int size) {
    bool result = true;
    TRACED();
    if (size != queue_size) {
      end();
      queue_size = size;
      result = setup();
    }
    return result;
  }

  bool enqueue(T& data) {
    TRACED();
    if (xQueue==nullptr) return false;
    return xQueueSend(xQueue, (void*)&data, (TickType_t)write_max_wait);
  }

  bool peek(T& data) {
    TRACED();
    if (xQueue==nullptr) return false;
    return xQueuePeek(xQueue, &data, (TickType_t)read_max_wait);
  }

  bool dequeue(T& data) {
    TRACED();
    if (xQueue==nullptr) return false;
    return xQueueReceive(xQueue, &data, (TickType_t)read_max_wait);
  }

  size_t size() { return queue_size; }

  bool clear() {
    TRACED();
    if (xQueue==nullptr) return false;
    xQueueReset(xQueue);
    return true;
  }

  bool empty() { return size() == 0; }

 protected:
  QueueHandle_t xQueue = nullptr;
  TickType_t write_max_wait = portMAX_DELAY;
  TickType_t read_max_wait = portMAX_DELAY;
  Allocator* p_allocator = nullptr;
  int queue_size;
  uint8_t* p_data = nullptr;
  StaticQueue_t queue_buffer;

  bool setup() {
    if (queue_size > 0) {
#if configSUPPORT_STATIC_ALLOCATION
      p_data = (uint8_t*)p_allocator->allocate((queue_size + 1) * sizeof(T));
      if (p_data == nullptr) return false;
      xQueue = xQueueCreateStatic(queue_size, sizeof(T), p_data, &queue_buffer);
#else
      xQueue = xQueueCreate(queue_size, sizeof(T));
#endif
      if (xQueue == nullptr) return false;
    }
    return true;
  }

  void end() {
    if (xQueue != nullptr) vQueueDelete(xQueue);
    if (p_data != nullptr) p_allocator->free(p_data);
  }
};

}  // namespace audio_tools

