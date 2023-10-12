
#pragma once

#include "AudioBasic/Collections.h"
#include "AudioTools/AudioLogger.h"
#include <limits.h>         /* For INT_MAX */
#include <atomic>        


/**
 * @defgroup buffers Buffers
 * @ingroup tools
 * @brief Different Buffer Implementations
 */

namespace audio_tools {

// forward declaration
template <typename T>
class NBuffer;

/**
 * @brief Shared functionality of all buffers
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename T>
class BaseBuffer {
 public:
  BaseBuffer() = default;
  virtual ~BaseBuffer() = default;
  BaseBuffer(BaseBuffer const &) = delete;
  BaseBuffer &operator=(BaseBuffer const &) = delete;

  /// reads a single value
  virtual T read() = 0;

  /// reads multiple values
  virtual int readArray(T data[], int len) {
    int lenResult = MIN(len, available());
    for (int j = 0; j < lenResult; j++) {
      data[j] = read();
    }
    LOGD("readArray %d -> %d", len, lenResult);
    return lenResult;
  }

  /// Removes the next len entries 
  virtual int clearArray(int len) {
    int lenResult = MIN(len, available());
    for (int j = 0; j < lenResult; j++) {
      read();
    }
    return lenResult;
  }


  /// Fills the buffer data 
  virtual int writeArray(const T data[], int len) {
    // LOGD("%s: %d", LOG_METHOD, len);
    // CHECK_MEMORY();

    int result = 0;
    for (int j = 0; j < len; j++) {
      if (!write(data[j])) {
        break;
      }
      result = j + 1;
    }
    // CHECK_MEMORY();
    LOGD("writeArray %d -> %d", len, result);
    return result;
  }


  /// Fills the buffer data and overwrites the oldest data if the buffer is full
  virtual int writeArrayOverwrite(const T data[], int len) {
    int to_delete = len - availableForWrite();
    if (to_delete>0){
      clearArray(to_delete);
    }
    return writeArray(data, len);
  }


  /// reads multiple values for array of 2 dimensional frames
  int readFrames(T data[][2], int len) {
    LOGD("%s: %d", LOG_METHOD, len);
    // CHECK_MEMORY();
    int result = MIN(len, available());
    for (int j = 0; j < result; j++) {
      T sample = read();
      data[j][0] = sample;
      data[j][1] = sample;
    }
    // CHECK_MEMORY();
    return result;
  }

  template <int rows, int channels>
  int readFrames(T (&data)[rows][channels]) {
    int lenResult = MIN(rows, available());
    for (int j = 0; j < lenResult; j++) {
      T sample = read();
      for (int i = 0; i < channels; i++) {
        // data[j][i] = htons(sample);
        data[j][i] = sample;
      }
    }
    return lenResult;
  }

  /// peeks the actual entry from the buffer
  virtual T peek() = 0;

  /// checks if the buffer is full
  virtual bool isFull() = 0;

  bool isEmpty() { return available() == 0; }

  /// write add an entry to the buffer
  virtual bool write(T data) = 0;

  /// clears the buffer
  virtual void reset() = 0;

  ///  same as reset
  void clear() { reset(); }

  /// provides the number of entries that are available to read
  virtual int available() = 0;

  /// provides the number of entries that are available to write
  virtual int availableForWrite() = 0;

  /// returns the address of the start of the physical read buffer
  virtual T *address() = 0;

 protected:
  void setWritePos(int pos){};

  friend NBuffer<T>;
};

/**
 * @brief A simple Buffer implementation which just uses a (dynamically sized)
 * array
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <typename T>
class SingleBuffer : public BaseBuffer<T> {
 public:
  /**
   * @brief Construct a new Single Buffer object
   *
   * @param size
   */
  SingleBuffer(int size) {
    this->max_size = size;
    buffer.resize(max_size);
    reset();
  }

  /**
   * @brief Construct a new Single Buffer w/o allocating any memory
   */
  SingleBuffer() { reset(); }

  /// notifies that the external buffer has been refilled
  void onExternalBufferRefilled(void *data, int len) {
    this->owns_buffer = false;
    this->buffer = (uint8_t *)data;
    this->current_read_pos = 0;
    this->current_write_pos = len;
  }

  bool write(T sample) override {
    bool result = false;
    if (current_write_pos < max_size) {
      buffer[current_write_pos++] = sample;
      result = true;
    }
    return result;
  }

  T read() override {
    T result = 0;
    if (current_read_pos < current_write_pos) {
      result = buffer[current_read_pos++];
    }
    return result;
  }

  T peek() override {
    T result = 0;
    if (current_read_pos < current_write_pos) {
      result = buffer[current_read_pos];
    }
    return result;
  }

  int available() override {
    int result = current_write_pos - current_read_pos;
    return max(result, 0);
  }

  int availableForWrite() override { return max_size - current_write_pos; }

  bool isFull() override { return availableForWrite() <= 0; }

  /// consumes len bytes and moves current data to the beginning
  int clearArray(int len) override{
    int len_available = available();
    if (len>available()) {
      reset();
      return len_available;
    }
    current_read_pos += len;
    len_available -= len;
    memmove(buffer.data(), buffer.data()+current_read_pos, len_available);
    current_read_pos = 0;
    current_write_pos = len_available;

    if (is_clear_with_zero){
      memset(buffer.data()+current_write_pos,0,buffer.size()-current_write_pos);
    }

    return len;
  }

  /// Provides address to beginning of the buffer
  T *address() override { return buffer.data(); }

  /// Provides address of actual data
  T *data() { return buffer.data()+current_read_pos; }

  void reset() override {
    current_read_pos = 0;
    current_write_pos = 0;
    if (is_clear_with_zero){
      memset(buffer.data(),0,buffer.size());
    }
  }

  /// If we load values directly into the address we need to set the avialeble
  /// size
  size_t setAvailable(size_t available_size) {
    size_t result = min(available_size, (size_t) max_size);
    current_read_pos = 0;
    current_write_pos = result;
    return result;
  }


  size_t size() { return max_size; }

  void resize(int size) {
    if (buffer.size() != size) {
      TRACED();
      buffer.resize(size);
      max_size = size;
    }
  }

  /// Sets the buffer to 0 on clear
  void setClearWithZero(bool flag){
    is_clear_with_zero = flag;
  }

 protected:
  int max_size = 0;
  int current_read_pos = 0;
  int current_write_pos = 0;
  bool owns_buffer = true;
  bool is_clear_with_zero = false;
  Vector<T> buffer{0};

  void setWritePos(int pos) { current_write_pos = pos; }
};

/**
 * @brief Implements a typed Ringbuffer
 * @ingroup buffers
 * @tparam T
 */
template <typename T>
class RingBuffer : public BaseBuffer<T> {
 public:
  RingBuffer(int size) {
    resize(size);
    reset();
  }

  virtual T read() {
    if (isEmpty()) return -1;

    T value = _aucBuffer[_iTail];
    _iTail = nextIndex(_iTail);
    _numElems--;

    return value;
  }

  // peeks the actual entry from the buffer
  virtual T peek() {
    if (isEmpty()) return -1;

    return _aucBuffer[_iTail];
  }

  virtual int peekArray(T*data, int n){
    if (isEmpty()) return -1;
    int result = 0;
    int count = _numElems;
    int tail = _iTail;
    for (int j=0;j<n;j++){
      data[j] = _aucBuffer[tail];
      tail = nextIndex(tail);
      count--;
      result++;
      if (count==0)break;
    }
    return result;
  }

  // checks if the buffer is full
  virtual bool isFull() { return available() == max_size; }

  bool isEmpty() { return available() == 0; }

  // write add an entry to the buffer
  virtual bool write(T data) {
    bool result = false;
    if (!isFull()) {
      _aucBuffer[_iHead] = data;
      _iHead = nextIndex(_iHead);
      _numElems++;
      result = true;
    }
    return result;
  }

  // clears the buffer
  virtual void reset() {
    _iHead = 0;
    _iTail = 0;
    _numElems = 0;
  }

  // provides the number of entries that are available to read
  virtual int available() { return _numElems; }

  // provides the number of entries that are available to write
  virtual int availableForWrite() { return (max_size - _numElems); }

  // returns the address of the start of the physical read buffer
  virtual T *address() { return _aucBuffer.data(); }

  virtual void resize(int len) {
    if (max_size != len) {
      LOGI("resize: %d", len);
      _aucBuffer.resize(len);
      max_size = len;
    }
  }

  /// Returns the maximum capacity of the buffer
  virtual int size() { return max_size; }

 protected:
  Vector<T> _aucBuffer;
  int _iHead;
  int _iTail;
  int _numElems;
  int max_size = 0;

  int nextIndex(int index) { return (uint32_t)(index + 1) % max_size; }
};

/**
 * @brief An File backed Ring Buffer that we can use to receive
 * streaming audio. We expect an open p_file as parameter.
 *
 * If you want to keep the processed data, call setAutoRewind(false) and
 * call p_file->save() when you are done!
 * @ingroup buffers
 * @tparam File
 * @tparam T
 */
template <class File, typename T>
class RingBufferFile : public BaseBuffer<T> {
 public:
  RingBufferFile(bool autoRewind = true) { setAutoRewind(autoRewind); }
  RingBufferFile(File &file, bool autoRewind = true) {
    setFile(file);
    setAutoRewind(autoRewind);
  }

  /// if the full buffer has been consumed we restart from the 0 p_file position
  /// Set this value to false if you want to keep the full processed data in the
  /// p_file
  void setAutoRewind(bool flag) { auto_rewind = true; }

  /// Assigns the p_file to be used.
  void setFile(File &bufferFile, bool clear = false) {
    p_file = &bufferFile;
    if (!*p_file) {
      LOGE("file is not valid");
    }
    // if no clear has been requested we can access the existing data in the
    // p_file
    if (!clear) {
      element_count = p_file->size() / sizeof(T);
      LOGI("existing elements: %s", element_count);
      read_pos = element_count;
    }
  }

  T read() override {
    if (isEmpty()) return -1;

    T result = peek();
    read_pos++;
    element_count--;
    // the buffer is empty
    if (auto_rewind && isEmpty()) {
      LOGI("pos 0");
      write_pos = 0;
      read_pos = 0;
    }

    return result;
  }

  /// reads multiple values
  int readArray(T data[], int count) override {
    if (p_file == nullptr) return 0;
    int read_count = min(count, element_count);
    file_seek(read_pos);
    int elements_processed = file_read(data, read_count);
    read_pos += elements_processed;
    element_count -= elements_processed;
    return elements_processed;
  }

  // peeks the actual entry from the buffer
  T peek() override {
    if (p_file == nullptr) return 0;
    if (isEmpty()) return -1;

    file_seek(read_pos);
    T result;
    size_t count = file_read(&result, 1);
    return result;
  }

  // checks if the buffer is full
  bool isFull() override { return available() == max_size; }

  bool isEmpty() { return available() == 0; }

  // write add an entry to the buffer
  virtual bool write(T data) override { return writeArray(&data, 1); }

  /// Fills the data from the buffer
  int writeArray(const T data[], int len) override {
    if (p_file == nullptr) return 0;
    file_seek(write_pos);
    int elements_written = file_write(data, len);
    write_pos += elements_written;
    element_count += elements_written;
    return elements_written;
  }

  // clears the buffer
  void reset() override {
    write_pos = 0;
    read_pos = 0;
    element_count = 0;
    if (p_file != nullptr) file_seek(0);
  }

  // provides the number of entries that are available to read
  int available() override { return element_count; }

  // provides the number of entries that are available to write
  int availableForWrite() override { return (max_size - element_count); }

  // /// Returns the maximum capacity of the buffer
  // int size() override { return max_size; }

  // not supported
  T *address() override { return nullptr; }

 protected:
  File *p_file = nullptr;
  int write_pos;
  int read_pos;
  int element_count;
  int max_size = INT_MAX;
  bool auto_rewind = true;

  void file_seek(int pos) {
    if (p_file->position() != pos * sizeof(T)) {
      LOGD("file_seek: %d", pos);
      if (!p_file->seek(pos * sizeof(T))) {
        LOGE("seek %d", pos * sizeof(T))
      }
    }
  }

  int file_write(const T *data, int count) {
    LOGD("file_write: %d", count);
    if (p_file == nullptr) return 0;
    int to_write = sizeof(T) * count;
    int bytes_written = p_file->write((const uint8_t *)data, to_write);
    // p_file->flush();
    int elements_written = bytes_written / sizeof(T);
    if (bytes_written != to_write) {
      LOGE("write: %d -> %d", to_write, bytes_written);
    }
    return elements_written;
  }

  int file_read(T *result, int count) {
    LOGD("file_read: %d", count);
    size_t result_bytes = p_file->read((uint8_t *)result, sizeof(T) * count);
    if (result_bytes != count * sizeof(T)) {
      LOGE("readBytes: %d -> %d", (int)sizeof(T) * count, (int)result_bytes);
      result = 0;
    }
    return count;
  }
};

/**
 * @brief A lock free N buffer. If count=2 we create a DoubleBuffer, if
 * count=3 a TripleBuffer etc.
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename T>
class NBuffer : public BaseBuffer<T> {
 public:
  NBuffer(int size, int count) {
    resize(size, count);
  }

  virtual ~NBuffer() {
    freeMemory();
  }

  // reads an entry from the buffer
  T read() {
    T result = 0;
    if (available() > 0) {
      result = actual_read_buffer->read();
    }
    return result;
  }

  // peeks the actual entry from the buffer
  T peek() {
    T result = 0;
    if (available() > 0) {
      result = actual_read_buffer->peek();
    }
    return result;
  }

  // checks if the buffer is full
  bool isFull() { return availableForWrite() == 0; }

  // write add an entry to the buffer
  bool write(T data) {
    bool result = false;
    if (actual_write_buffer == nullptr) {
      actual_write_buffer = getNextAvailableBuffer();
    }
    if (actual_write_buffer != nullptr) {
      result = actual_write_buffer->write(data);
      // if buffer is full move to next available
      if (actual_write_buffer->isFull()) {
        addFilledBuffer(actual_write_buffer);
        actual_write_buffer = getNextAvailableBuffer();
      }
    } else {
      // Logger.debug("actual_write_buffer is full");
    }

    if (start_time == 0l) {
      start_time = millis();
    }
    sample_count++;

    return result;
  }

  // determines the available entries for the current read buffer
  int available() {
    if (actual_read_buffer == nullptr) {
      actual_read_buffer = getNextFilledBuffer();
    }
    if (actual_read_buffer == nullptr) {
      return 0;
    }
    int result = actual_read_buffer->available();
    if (result == 0) {
      // make current read buffer available again
      resetCurrent();
      result =
          actual_read_buffer == nullptr ? 0 : actual_read_buffer->available();
    }
    return result;
  }

  // deterMINes the available entries for the write buffer
  int availableForWrite() {
    if (actual_write_buffer == nullptr) {
      actual_write_buffer = getNextAvailableBuffer();
    }
    // if we used up all buffers - there is nothing available any more
    if (actual_write_buffer == nullptr) {
      return 0;
    }
    // check on actual buffer
    if (actual_write_buffer->isFull()) {
      // if buffer is full we move it to filled buffers ang get the next
      // available
      addFilledBuffer(actual_write_buffer);
      actual_write_buffer = getNextAvailableBuffer();
    }
    return actual_write_buffer->availableForWrite();
  }

  // resets all buffers
  void reset() {
    TRACED();
    while (actual_read_buffer != nullptr) {
      actual_read_buffer->reset();
      addAvailableBuffer(actual_read_buffer);
      // get next read buffer
      actual_read_buffer = getNextFilledBuffer();
    }
  }

  // provides the actual sample rate
  unsigned long sampleRate() {
    unsigned long run_time = (millis() - start_time);
    return run_time == 0 ? 0 : sample_count * 1000 / run_time;
  }

  // returns the address of the start of the phsical read buffer
  T *address() {
    return actual_read_buffer == nullptr ? nullptr
                                         : actual_read_buffer->address();
  }

  // Alternative interface using address: the current buffer has been filled
  BaseBuffer<T> &writeEnd() {
    if (actual_write_buffer != nullptr) {
      actual_write_buffer->setWritePos(buffer_size);
      addFilledBuffer(actual_write_buffer);
    }
    actual_write_buffer = getNextAvailableBuffer();
    return *actual_write_buffer;
  }

  // Alternative interface using address: marks actual buffer as processed and
  // provides access to next read buffer
  BaseBuffer<T> &readEnd() {
    // make current read buffer available again
    resetCurrent();
    return *actual_read_buffer;
  }

  int bufferCountFilled() {
    int result = 0;
    for (int j = 0; j < buffer_count; j++) {
      if (filled_buffers[j] != nullptr) {
        result++;
      }
    }
    return result;
  }

  int bufferCountEmpty() {
    int result = 0;
    for (int j = 0; j < buffer_count; j++) {
      if (avaliable_buffers[j] != nullptr) {
        result++;
      }
    }
    return result;
  }

  void resize(int size, int count) { 
    if (buffer_size==size && buffer_count == count)
      return;
    freeMemory();
    filled_buffers.resize(count);
    avaliable_buffers.resize(count);

    write_buffer_count = 0;
    buffer_count = count;
    buffer_size = size;
    for (int j = 0; j < count; j++) {
      avaliable_buffers[j] = new SingleBuffer<T>(size);
      if (avaliable_buffers[j] == nullptr) {
        LOGE("Not Enough Memory for buffer %d", j);
      }
    }
  }

 protected:
  int buffer_size = 0;
  uint16_t buffer_count = 0;
  uint16_t write_buffer_count = 0;
  BaseBuffer<T> *actual_read_buffer = nullptr;
  BaseBuffer<T> *actual_write_buffer = nullptr;
  Vector<BaseBuffer<T> *> avaliable_buffers;
  Vector<BaseBuffer<T> *> filled_buffers;
  unsigned long start_time = 0;
  unsigned long sample_count = 0;

  // empty constructor only allowed by subclass
  NBuffer() = default;

  void freeMemory()  {
    delete actual_write_buffer;
    actual_write_buffer = nullptr;
    delete actual_read_buffer;
    actual_read_buffer = nullptr;

    BaseBuffer<T> *ptr = getNextAvailableBuffer();
    while (ptr != nullptr) {
      delete ptr;
      ptr = getNextAvailableBuffer();
    }

    ptr = getNextFilledBuffer();
    while (ptr != nullptr) {
      delete ptr;
      ptr = getNextFilledBuffer();
    }
  }

  void resetCurrent() {
    if (actual_read_buffer != nullptr) {
      actual_read_buffer->reset();
      addAvailableBuffer(actual_read_buffer);
    }
    // get next read buffer
    actual_read_buffer = getNextFilledBuffer();
  }

  virtual BaseBuffer<T> *getNextAvailableBuffer() {
    BaseBuffer<T> *result = nullptr;
    for (int j = 0; j < buffer_count; j++) {
      result = avaliable_buffers[j];
      if (result != nullptr) {
        avaliable_buffers[j] = nullptr;
        break;
      }
    }
    return result;
  }

  virtual bool addAvailableBuffer(BaseBuffer<T> *buffer) {
    bool result = false;
    for (int j = 0; j < buffer_count; j++) {
      if (avaliable_buffers[j] == nullptr) {
        avaliable_buffers[j] = buffer;
        result = true;
        break;
      }
    }
    return result;
  }

  virtual BaseBuffer<T> *getNextFilledBuffer() {
    BaseBuffer<T> *result = nullptr;
    if (write_buffer_count > 0) {
      // get oldest entry
      result = filled_buffers[0];
      // move data by 1 entry to the left
      for (int j = 0; j < write_buffer_count - 1; j++)
        filled_buffers[j] = filled_buffers[j + 1];
      // clear last enty
      filled_buffers[write_buffer_count - 1] = nullptr;
      write_buffer_count--;
    }
    return result;
  }

  virtual bool addFilledBuffer(BaseBuffer<T> *buffer) {
    bool result = false;
    if (write_buffer_count < buffer_count) {
      filled_buffers[write_buffer_count++] = buffer;
      result = true;
    }
    return result;
  }
};

/**
 * @brief Class which is usfull ot provide incremental data access e.g. for
 * EdgeImpulse which request data with an offset and length starting from 0 up
 * to the buffer length, restarting at 0 again.
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <typename T>
class BufferedArray {
 public:
  BufferedArray(Stream &input, int len) {
    LOGI("BufferedArray(%d)", len);
    array.resize(len);
    p_stream = &input;
  }
  // access values, the offset and length are specified in samples of type <T>
  int16_t *getValues(size_t offset, size_t length) {
    LOGD("getValues(%d,%d) - max %d", offset, length, array.size());
    if (offset == 0) {
      // we restart at the beginning
      last_end = 0;
      actual_end = length;
    } else {
      // if first position is at end we do not want to read the full buffer
      last_end = actual_end >= 0 ? actual_end : offset;
      // increase actual end if bigger then old
      actual_end = offset + length > actual_end ? offset + length : actual_end;
    }
    int size = actual_end - last_end;
    if (size > 0) {
      LOGD("readBytes(%d,%d)", last_end, size);
      assert(last_end + size <= array.size());
      p_stream->readBytes((uint8_t *)(&array[last_end]), size * 2);
    }
    assert(offset < actual_end);
    return &array[offset];
  }

 protected:
  int actual_end = -1;
  int last_end = 0;
  Vector<T> array;
  Stream *p_stream = nullptr;
};

}  // namespace audio_tools
