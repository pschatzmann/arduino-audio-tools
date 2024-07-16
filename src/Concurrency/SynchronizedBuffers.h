
#pragma once
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/AudioLogger.h"

#ifdef ESP32
#  include "freertos/FreeRTOS.h"
#  include "Concurrency/QueueRTOS.h"
#  if ESP_IDF_VERSION_MAJOR >= 4 
#    include <freertos/stream_buffer.h>
#  endif
#else
#  include "stream_buffer.h"
#endif

#include "LockGuard.h"

namespace audio_tools {

/**
 * @brief Wrapper class that can turn any Buffer into a thread save
 * implementation.
 * @ingroup buffers
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 * @tparam T
 */
template <typename T> 
class SynchronizedBuffer : public BaseBuffer<T> {
public:
  SynchronizedBuffer(BaseBuffer<T> &buffer, Mutex &mutex) {
    p_buffer = &buffer;
    p_mutex = &mutex;
  }

  // reads a single value
  T read() override {
    TRACED();
    LockGuard guard(p_mutex);
    return p_buffer->read();
  }

  // reads multiple values
  int readArray(T data[], int len) {
    TRACED();
    LockGuard guard(p_mutex);
    int lenResult = MIN(len, available());
    for (int j = 0; j < lenResult; j++) {
      data[j] = p_buffer->read();
    }
    return lenResult;
  }

  int writeArray(const T data[], int len) {
    LOGD("%s: %d", LOG_METHOD, len);
    LockGuard guard(p_mutex);
    int result = 0;
    for (int j = 0; j < len; j++) {
      if (p_buffer->write(data[j]) == 0) {
        break;
      }
      result = j + 1;
    }
    return result;
  }

  // peeks the actual entry from the buffer
  T peek() override {
    TRACED();
    LockGuard guard(p_mutex);
    return p_buffer->peek();
  }

  // checks if the buffer is full
  bool isFull() override { return p_buffer->isFull(); }

  bool isEmpty() { return available() == 0; }

  // write add an entry to the buffer
  bool write(T data) override {
    TRACED();
    LockGuard guard(p_mutex);
    return p_buffer->write(data);
  }

  // clears the buffer
  void reset() override {
    TRACED();
    LockGuard guard(p_mutex);
    p_buffer->reset();
  }

  // provides the number of entries that are available to read
  int available() override {
    TRACED();
    LockGuard guard(p_mutex);
    return p_buffer->available();
  }

  // provides the number of entries that are available to write
  int availableForWrite() override {
    TRACED();
    LockGuard guard(p_mutex);
    return p_buffer->availableForWrite();
  }

  // returns the address of the start of the physical read buffer
  T *address() override {
    TRACED();
    return p_buffer->address();
  }

  size_t size() {
    return p_buffer->size();
  }

protected:
  BaseBuffer<T> *p_buffer = nullptr;
  Mutex *p_mutex = nullptr;
};

/**
 * @brief NBuffer which uses some RTOS queues to manage the available and filled buffers
 * @ingroup buffers
 * @ingroup concurrency
 * @tparam T 
 * @tparam COUNT number of buffers
 */
template <typename T> 
class SynchronizedNBuffer : public NBuffer<T> {
public:
  SynchronizedNBuffer(int bufferSize, int bufferCount, int writeMaxWait=portMAX_DELAY, int readMaxWait=portMAX_DELAY) {
    TRACED();
    max_size = bufferSize * bufferCount;
    NBuffer<T>::buffer_count = bufferCount;
    NBuffer<T>::buffer_size = bufferSize;

    available_buffers.resize(bufferCount);
    filled_buffers.resize(bufferCount);

    setReadMaxWait(readMaxWait);
    setWriteMaxWait(writeMaxWait);

    // setup buffers
    NBuffer<T>::write_buffer_count = 0;
    for (int j = 0; j < bufferCount; j++) {
      BaseBuffer<T> *tmp = new SingleBuffer<T>(bufferSize);
      if (tmp != nullptr) {
        available_buffers.enqueue(tmp);
      } else {
        LOGE("Not Enough Memory for buffer %d", j);
      }
    }
  }

  void setReadMaxWait(TickType_t ticks){
      available_buffers.setReadMaxWait(ticks);
      filled_buffers.setReadMaxWait(ticks);
  }

  void setWriteMaxWait(TickType_t ticks){
      available_buffers.setWriteMaxWait(ticks);
      filled_buffers.setWriteMaxWait(ticks);
  }

  size_t size() {
    return max_size;
  }

protected:
  QueueRTOS<BaseBuffer<T>*> available_buffers{0,portMAX_DELAY,0};
  QueueRTOS<BaseBuffer<T>*> filled_buffers{0,portMAX_DELAY,0};
  size_t max_size;

  BaseBuffer<T> *getNextAvailableBuffer() {
    TRACED();
    BaseBuffer<T>* result;
    return available_buffers.dequeue(result) ? result : nullptr;
  }

  bool addAvailableBuffer(BaseBuffer<T> *buffer) {
    TRACED();
    return available_buffers.enqueue(buffer);
  }

  BaseBuffer<T> *getNextFilledBuffer() {
    TRACED();
    BaseBuffer<T>* result;
    return filled_buffers.dequeue(result) ? result : nullptr;
  }

  bool addFilledBuffer(BaseBuffer<T> *buffer) {
    TRACED();
    return filled_buffers.enqueue(buffer);
  }
};


} // namespace audio_tools

