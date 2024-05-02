#pragma once
#include "AudioTools/Buffers.h"

#ifdef ARDUINO
#include "Stream.h"
#endif

#ifdef USE_STREAM_WRITE_OVERRIDE
#define STREAM_WRITE_OVERRIDE override
#else
#define STREAM_WRITE_OVERRIDE
#endif

#ifdef USE_STREAM_READ_OVERRIDE
#define STREAM_READ_OVERRIDE override
#else
#define STREAM_READ_OVERRIDE
#endif

#ifdef USE_STREAM_READCHAR_OVERRIDE
#define STREAM_READCHAR_OVERRIDE override
#else
#define STREAM_READCHAR_OVERRIDE
#endif

namespace audio_tools {

/**
 * @brief Base class for all Streams. It relies on write(const uint8_t *buffer,
 * size_t size) and readBytes(uint8_t *buffer, size_t length).
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BaseStream : public Stream {
 public:
  BaseStream() = default;
  virtual ~BaseStream() = default;
  BaseStream(BaseStream const &) = delete;
  BaseStream &operator=(BaseStream const &) = delete;

  virtual bool begin(){return true;}
  virtual void end(){}

  virtual size_t readBytes(uint8_t *buffer,
                           size_t length) STREAM_READ_OVERRIDE = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) override = 0;

  virtual size_t write(uint8_t ch) override {
    tmp_out.resize(MAX_SINGLE_CHARS);
    if (tmp_out.isFull()) {
      flush();
    }
    return tmp_out.write(ch);
  }

  virtual int available() override { return DEFAULT_BUFFER_SIZE; };

  virtual int availableForWrite() override { return DEFAULT_BUFFER_SIZE; }

  virtual void flush() override {
    if (tmp_out.available() > 0) {
      write((const uint8_t *)tmp_out.address(), tmp_out.available());
    }
  }

// Methods which should be suppressed in the documentation
#ifndef DOXYGEN

  virtual size_t readBytes(char *buffer,
                           size_t length) STREAM_READCHAR_OVERRIDE {
    return readBytes((uint8_t *)buffer, length);
  }

  virtual int read() override {
    refillReadBuffer();
    return tmp_in.read();
  }

  virtual int peek() override {
    refillReadBuffer();
    return tmp_in.peek();
  }

#endif

 protected:
  RingBuffer<uint8_t> tmp_in{0};
  RingBuffer<uint8_t> tmp_out{0};

  void refillReadBuffer() {
    tmp_in.resize(MAX_SINGLE_CHARS);
    if (tmp_in.isEmpty()) {
      TRACED();
      const int len = tmp_in.size();
      uint8_t bytes[len];
      int len_eff = readBytes(bytes, len);
      // LOGD("tmp_in available: %d / size: %d / to be written %d",
      // tmp_in.available(), tmp_in.size(), len_eff);
      tmp_in.writeArray(bytes, len_eff);
    }
  }
};

/**
 * @brief Provides data from a concatenation of Streams. Please note that the
 * provided Streams can be played only once! You will need to reset them (e.g.
 * moving the file pointer to the beginning) and readd them back if you want to
 * process them a second time. The default timeout on the available() method is
 * set to 0. This might be not good if you use e.g. a URLStream.
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CatStream : public BaseStream {
 public:
  CatStream() = default;

  void add(Stream *stream) { input_streams.push_back(stream); }
  void add(Stream &stream) { input_streams.push_back(&stream); }

  bool begin() {
    is_active = true;
    return true;
  }

  void end() { is_active = false; }

  int available() override {
    if (!is_active) return 0;
    if (!moveToNextStreamOnEnd()) {
      return 0;
    }
    return availableWithTimout();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    if (!is_active) return 0;
    if (!moveToNextStreamOnEnd()) {
      return 0;
    }
    return p_current_stream->readBytes(data, len);
  }

  /// Returns true if active and we still have data
  operator bool() { return is_active && available() > 0; }

  void setOnBeginCallback(void (*callback)(Stream *stream)) {
    begin_callback = callback;
  }
  void setOnEndCallback(void (*callback)(Stream *stream)) {
    end_callback = callback;
  }
  void setTimeout(uint32_t t) { _timeout = t; }

 protected:
  Vector<Stream *> input_streams;
  Stream *p_current_stream = nullptr;
  bool is_active = false;
  void (*begin_callback)(Stream *stream) = nullptr;
  void (*end_callback)(Stream *stream) = nullptr;
  uint_fast32_t _timeout = 0;

  /// moves to the next stream if necessary: returns true if we still have a
  /// valid stream
  bool moveToNextStreamOnEnd() {
    // keep on running
    if (p_current_stream != nullptr && p_current_stream->available() > 0)
      return true;
    // at end?
    if ((p_current_stream == nullptr || availableWithTimout() == 0)) {
      if (end_callback && p_current_stream) end_callback(p_current_stream);
      if (!input_streams.empty()) {
        LOGI("using next stream");
        p_current_stream = input_streams[0];
        input_streams.pop_front();
        if (begin_callback && p_current_stream)
          begin_callback(p_current_stream);
      } else {
        p_current_stream = nullptr;
      }
    }
    // returns true if we have a valid stream
    return p_current_stream != nullptr;
  }

  int availableWithTimout() {
    int result = p_current_stream->available();
    if (result == 0) {
      for (int j = 0; j < _timeout / 10; j++) {
        delay(10);
        result = p_current_stream->available();
        if (result != 0) break;
      }
    }
    return result;
  }
};

/**
 * @brief The Arduino Stream which provides silence and simulates a null device
 * when used as audio target or audio source
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NullStream : public BaseStream {
 public:
  size_t write(const uint8_t *buffer, size_t len) override { return len; }

  size_t readBytes(uint8_t *buffer, size_t len) override {
    memset(buffer, 0, len);
    return len;
  }
};


/**
 * @brief Stream class which stores the data in a temporary queue buffer.
 * The queue can be consumed e.g. by a callback function by calling readBytes();
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T>
class QueueStream : public BaseStream {
 public:
  /// Default constructor
  QueueStream(int bufferSize, int bufferCount,
              bool autoRemoveOldestDataIfFull = false) {
    owns_buffer = true;
    callback_buffer_ptr = new NBuffer<T>(bufferSize, bufferCount);
    remove_oldest_data = autoRemoveOldestDataIfFull;
  }
  /// Create stream from any BaseBuffer subclass
  QueueStream(BaseBuffer<T> &buffer) {
    owns_buffer = false;
    callback_buffer_ptr = &buffer;
  }

  virtual ~QueueStream() {
    if (owns_buffer) {
      delete callback_buffer_ptr;
    }
  }

  /// Activates the output
  virtual bool begin() {
    TRACED();
    active = true;
    return true;
  }

  /// Activate only when filled buffer reached %
  virtual bool begin(size_t activeWhenPercentFilled) {
    // determine total buffer size in bytes
    size_t size = callback_buffer_ptr->size() * sizeof(T);
    // calculate limit
    active_limit = size * activeWhenPercentFilled / 100;
    return true;
  }

  /// stops the processing
  virtual void end() {
    TRACED();
    active = false;
  };

  int available() override {
    return active ? callback_buffer_ptr->available() * sizeof(T) : 0;
  }

  int availableForWrite() override {
    return callback_buffer_ptr->availableForWrite() * sizeof(T);
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    if (active_limit == 0 && !active) return 0;

    // activate automaticaly when limit has been reached
    if (active_limit > 0 && !active && available() >= active_limit) {
      this->active = true;
    }

    // make space by deleting oldest entries
    if (remove_oldest_data) {
      int available_bytes =
          callback_buffer_ptr->availableForWrite() * sizeof(T);
      if ((int)len > available_bytes) {
        int gap = len - available_bytes;
        uint8_t tmp[gap];
        readBytes(tmp, gap);
      }
    }

    return callback_buffer_ptr->writeArray(data, len / sizeof(T));
  }

  virtual size_t readBytes(uint8_t *data, size_t len) override {
    if (!active) return 0;
    return callback_buffer_ptr->readArray(data, len / sizeof(T));
  }

  /// Clears the data in the buffer
  void clear() {
    if (active) {
      callback_buffer_ptr->reset();
    }
  }

  /// Returns true if active
  operator bool() { return active; }

 protected:
  BaseBuffer<T> *callback_buffer_ptr;
  size_t active_limit = 0;
  bool active;
  bool remove_oldest_data;
  bool owns_buffer;
};

// support legacy name
template <typename T>
using CallbackBufferedStream = QueueStream<T>;

/**
 * @brief MemoryStream which is written and read using the internal RAM. For each write the data is allocated
 * on the heap.
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DynamicMemoryStream : public BaseStream {
public:
  struct DataNode {
    size_t len=0;
    uint8_t* data=nullptr;

    DataNode() = default;
    /// Constructor
    DataNode(void*inData, int len){
      this->len = len;
      this->data = (uint8_t*) malloc(len);
      assert(this->data!=nullptr);
      memcpy(this->data, inData, len);
    }

    ~DataNode(){
      if (data!=nullptr) {
         free(data);
         data = nullptr;
      }
    }
  };

  DynamicMemoryStream() = default;

  DynamicMemoryStream(bool isLoop, int defaultBufferSize=DEFAULT_BUFFER_SIZE ) {
    this->default_buffer_size = defaultBufferSize;
    is_loop = isLoop;
  }
  // Assign values from ref, clearing the original ref
  void assign(DynamicMemoryStream &ref){
    audio_list.swap(ref.audio_list);
    it = ref.it;
    total_available=ref.total_available;
    default_buffer_size = ref.default_buffer_size;
    alloc_failed = ref.alloc_failed;;
    is_loop = ref.is_loop;
    ref.clear();
  }

  /// Intializes the processing
  virtual bool begin()  {
    clear();
    temp_audio.resize(default_buffer_size);
    return true;
  }

  virtual void end()  {
    clear();
  }

  /// Automatically rewinds to the beginning when reaching the end
  virtual void setLoop(bool loop){
    is_loop = loop;
  }

  void clear() {
    DataNode *p_node;
    bool ok;
    do{
        ok = audio_list.pop_front(p_node);
        if (ok){
          delete p_node;
        }
    } while (ok);

    temp_audio.reset();
    total_available = 0;
    alloc_failed = false;
    rewind();
  }

  size_t size(){
    return total_available;
  }

  /// Sets the read position to the beginning
  void rewind() {
    it = audio_list.begin();
  }

  virtual size_t write(const uint8_t *buffer, size_t size) override {
    DataNode *p_node = new DataNode((void*)buffer, size);
    if (p_node->data!=nullptr){
      alloc_failed = false;
      total_available += size;
      audio_list.push_back(p_node);

      // setup interator to point to first record
      if (it == audio_list.end()){
        it = audio_list.begin();
      }

      return size;
    } 
    alloc_failed = true;
    return 0;
  }

  virtual int availableForWrite() override {
    return alloc_failed ? 0 : default_buffer_size;
  } 

  virtual int available() override {
    if (it == audio_list.end()){
      if (is_loop) rewind();
      if (it == audio_list.end()) {
        return 0;
      }
    }
    return (*it)->len;
  }

  virtual size_t readBytes(uint8_t *buffer, size_t length) override {
    // provide unprocessed data
    if (temp_audio.available()>0){
      return temp_audio.readArray(buffer, length);
    }

    // We have no more data
    if (it==audio_list.end()){
      if (is_loop){
        rewind();
      } else {
        // stop the processing
        return 0;
      }
    }

    // provide data from next node
    DataNode *p_node = *it;
    int result_len = min(length, (size_t) p_node->len);
    memcpy(buffer, p_node->data, result_len);
    // save unprocessed data to temp buffer
    if (p_node->len>length){
      uint8_t *start = p_node->data+result_len;
      int uprocessed_len = p_node->len - length; 
      temp_audio.writeArray(start, uprocessed_len);
    }
    //move to next pos
    ++it;
    return result_len;
  }

  List<DataNode*> &list() {
    return audio_list;
  }

  /// @brief  Post processing after the recording. We add a smooth transition at the beginning and at the end
  /// @tparam T 
  /// @param factor 
  template<typename T>
  void postProcessSmoothTransition(int channels, float factor = 0.01, int remove=0){
      if (remove>0){
        for (int j=0;j<remove;j++){
          DataNode* node = nullptr;
          audio_list.pop_front(node);
          if (node!=nullptr) delete node;
          node = nullptr;
          audio_list.pop_back(node);
          if (node!=nullptr) delete node;
        }
      }

      // Remove popping noise
      SmoothTransition<T> clean_start(channels, true, false, factor);
      auto first = *list().begin();   
      if (first!=nullptr){ 
        clean_start.convert(first->data,first->len);
      }

      SmoothTransition<T> clean_end(channels, false, true, factor);
      auto last = * (--(list().end()));
      if (last!=nullptr){
        clean_end.convert(last->data,last->len);  
      }  
  }


protected:
  List<DataNode*> audio_list;
  List<DataNode*>::Iterator it = audio_list.end();
  size_t total_available=0;
  int default_buffer_size=DEFAULT_BUFFER_SIZE;
  bool alloc_failed = false;
  RingBuffer<uint8_t> temp_audio{DEFAULT_BUFFER_SIZE};
  bool is_loop = false;

};



}  // namespace audio_tools