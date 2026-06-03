#pragma once

#include "AudioTools/Concurrency/Zephyr/QueueZephyr.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioToolsConfig.h"

#include <zephyr/kernel.h>

namespace audio_tools {

/**
 * @brief Buffer implementation using Zephyr semaphores + mutex as a bounded
 * ring buffer.
 * @ingroup buffers
 * @ingroup concurrency
 */
template <typename T>
class BufferZephyr : public BaseBuffer<T> {
 public:
  BufferZephyr(size_t streamBufferSize, size_t xTriggerLevel = 1,
               TickType_t writeMaxWait = portMAX_DELAY,
               TickType_t readMaxWait = portMAX_DELAY,
               Allocator &allocator = DefaultAllocator)
      : BaseBuffer<T>() {
    (void)xTriggerLevel;
    readWait = readMaxWait;
    writeWait = writeMaxWait;
    current_size_entries = streamBufferSize + 1;  // +1 for ring-buffer sentinel
    buffer_size_public = streamBufferSize;
    p_allocator = &allocator;

    if (streamBufferSize > 0) {
      setup();
    }
  }

  ~BufferZephyr() { end(); }

  /// Re-Allocates the memory and queue
  bool resize(size_t size) {
    bool result = true;
    size_t req_size = size + 1;
    if (current_size_entries != req_size) {
      end();
      current_size_entries = req_size;
      buffer_size_public = size;
      result = setup();
    }
    return result;
  }

  void setReadMaxWait(TickType_t ticks) { readWait = ticks; }

  void setWriteMaxWait(TickType_t ticks) { writeWait = ticks; }

  void setWriteFromISR(bool active) { write_from_isr = active; }

  void setReadFromISR(bool active) { read_from_isr = active; }

  // reads a single value
  bool read(T &result) override { return readArray(&result, 1) == 1; }

  // reads multiple values
  int readArray(T data[], int len) override {
    if (p_data == nullptr || data == nullptr || len <= 0) return 0;

    int result = 0;
    for (int j = 0; j < len; j++) {
      if (!readOne(data[j])) break;
      result++;
    }
    return result;
  }

  int writeArray(const T data[], int len) override {
    LOGD("%s: %d", LOG_METHOD, len);
    if (p_data == nullptr || data == nullptr || len <= 0) return 0;

    int result = 0;
    for (int j = 0; j < len; j++) {
      if (!writeOne(data[j])) break;
      result++;
    }
    return result;
  }

  // peeks the actual entry from the buffer
  bool peek(T &result) override {
    if (p_data == nullptr) return false;
    if (k_sem_count_get(&items_sem) == 0) return false;

    k_mutex_lock(&buffer_mutex, K_FOREVER);
    result = p_data[tail];
    k_mutex_unlock(&buffer_mutex);
    return true;
  }

  // checks if the buffer is full
  bool isFull() override { return availableForWrite() == 0; }

  bool isEmpty() {
    return available() == 0;
  }

  // write add an entry to the buffer
  bool write(T data) override { return writeArray(&data, 1) == 1; }

  // clears the buffer
  void reset() override {
    if (p_data == nullptr) return;

    k_mutex_lock(&buffer_mutex, K_FOREVER);
    head = 0;
    tail = 0;
    k_sem_init(&items_sem, 0, current_size_entries);
    k_sem_init(&spaces_sem, current_size_entries, current_size_entries);
    k_mutex_unlock(&buffer_mutex);
  }

  // provides the number of entries that are available to read
  int available() override { return k_sem_count_get(&items_sem); }

  // provides the number of entries that are available to write
  int availableForWrite() override { return k_sem_count_get(&spaces_sem); }

  // returns the address of the start of the physical read buffer
  T *address() override {
    LOGE("address() not implemented");
    return nullptr;
  }

  size_t size() override { return buffer_size_public; }

  operator bool() { return p_data != nullptr && size() > 0; }

 protected:
  T *p_data = nullptr;
  Allocator *p_allocator = nullptr;
  TickType_t readWait = portMAX_DELAY;
  TickType_t writeWait = portMAX_DELAY;
  bool read_from_isr = false;
  bool write_from_isr = false;
  size_t current_size_entries = 0;  // internal ring-buffer capacity (public size + 1)
  size_t buffer_size_public = 0;    // user-visible capacity

  struct k_mutex buffer_mutex;
  struct k_sem items_sem;
  struct k_sem spaces_sem;
  size_t head = 0;
  size_t tail = 0;

  /// Allocation has been postponed to be done here so that e.g. psram can be
  /// used.
  bool setup() {
    if (current_size_entries == 0) return true;

    if (p_data == nullptr) {
      p_data = static_cast<T *>(
          p_allocator->allocate(current_size_entries * sizeof(T)));
      if (p_data == nullptr) {
        LOGE("allocate failed for %d bytes",
             (int)(current_size_entries * sizeof(T)));
        return false;
      }
    }

    k_mutex_init(&buffer_mutex);
    k_sem_init(&items_sem, 0, current_size_entries);
    k_sem_init(&spaces_sem, current_size_entries, current_size_entries);
    head = 0;
    tail = 0;
    return true;
  }

  /// Release resources: call resize to restart again
  void end() {
    if (p_data != nullptr) {
      p_allocator->free(p_data);
    }
    current_size_entries = 0;
    buffer_size_public = 0;
    p_data = nullptr;
    head = 0;
    tail = 0;
  }

  bool readOne(T &value) {
    k_timeout_t timeout = read_from_isr ? K_NO_WAIT : rtosTimeoutFromTicks(readWait);
    if (k_sem_take(&items_sem, timeout) != 0) return false;

    k_mutex_lock(&buffer_mutex, K_FOREVER);
    value = p_data[tail];
    tail = (tail + 1) % current_size_entries;
    k_mutex_unlock(&buffer_mutex);

    k_sem_give(&spaces_sem);
    return true;
  }

  bool writeOne(const T &value) {
    k_timeout_t timeout = write_from_isr ? K_NO_WAIT : rtosTimeoutFromTicks(writeWait);
    if (k_sem_take(&spaces_sem, timeout) != 0) return false;

    k_mutex_lock(&buffer_mutex, K_FOREVER);
    p_data[head] = value;
    head = (head + 1) % current_size_entries;
    k_mutex_unlock(&buffer_mutex);

    k_sem_give(&items_sem);
    return true;
  }
};

/// @brief Compatibility typedef for RTOS-based buffer naming
template <class T>
using BufferRTOS = BufferZephyr<T>;

/// @brief Template alias for RTOS-based synchronized buffer
/// @ingroup concurrency
template <class T>
using SynchronizedBufferRTOS = BufferZephyr<T>;

}  // namespace audio_tools
