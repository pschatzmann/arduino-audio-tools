
#pragma once
#include "AudioConfig.h"
#ifdef USE_CONCURRENCY
#include "AudioTools/Buffers.h"
#include "AudioTools/AudioLogger.h"

#ifdef ESP32
#  include "freertos/FreeRTOS.h"
#  include "AudioBasic/Collections/QueueFreeRTOS.h"
#  if ESP_IDF_VERSION_MAJOR >= 4 
#    include <freertos/stream_buffer.h>
#  endif
#endif

#ifdef USE_STD_CONCURRENCY
#include <mutex>
#endif

/**
 * @defgroup concurrency Concurrency
 * @ingroup tools
 * @brief Multicore support
 */


namespace audio_tools {

/**
 * @brief Empty Mutex implementation which does nothing
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MutexBase {
public:
  virtual void lock() {}
  virtual void unlock() {}
};

#if defined(USE_STD_CONCURRENCY) 

/**
 * @brief Mutex implemntation based on std::mutex
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StdMutex : public MutexBase {
public:
  void lock() override { std_mutex.lock(); }
  void unlock() override { std_mutex.unlock(); }

protected:
  std::mutex std_mutex;
};

#endif

#if defined(ESP32) 

/**
 * @brief Mutex implemntation using FreeRTOS
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */
class Mutex : public MutexBase {
public:
  Mutex() {
    TRACED();
    xSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xSemaphore);
  }
  ~Mutex() {
    TRACED();
    vSemaphoreDelete(xSemaphore);
  }
  void lock() override {
    TRACED();
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
  }
  void unlock() override {
    TRACED();
    xSemaphoreGive(xSemaphore);
  }

protected:
  SemaphoreHandle_t xSemaphore = NULL;
};

//#elif defined(ARDUINO_ARCH_RP2040)
// /**
//  * @brief Mutex implemntation using RP2040 API
//  * @ingroup concurrency
//  * @author Phil Schatzmann
//  * @copyright GPLv3 *
//  */
// class Mutex : public MutexBase {
// public:
//   Mutex() {
//     TRACED();
//     mutex_init(&mtx);
//   }
//   ~Mutex() { TRACED(); }
//   void lock() override {
//     TRACED();
//     mutex_enter_blocking(&mtx);
//   }
//   void unlock() override {
//     TRACED();
//     mutex_exit(&mtx);
//   }

// protected:
//   mutex_t mtx;
// };

#else

using Mutex = MutexBase;

#endif

/**
 * @brief RAII implementaion using a Mutex: Only a few microcontrollers provide
 * lock guards, so I decided to roll my own solution where we can just use a
 * dummy Mutex implementation that does nothing for the cases where this is not
 * needed.
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */
class LockGuard {
public:
  LockGuard(Mutex &mutex) {
    TRACED();
    p_mutex = &mutex;
    p_mutex->lock();
  }
  LockGuard(Mutex *mutex) {
    TRACED();
    p_mutex = mutex;
    p_mutex->lock();
  }
  ~LockGuard() {
    TRACED();
    p_mutex->unlock();
  }

protected:
  Mutex *p_mutex = nullptr;
};

/**
 * @brief Wrapper class that can turn any Buffer into a thread save
 * implementation.
 * @ingroup buffers
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

protected:
  BaseBuffer<T> *p_buffer = nullptr;
  Mutex *p_mutex = nullptr;
};

#if defined(ESP32) 

/**
 * @brief NBuffer which uses some RTOS queues to manage the available and filled buffers
 * @ingroup buffers
 * @tparam T 
 * @tparam COUNT number of buffers
 */
template <typename T> 
class SynchronizedNBuffer : public NBuffer<T> {
public:
  SynchronizedNBuffer(int bufferSize, int bufferCount, int writeMaxWait=portMAX_DELAY, int readMaxWait=portMAX_DELAY) {
    TRACED();
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


protected:
  QueueFreeRTOS<BaseBuffer<T>*> available_buffers{0,portMAX_DELAY,0};
  QueueFreeRTOS<BaseBuffer<T>*> filled_buffers{0,portMAX_DELAY,0};

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

#if ESP_IDF_VERSION_MAJOR >= 4 

/**
 * @brief Buffer implementation which is using a FreeRTOS StreamBuffer
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 * @tparam T
 */
template <typename T> 
class SynchronizedBufferRTOS : public BaseBuffer<T> {
public:
  SynchronizedBufferRTOS(size_t xStreamBufferSizeBytes, size_t xTriggerLevel=256, TickType_t writeMaxWait=portMAX_DELAY, TickType_t readMaxWait=portMAX_DELAY)
      : BaseBuffer<T>() {
    xStreamBuffer = xStreamBufferCreate(xStreamBufferSizeBytes, xTriggerLevel);
    readWait = readMaxWait;
    writeWait = writeMaxWait;
    current_size = xStreamBufferSizeBytes;
    trigger_level = xTriggerLevel;
  }
  ~SynchronizedBufferRTOS() { vStreamBufferDelete(xStreamBuffer); }

  void resize(size_t size){
    if (current_size != size){
      vStreamBufferDelete(xStreamBuffer);
      xStreamBuffer = xStreamBufferCreate(size, trigger_level);
      current_size = size;
    }
  }

  void setReadMaxWait(TickType_t ticks){
    readWait = ticks;
  }

  void setWriteMaxWait(TickType_t ticks){
    writeWait = ticks;
  }

  void setWriteFromISR(bool active){
    write_from_isr = active;
  }

  void setReadFromISR(bool active){
    read_from_isr = active;
  }

  // reads a single value
  T read() override {
    T data = 0;
    readArray(&data, sizeof(T));
    return data;
  }

  // reads multiple values
  int readArray(T data[], int len) {
    if (read_from_isr){
      xHigherPriorityTaskWoken = pdFALSE;
      int result = xStreamBufferReceiveFromISR(xStreamBuffer, (void *)data, sizeof(T) * len, &xHigherPriorityTaskWoken);
#ifdef ESP32X
      portYIELD_FROM_ISR(  );
#else
      portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
#endif
      return result;
    } else {
      return xStreamBufferReceive(xStreamBuffer, (void *)data, sizeof(T) * len, readWait);
    }
  }

  int writeArray(const T data[], int len) {
    LOGD("%s: %d", LOG_METHOD, len);
    if (write_from_isr){
      xHigherPriorityTaskWoken = pdFALSE;
      int result = xStreamBufferSendFromISR(xStreamBuffer, (void *)data, sizeof(T) * len, &xHigherPriorityTaskWoken);
#ifdef ESP32X
      portYIELD_FROM_ISR(  );
#else
      portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
#endif
      return result;
    } else {
      return xStreamBufferSend(xStreamBuffer, (void *)data, sizeof(T) * len, writeWait);
    }
  }

  // peeks the actual entry from the buffer
  T peek() override {
    LOGE("peek not implmented");
    return 0;
  }

  // checks if the buffer is full
  bool isFull() override {
    return xStreamBufferIsFull(xStreamBuffer) == pdTRUE;
  }

  bool isEmpty() { return xStreamBufferIsEmpty(xStreamBuffer) == pdTRUE; }

  // write add an entry to the buffer
  bool write(T data) override {
    int len = sizeof(T);
    return writeArray(&data, len) == len;
  }

  // clears the buffer
  void reset() override { xStreamBufferReset(xStreamBuffer); }

  // provides the number of entries that are available to read
  int available() override {
    return xStreamBufferBytesAvailable(xStreamBuffer);
  }

  // provides the number of entries that are available to write
  int availableForWrite() override {
    return xStreamBufferSpacesAvailable(xStreamBuffer);
  }

  // returns the address of the start of the physical read buffer
  T *address() override {
    LOGE("address() not implemented");
    return nullptr;
  }

protected:
  StreamBufferHandle_t xStreamBuffer;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;  // Initialised to pdFALSE.
  int readWait = portMAX_DELAY;
  int writeWait = portMAX_DELAY;
  bool read_from_isr = false;
  bool write_from_isr = false;
  size_t current_size=0;
  size_t trigger_level=0;
};
#endif // ESP_IDF_VERSION_MAJOR >= 4 
#endif // ESP32

} // namespace audio_tools

#endif

