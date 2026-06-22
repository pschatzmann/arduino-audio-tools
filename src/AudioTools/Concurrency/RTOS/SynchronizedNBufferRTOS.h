#pragma once
#include "AudioToolsConfig.h"
#include "QueueRTOS.h"

#define DEFAULT_BUFFER_WAIT 1000

namespace audio_tools {

/**
 * @brief NBuffer which uses FreeRTOS queues to manage the available and filled
 *        buffers, making it safe for cross-core SPSC (single producer, single
 *        consumer) use on dual-core ESP32.
 *
 * The writer (e.g. sketch task, core 1) calls write()/writeArray()/availableForWrite().
 * The reader (e.g. USB task, core 0) calls read()/readArray()/available()/address().
 * Block ownership is transferred via FreeRTOS queues which provide the
 * necessary memory barriers for cross-core visibility.
 *
 * @note Supported by all FreeRTOS platforms
 *
 * @ingroup buffers
 * @ingroup concurrency
 * @tparam T
 */
template <typename T>
class SynchronizedNBufferRTOST : public NBuffer<T> {
 public:
  SynchronizedNBufferRTOST(int bufferSize, int bufferCount,
                            TickType_t writeMaxWait = DEFAULT_BUFFER_WAIT,
                            TickType_t readMaxWait = DEFAULT_BUFFER_WAIT) {
    TRACED();
    write_max_wait_ = writeMaxWait;
    read_max_wait_ = readMaxWait;
    resize(bufferSize, bufferCount);
  }

  ~SynchronizedNBufferRTOST() { cleanup(); }

  bool resize(size_t bufferSize, int bufferCount) {
    TRACED();
    if (NBuffer<T>::buffer_size == (int)bufferSize &&
        NBuffer<T>::buffer_count == bufferCount) {
      return true;
    }
    LOGW("resize: bufferSize=%d, bufferCount=%d", (int)bufferSize, bufferCount);

    // Update parent's tracking fields
    NBuffer<T>::buffer_count = bufferCount;
    NBuffer<T>::buffer_size = bufferSize;

    // Clear held pointers so we don't dangle after cleanup
    NBuffer<T>::actual_write_buffer = nullptr;
    NBuffer<T>::actual_read_buffer = nullptr;

    // Free old blocks and resize the RTOS queues
    cleanup();
    available_queue_.resize(bufferCount);
    filled_queue_.resize(bufferCount);
    applyWaitTimes();

    // Allocate new blocks into the available queue
    for (int j = 0; j < bufferCount; j++) {
      BaseBuffer<T>* tmp = new SingleBuffer<T>(bufferSize);
      if (tmp == nullptr) {
        LOGE("Not enough memory for buffer %d", j);
        return false;
      }
      available_queue_.enqueue(tmp);
    }
    return true;
  }

  /// Set how long the writer waits for a free block (0 = non-blocking)
  void setWriteMaxWait(TickType_t ticks) {
    write_max_wait_ = ticks;
    applyWaitTimes();
  }

  /// Set how long the reader waits for a filled block (0 = non-blocking)
  void setReadMaxWait(TickType_t ticks) {
    read_max_wait_ = ticks;
    applyWaitTimes();
  }

  size_t size() { return NBuffer<T>::buffer_size * NBuffer<T>::buffer_count; }

  int bufferCountFilled() { return filled_queue_.size(); }

  int bufferCountEmpty() { return available_queue_.size(); }

 protected:
  QueueRTOS<BaseBuffer<T>*> available_queue_{0, DEFAULT_BUFFER_WAIT, 0};
  QueueRTOS<BaseBuffer<T>*> filled_queue_{0, DEFAULT_BUFFER_WAIT, 0};
  TickType_t write_max_wait_ = DEFAULT_BUFFER_WAIT;
  TickType_t read_max_wait_ = DEFAULT_BUFFER_WAIT;

  /// Apply wait times with correct semantics:
  ///   Writer needs: available_queue_ READ wait + filled_queue_ WRITE wait
  ///   Reader needs: filled_queue_ READ wait + available_queue_ WRITE wait
  void applyWaitTimes() {
    available_queue_.setReadMaxWait(write_max_wait_);   // writer gets free block
    available_queue_.setWriteMaxWait(read_max_wait_);   // reader returns drained block
    filled_queue_.setWriteMaxWait(write_max_wait_);     // writer submits filled block
    filled_queue_.setReadMaxWait(read_max_wait_);       // reader gets filled block
  }

  void cleanup() {
    TRACED();
    BaseBuffer<T>* buffer = nullptr;
    while (available_queue_.dequeue(buffer)) {
      delete buffer;
    }
    while (filled_queue_.dequeue(buffer)) {
      delete buffer;
    }
  }

  // ── Virtual overrides that route NBuffer's block management through
  //    FreeRTOS queues instead of the non-thread-safe QueueFromVector. ────

  BaseBuffer<T>* getNextAvailableBuffer() override {
    TRACED();
    BaseBuffer<T>* result = nullptr;
    return available_queue_.dequeue(result) ? result : nullptr;
  }

  bool addAvailableBuffer(BaseBuffer<T>* buffer) override {
    TRACED();
    return available_queue_.enqueue(buffer);
  }

  BaseBuffer<T>* getNextFilledBuffer() override {
    TRACED();
    BaseBuffer<T>* result = nullptr;
    return filled_queue_.dequeue(result) ? result : nullptr;
  }

  bool addFilledBuffer(BaseBuffer<T>* buffer) override {
    TRACED();
    return filled_queue_.enqueue(buffer);
  }
};

/// @brief RTOS synchronized buffer for managing multiple audio buffers
/// @ingroup buffers
using SynchronizedNBufferRTOS = SynchronizedNBufferRTOST<uint8_t>;

/// @brief Default synchronized buffer alias
/// @ingroup buffers
using SynchronizedNBuffer = SynchronizedNBufferRTOS;

}  // namespace audio_tools
