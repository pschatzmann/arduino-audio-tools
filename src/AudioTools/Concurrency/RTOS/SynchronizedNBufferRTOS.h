#pragma once
#include "AudioConfig.h"
#include "QueueRTOS.h"

namespace audio_tools {

/**
 * @brief NBuffer which uses some RTOS queues to manage the available and filled buffers
 * @ingroup buffers
 * @ingroup concurrency
 * @tparam T 
 * @tparam COUNT number of buffers
 */
template <typename T> 
class SynchronizedNBufferRTOST : public NBuffer<T> {
public:
  SynchronizedNBufferRTOST(int bufferSize, int bufferCount, int writeMaxWait=portMAX_DELAY, int readMaxWait=portMAX_DELAY) {
    TRACED();
    read_max_wait = readMaxWait;
    write_max_wait = writeMaxWait;
    resize(bufferSize, bufferCount);
  }
  ~SynchronizedNBufferRTOST(){
    cleanup();
  }

  void resize(int bufferSize, int bufferCount) {
    TRACED();
    if (buffer_size == bufferSize && buffer_count == bufferCount){
      return;
    }

    max_size = bufferSize * bufferCount;
    NBuffer<T>::buffer_count = bufferCount;
    NBuffer<T>::buffer_size = bufferSize;

    cleanup();
    available_buffers.resize(bufferCount);
    filled_buffers.resize(bufferCount);

    setReadMaxWait(read_max_wait);
    setWriteMaxWait(write_max_wait);

    // setup buffers
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

  int bufferCountFilled() {
      return filled_buffers.size();
  }

  int bufferCountEmpty() {
      return available_buffers.size();
  }

protected:
  QueueRTOS<BaseBuffer<T>*> available_buffers{0,portMAX_DELAY,0};
  QueueRTOS<BaseBuffer<T>*> filled_buffers{0,portMAX_DELAY,0};
  size_t max_size;
  size_t read_max_wait, write_max_wait;
  int buffer_size = 0, buffer_count = 0;

  /// Removes all allocated buffers
  void cleanup(){
    TRACED();
    BaseBuffer<T>* buffer = nullptr;;
    while (available_buffers.dequeue(buffer)){
      delete buffer;
    }
    while (filled_buffers.dequeue(buffer)){
      delete buffer;
    }
  }

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

using SynchronizedNBufferRTOS = SynchronizedNBufferRTOST<uint8_t>;
using SynchronizedNBuffer = SynchronizedNBufferRTOS;

}  // namespace audio_tools