#pragma once
#include "AudioConfig.h"
#include "AudioTimer/AudioTimer.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/AudioLogger.h"
#include "AudioEffects/SoundGenerator.h"

#ifndef URL_CLIENT_TIMEOUT
#  define URL_CLIENT_TIMEOUT 60000
#endif

#ifndef URL_HANDSHAKE_TIMEOUT
#  define URL_HANDSHAKE_TIMEOUT 120000
#endif

#ifndef IRAM_ATTR
#  define IRAM_ATTR
#endif

#ifdef USE_STREAM_WRITE_OVERRIDE
#  define STREAM_WRITE_OVERRIDE override
#else
#  define STREAM_WRITE_OVERRIDE 
#endif

#ifdef USE_STREAM_READ_OVERRIDE
#  define STREAM_READ_OVERRIDE override
#else
#  define STREAM_READ_OVERRIDE 
#endif

#ifdef USE_STREAM_READCHAR_OVERRIDE
#  define STREAM_READCHAR_OVERRIDE override
#else
#  define STREAM_READCHAR_OVERRIDE 
#endif

namespace audio_tools {

/**
 * @brief Base class for all Audio Streams. It support the boolean operator to
 * test if the object is ready with data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioStream : public Stream, public AudioInfoSupport, public AudioInfoSource {
 public:
  AudioStream() = default;
  virtual ~AudioStream() = default;
  AudioStream(AudioStream const&) = delete;
  AudioStream& operator=(AudioStream const&) = delete;

  virtual bool begin(){return true;}
  virtual void end(){}
  
  // Call from subclass or overwrite to do something useful
  virtual void setAudioInfo(AudioInfo info) override {
      TRACED();
      this->info = info;
      info.logInfo();
      if (p_notify!=nullptr){
          p_notify->setAudioInfo(info);
      }
  }

  virtual void  setNotifyAudioChange(AudioInfoSupport &bi) override {
      p_notify = &bi;
  }

  virtual size_t readBytes(uint8_t *buffer, size_t length) STREAM_READ_OVERRIDE { return not_supported(0, "readBytes"); }

  virtual size_t write(const uint8_t *buffer, size_t size) override{ return not_supported(0,"write"); }

  virtual size_t write(uint8_t ch) override {
      tmp_out.resize(MAX_SINGLE_CHARS);
      if (tmp_out.isFull()){
          flush();
      }
      return tmp_out.write(ch);
  }

  virtual int available() override { return DEFAULT_BUFFER_SIZE; };


  operator bool() { return available() > 0; }

  virtual AudioInfo audioInfo() override {
    return info;
  }

  virtual int availableForWrite() override { return DEFAULT_BUFFER_SIZE; }

  virtual void flush() override {
      if (tmp_out.available()>0){
          write((const uint8_t*)tmp_out.address(), tmp_out.available());
      }
  }

  /// Writes len bytes of silence (=0).
  virtual void writeSilence(size_t len){
    int16_t zero = 0;
    for (int j=0;j<len/2;j++){
      write((uint8_t*)&zero,2);
    } 
  }

  /// Source to generate silence: just sets the buffer to 0
  virtual size_t readSilence(uint8_t *buffer, size_t length)  { 
    memset(buffer, 0, length);
    return length;
  }
  

// Methods which should be suppressed in the documentation
#ifndef DOXYGEN

  virtual size_t readBytes(char *buffer, size_t length) STREAM_READCHAR_OVERRIDE {
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
  AudioInfoSupport *p_notify=nullptr;
  AudioInfo info;
  RingBuffer<uint8_t> tmp_in{0};
  RingBuffer<uint8_t> tmp_out{0};


  virtual int not_supported(int out, const char* msg="") {
    LOGE("AudioStream: %s unsupported operation!", msg);
    // trigger stacktrace
    //assert(false);
    return out;
  }

  void refillReadBuffer() {
    tmp_in.resize(MAX_SINGLE_CHARS);
    if (tmp_in.isEmpty()){
      TRACED();
      const int len = tmp_in.size();
      uint8_t bytes[len];
      int len_eff = readBytes(bytes, len);
      //LOGD("tmp_in available: %d / size: %d / to be written %d", tmp_in.available(), tmp_in.size(), len_eff);
      tmp_in.writeArray(bytes,len_eff);
    }
  }

};

/**
 * @brief To be used to support implementations where the readBytes is not
 * virtual
 *
 */
class AudioStreamWrapper : public AudioStream {
 public:
     AudioStreamWrapper(Stream& s) { 
         TRACED();
         p_stream = &s; 
         p_stream->setTimeout(clientTimeout);
     }

  virtual bool begin(){return true;}
  virtual void end(){}

  virtual size_t readBytes(uint8_t *buffer, size_t length) {
      //Serial.print("Timeout audiostream: ");
      //Serial.println(p_stream->getTimeout());
    return p_stream->readBytes(buffer, length);
  }

  int read() { return p_stream->read(); }

  int peek() { return p_stream->peek(); }

  int available() { return p_stream->available(); }

  virtual size_t write(uint8_t c) { return p_stream->write(c); }

  virtual size_t write(const uint8_t *buffer, size_t size) {
    return p_stream->write(buffer, size);
  }

  virtual int availableForWrite() { return p_stream->availableForWrite(); }

  virtual void flush() { p_stream->flush(); }

 protected:
  Stream *p_stream;
  int32_t clientTimeout = URL_CLIENT_TIMEOUT; // 60000;
};


/**
 * @brief A simple Stream implementation which is backed by allocated memory
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class MemoryStream : public AudioStream {
 public:
  MemoryStream(int buffer_size = 512, MemoryType memoryType = RAM) {
    LOGD("MemoryStream: %d", buffer_size);
    this->buffer_size = buffer_size;
    this->memory_type = memoryType;
    resize(buffer_size);
  }

  MemoryStream(const uint8_t *buffer, int buffer_size, MemoryType memoryType = FLASH_RAM) {
    LOGD("MemoryStream: %d", buffer_size);
    setValue(buffer, buffer_size, memoryType);
  }

  ~MemoryStream() {
    TRACED();
    if (memoryCanChange() && buffer!=nullptr) free(buffer);
  }

  // resets the read pointer
  bool begin() override {
    TRACED();
    write_pos = memoryCanChange() ? 0 : buffer_size;
    if (this->buffer==nullptr && memoryCanChange()){
      resize(buffer_size);
    }
    read_pos = 0;
    return true;
  }

  virtual size_t write(uint8_t byte) override {
    if (buffer==nullptr) return 0;
    int result = 0;
    if (write_pos < buffer_size) {
      result = 1;
      buffer[write_pos] = byte;
      write_pos++;
    }
    return result;
  }

  virtual size_t write(const uint8_t *buffer, size_t size) override {
    size_t result = 0;
    for (size_t j = 0; j < size; j++) {
      if (!write(buffer[j])) {
        break;
      }
      result = j;
    }
    return result;
  }

  virtual int available() override { 
    if (buffer==nullptr) return 0;
    int result = write_pos - read_pos;
    if (result<=0 && is_loop){
      // rewind to start
      read_pos = 0;
      result = write_pos - read_pos;
      // call callback
      if (rewind!=nullptr) rewind();
    }
    return result;
  }

  virtual int availableForWrite() override {
    return buffer_size - write_pos;
  } 

  virtual int read() override {
    int result = peek();
    if (result >= 0) {
      read_pos++;
    }
    return result;
  }

  virtual size_t readBytes(uint8_t *buffer, size_t length) override {
    size_t count = 0;
    while (count < length) {
      int c = read();
      if (c < 0) break;
      *buffer++ = (char)c;
      count++;
    }
    return count;
  }

  virtual int peek() override {
    int result = -1;
    if (available() > 0) {
      result = buffer[read_pos];
    }
    return result;
  }

  virtual void flush() override {}

  virtual void end() override {
    read_pos = 0;
  }

  /// clears the audio data: sets all values to 0
  virtual void clear(bool reset = false) {
    if (memoryCanChange()){
      write_pos = 0;
      read_pos = 0;
      if (buffer==nullptr){
        resize(buffer_size);
      }
      if (reset) {
        // we clear the buffer data
        memset(buffer, 0, buffer_size);
      }
    } else {
      read_pos = 0;
      LOGW("data is read only");
    }
  }

  /// Automatically rewinds to the beginning when reaching the end
  virtual void setLoop(bool loop){
    is_loop = loop;
  }

  virtual void resize(size_t size){
    if (memoryCanChange()){      
      buffer_size = size;
      switch(memory_type){
  #if defined(ESP32) && defined(ARDUINO) 
        case PS_RAM:
          buffer = (buffer==nullptr) ? (uint8_t*)ps_calloc(size,1) : (uint8_t*)ps_realloc(buffer, size);
          assert(buffer!=nullptr);
          break;
  #endif
        default:
          buffer = (buffer==nullptr) ? (uint8_t*)calloc(size,1) : (uint8_t*)realloc(buffer, size);
          assert(buffer!=nullptr);
          break;
      }
    }
  }

  virtual uint8_t* data(){
    return buffer;
  }

  /// Callback which is executed when we rewind (in loop mode) to the beginning
  void setRewindCallback(void (*cb)()){
    this->rewind = cb;
  }

  /// Update the values  (buffer and size)
  void setValue(const uint8_t *buffer, int buffer_size, MemoryType memoryType = FLASH_RAM) {
    this->buffer_size = buffer_size;
    this->read_pos = 0;
    this->write_pos = buffer_size;
    this->buffer = (uint8_t *)buffer;
    this->memory_type = memoryType;
  }

 protected:
  int write_pos = 0;
  int read_pos = 0;
  int buffer_size = 0;
  uint8_t *buffer = nullptr;
  MemoryType memory_type = RAM;
  bool is_loop = false;
  void (*rewind)() = nullptr;

  bool memoryCanChange() {
    return memory_type!=FLASH_RAM;
  }
};

/**
 * @brief MemoryStream which is written and read using the internal RAM. For each write the data is allocated
 * on the heap.
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DynamicMemoryStream : public AudioStream {
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
  virtual bool begin() override {
    clear();
    temp_audio.resize(default_buffer_size);
    return true;
  }

  virtual void end() override {
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

/**
 * @brief Source for reading generated tones. Please note
 * - that the output is for one channel only!
 * - we do not support reading of individual characters!
 * - we do not support any write operations
 * @ingroup io
 * @param generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class T>
class GeneratedSoundStream : public AudioStream {
 public:
  GeneratedSoundStream() = default;
  
  GeneratedSoundStream(SoundGenerator<T> &generator) {
    TRACED();
    setInput(generator);
  }

  void setInput(SoundGenerator<T> &generator){
    this->generator_ptr = &generator;
  }

  AudioInfo defaultConfig() { return this->generator_ptr->defaultConfig(); }

  /// start the processing
  bool begin() override {
    TRACED();
    if (generator_ptr==nullptr){
      LOGE("%s",source_not_defined_error);
      return false;
    }
    generator_ptr->begin();
    if (audioBaseInfoDependent != nullptr)
      audioBaseInfoDependent->setAudioInfo(generator_ptr->audioInfo());
    active = true;
    return active;
  }

  /// start the processing
  bool begin(AudioInfo cfg) {
    TRACED();
    if (generator_ptr==nullptr){
      LOGE("%s",source_not_defined_error);
      return false;
    }
    generator_ptr->begin(cfg);
    if (audioBaseInfoDependent != nullptr)
      audioBaseInfoDependent->setAudioInfo(generator_ptr->audioInfo());
    active = true;
    return active;
  }

  /// stop the processing
  void end() override {
    TRACED();
    generator_ptr->end();
    active = false;
  }

  virtual void setNotifyAudioChange(AudioInfoSupport &bi) override {
    audioBaseInfoDependent = &bi;
  }

  AudioInfo audioInfo() override {
    return generator_ptr->audioInfo();
  }

  /// This is unbounded so we just return the buffer size
  virtual int available() override { return DEFAULT_BUFFER_SIZE*2; }

  /// privide the data as byte stream
  size_t readBytes(uint8_t *buffer, size_t length) override {
    LOGD("GeneratedSoundStream::readBytes: %u", (unsigned int)length);
    return generator_ptr->readBytes(buffer, length);
  }

  bool isActive() {return active && generator_ptr->isActive();}

  operator bool() { return isActive(); }

  void flush() override {}

 protected:
  bool active = false;
  SoundGenerator<T> *generator_ptr;
  AudioInfoSupport *audioBaseInfoDependent = nullptr;
  const char* source_not_defined_error = "Source not defined";

};

/**
 * @brief The Arduino Stream supports operations on single characters. This is
 * usually not the best way to push audio information, but we will support it
 * anyway - by using a buffer. On reads: if the buffer is empty it gets refilled
 * - for writes if it is full it gets flushed.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BufferedStream : public AudioStream {
 public:
  BufferedStream(size_t buffer_size) {
    TRACED();
    buffer = new SingleBuffer<uint8_t>(buffer_size);
  }

  ~BufferedStream() {
    TRACED();
    if (buffer != nullptr) {
      delete buffer;
    }
  }

  /// writes a byte to the buffer
  virtual size_t write(uint8_t c) override {
    if (buffer->isFull()) {
      flush();
    }
    return buffer->write(c);
  }

  /// Use this method: write an array
  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("%s: %zu", LOG_METHOD, len);
    flush();
    return writeExt(data, len);
  }

  /// empties the buffer
  virtual void flush() override                                            {
    // just dump the memory of the buffer and clear it
    if (buffer->available() > 0) {
      writeExt(buffer->address(), buffer->available());
      buffer->reset();
    }
  }

  /// reads a byte - to be avoided
  virtual int read() override {
    if (buffer->isEmpty()) {
      refill();
    }
    return buffer->read();
  }

  /// peeks a byte - to be avoided
  virtual int peek() override{
    if (buffer->isEmpty()) {
      refill();
    }
    return buffer->peek();
  };

  /// Use this method !!
  size_t readBytes(uint8_t *data, size_t length) override {
    if (buffer->isEmpty()) {
      return readExt(data, length);
    } else {
      return buffer->readArray(data, length);
    }
  }

  /// Returns the available bytes in the buffer: to be avoided
  virtual int available() override {
    if (buffer->isEmpty()) {
      refill();
    }
    return buffer->available();
  }

  /// Clears all the data in the buffer
  void clear() { buffer->reset(); }

 protected:
  SingleBuffer<uint8_t> *buffer = nullptr;

  // refills the buffer with data from i2s
  void refill() {
    size_t result = readExt(buffer->address(), buffer->size());
    buffer->setAvailable(result);
  }

  virtual size_t writeExt(const uint8_t *data, size_t len) = 0;
  virtual size_t readExt(uint8_t *data, size_t length) = 0;
};

/**
 * @brief The Arduino Stream which provides silence and simulates a null device
 * when used as audio target or audio source
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NullStream : public AudioStream {
 public:

  bool begin(AudioInfo info) {    
    this->info = info;
    return true;
  }

  AudioInfo defaultConfig() {
    AudioInfo info;
    return info;
  }

  size_t write(const uint8_t *buffer, size_t len) override{
    return len;
  }

  size_t readBytes(uint8_t *buffer, size_t len) override{
    memset(buffer,0, len);
    return len;
  }

  /// Define object which need to be notified if the basinfo is changing
  void setNotifyAudioChange(AudioInfoSupport &bi) override {}

  void setAudioInfo(AudioInfo info) override {
    this->info = info;
  }
};

/**
 * @brief A Stream backed by a Ringbuffer. We can write to the end and read from
 * the beginning of the stream
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RingBufferStream : public AudioStream {
 public:
  RingBufferStream(int size = DEFAULT_BUFFER_SIZE) {
    resize(size);
  }

  virtual int available() override {
    // LOGD("RingBufferStream::available: %zu",buffer->available());
    return buffer.available();
  }

  virtual int availableForWrite() override {
    return buffer.availableForWrite();
  }

  virtual void flush() override {}
  virtual int peek() override { return buffer.peek(); }
  virtual int read() override { return buffer.read(); }

  virtual size_t readBytes(uint8_t *data, size_t length) override {
    return buffer.readArray(data, length);
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    // LOGD("RingBufferStream::write: %zu",len);
    return buffer.writeArray(data, len);
  }

  virtual size_t write(uint8_t c) override { return buffer.write(c); }

  void resize(int size){
    buffer.resize(size);
  }

  size_t size() {
    return buffer.size();
  }


 protected:
  RingBuffer<uint8_t> buffer{0};
};


/**
 * @brief AudioStream class which stores the data in a temporary queue buffer.
 * The queue can be consumed e.g. by a callback function by calling readBytes();
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T>
class QueueStream : public AudioStream {
 public:
  /// Default constructor
  QueueStream(int bufferSize, int bufferCount, bool autoRemoveOldestDataIfFull=false)
      : AudioStream() {
    owns_buffer = true;
    callback_buffer_ptr = new NBuffer<T>(bufferSize, bufferCount);
    remove_oldest_data = autoRemoveOldestDataIfFull;
  }
  /// Create stream from any BaseBuffer subclass
  QueueStream(BaseBuffer<T> &buffer){
    owns_buffer = false;
    callback_buffer_ptr = &buffer;
  }

  virtual ~QueueStream() { 
    if(owns_buffer) {
      delete callback_buffer_ptr; 
    }
  }

  /// Activates the output
  virtual bool begin() override {
    TRACEI();
    active = true;
    return true;
  }

  /// stops the processing
  virtual void end() override {
    TRACEI();
    active = false;
  };

  int available() override {
    return callback_buffer_ptr->available()*sizeof(T);
  }

  int availableForWrite() override {
    return callback_buffer_ptr->availableForWrite()*sizeof(T);
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    if (!active) return 0;

    // make space by deleting oldest entries
    if (remove_oldest_data){
      int available_bytes = callback_buffer_ptr->availableForWrite()*sizeof(T);
      if ((int)len>available_bytes){
        int gap = len-available_bytes;
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
    if (active){
      callback_buffer_ptr->reset();
    }
  }

  /// Returns true if active
  operator bool(){
    return active;
  }

 protected:
  BaseBuffer<T> *callback_buffer_ptr;
  bool active;
  bool remove_oldest_data;
  bool owns_buffer;

};

// support legacy name
template <typename T>
using CallbackBufferedStream = QueueStream<T>;

/**
 * @brief Both the data of the read or write
 * operations will be converted with the help of the indicated converter.
 * @ingroup transform
 * @tparam T 
 * @param out 
 * @param converter 
 */
template<typename T>
class ConverterStream : public AudioStream {

    public:
        ConverterStream(Stream &stream, BaseConverter &converter) : AudioStream() {
            p_converter = &converter;
            p_stream = &stream;
        }

        virtual int availableForWrite() { return p_stream->availableForWrite(); }

        virtual size_t write(const uint8_t *buffer, size_t size) { 
          size_t result = p_converter->convert((uint8_t *)buffer, size); 
          if (result>0) {
            size_t result_written = p_stream->write(buffer, result);
            return size * result_written / result;
          }
          return 0;
        }

        size_t readBytes(uint8_t *data, size_t length) override {
           size_t result; p_stream->readBytes(data, length);
           return p_converter->convert(data, result); 
        }


        /// Returns the available bytes in the buffer: to be avoided
        virtual int available() override {
          return p_stream->available();
        }

    protected:
        Stream *p_stream;
        BaseConverter *p_converter;

};

/**
 * @brief Class which measures the truput
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
 */
class MeasuringStream : public AudioStream {
  public:
    MeasuringStream(int count=10, Print *logOut=nullptr){
      this->count = count;
      this->max_count = count;
      p_stream = &null;
      p_print = &null;
      start_time = millis();
      p_logout = logOut;
    }

    MeasuringStream(Print &print, int count=10, Print *logOut=nullptr){
      this->count = count;
      this->max_count = count;
      p_print =&print;
      start_time = millis();
      p_logout = logOut;
    }

    MeasuringStream(Stream &stream, int count=10, Print *logOut=nullptr){
      this->count = count;
      this->max_count = count;
      p_stream =&stream;
      p_print = &stream;
      start_time = millis();
      p_logout = logOut;
    }

        /// Provides the data from all streams mixed together 
    size_t readBytes(uint8_t* data, size_t len) override {
      return measure(p_stream->readBytes(data, len));
    }

    int available()  override {
      return p_stream->available();
    }

    /// Writes raw PCM audio data, which will be the input for the volume control 
    virtual size_t write(const uint8_t *buffer, size_t size) override {
      return measure(p_print->write(buffer, size));
    }

    /// Provides the nubmer of bytes we can write
    virtual int availableForWrite() override { 
      return p_print->availableForWrite();
    }

    /// Returns the actual thrughput in bytes per second
    int bytesPerSecond() {
      return bytes_per_second;
    }

    /// Provides the time when the last measurement was started
    uint32_t startTime() {
      return start_time;
    }

    void setAudioInfo(AudioInfo info){
      AudioStream::info = info;
      setFrameSize(info.bits_per_sample / 8 *info.channels);
    }

    bool begin(AudioInfo info){
      setAudioInfo(info);
      return true;
    }

    void setFrameSize(int size){
      frame_size = size;
    }

  protected:
    int max_count=0;
    int count=0;
    Stream *p_stream=nullptr;
    Print *p_print=nullptr;
    uint32_t start_time;
    int total_bytes = 0;
    int bytes_per_second = 0;
    int frame_size = 0;
    NullStream null;
    Print *p_logout=nullptr;

    size_t measure(size_t len) {
      count--;
      total_bytes+=len;

      if (count<0){
        uint32_t end_time = millis();
        int time_diff = end_time - start_time; // in ms
        if (time_diff>0){
          bytes_per_second = total_bytes / time_diff * 1000;
          printResult();
          count = max_count;
          total_bytes = 0;
          start_time = end_time;
        }
      }
      return len;
    }

    void printResult() {
        char msg[70];
        if (frame_size==0){
          sprintf(msg, "==> Bytes per second: %d", bytes_per_second);
        } else {
          sprintf(msg, "==> Samples per second: %d", bytes_per_second/frame_size);
        }
        if (p_logout!=nullptr){
          p_logout->println(msg);
        } else {
          LOGI("%s",msg);
        }
    }
};

/**
 * @brief Configuration for ProgressStream
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class ProgressStreamInfo : public AudioInfo {
  public:
    size_t total_size = 0;
};
/**
 * @brief Generic calss to measure the the total bytes which were processed in order to 
 * calculate the progress as a percentage of the total size.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
 */
class ProgressStream : public AudioStream {
  public:
    ProgressStream() = default;

    ProgressStream(Print &print){
      setPrint(print);
    }

    ProgressStream(Stream &stream){
      setStream(stream);
    }

    ProgressStream(AudioStream &stream){
      setStream(stream);
      p_info_from = &stream;
    }

    ProgressStreamInfo& defaultConfig() {
      return progress_info;
    }

    void setAudioInfo(AudioInfo info) override {
      AudioStream::setAudioInfo(info);
      progress_info.copyFrom(info);
    }

    void setStream(Stream &stream){
      p_stream =&stream;
      p_print = &stream;
    }

    void setStream(Print &print){
      p_print =&print;
    }

    void setPrint(Print &print){
      p_print =&print;
    }

    bool begin() override {
      if (p_info_from!=nullptr){
        setAudioInfo(p_info_from->audioInfo());
      }
      return AudioStream::begin();
    }

    /// Updates the total size and restarts the percent calculation: Same as calling setSize()
    bool begin(size_t len){
      setSize(len);
      return begin();
    }

    bool begin(ProgressStreamInfo info){
      progress_info = info;
      setAudioInfo(info);
      return begin();
    }

    /// Updates the total size and restarts the percent calculation
    void setSize(size_t len){
      total_processed = 0;
      progress_info.total_size = len;
    }

    /// Provides the current total size (defined by setSize)
    size_t size(){
      return progress_info.total_size;
    }

    /// Provides the number of processed bytes
    size_t processedBytes() {
      return total_processed;
    }

    /// Provides the number of processed seconds
    size_t processedSecs() {
      return total_processed / byteRate();
    }

    /// Provides the total_size provided in the configuration
    size_t totalBytes() {
      return progress_info.total_size;
    }

    /// Converts the totalBytes() to seconds
    size_t totalSecs() {
      return totalBytes() / byteRate();
    }

    /// Provides the processed percentage: If no size has been defined we return 0 
    float percentage() {
      if (progress_info.total_size==0) return 0;
      return 100.0 * total_processed / progress_info.total_size;
    }

        /// Provides the data from all streams mixed together 
    size_t readBytes(uint8_t* data, size_t len) override {
      if (p_stream==nullptr) return 0;
      return measure(p_stream->readBytes(data, len));
    }

    int available()  override {
      if (p_stream==nullptr) return 0;
      return p_stream->available();
    }

    /// Writes raw PCM audio data, which will be the input for the volume control 
    virtual size_t write(const uint8_t *buffer, size_t size) override {
      if (p_print==nullptr) return 0;
      return measure(p_print->write(buffer, size));
    }

    /// Provides the nubmer of bytes we can write
    virtual int availableForWrite() override { 
      if (p_print==nullptr) return 0;
      return p_print->availableForWrite();
    }

  protected:
    ProgressStreamInfo progress_info;
    Stream *p_stream=nullptr;
    Print *p_print=nullptr;
    AudioInfoSupport *p_info_from=nullptr;
    size_t total_processed = 0;

    size_t measure(size_t len) {
      total_processed += len;
      return len;
    }

    size_t byteRate() {
      AudioInfo info = audioInfo();
      int byte_rate = info.sample_rate * info.bits_per_sample * info.channels / 8;
      if (byte_rate==0){
        LOGE("Audio Info not defined");
        return 0;
      }
      return byte_rate;
    }

};

/**
 * @brief Configure Throttle setting
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct ThrottleConfig : public AudioInfo {
  ThrottleConfig() {
    sample_rate = 44100;
    bits_per_sample = 16;
    channels = 2;
  }
  int correction_us = 0;
};

/**
 * @brief Throttle the sending or receiving of the audio data to limit it to the indicated
 * sample rate.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Throttle : public AudioStream {
 public:
  Throttle() = default;
  Throttle(Print &out) { p_out = &out; } 
  Throttle(Stream &out) { p_out = &out; p_in = &out; } 

  ThrottleConfig defaultConfig() {
    ThrottleConfig c;
    return c;
  }

  bool begin(ThrottleConfig cfg) {
    LOGI("begin sample_rate: %d, channels: %d, bits: %d", info.sample_rate, info.channels, info.bits_per_sample);
    this->info = cfg;
    this->cfg = cfg;
    return begin();
  }

  bool begin(AudioInfo info) {
    LOGI("begin sample_rate: %d, channels: %d, bits: %d", info.sample_rate, info.channels, info.bits_per_sample);
    this->info = info;
    this->cfg.copyFrom(info);
    return begin();
  }

  bool begin(){ 
    frame_size = cfg.bits_per_sample / 8 * cfg.channels;
    startDelay();
    return true;
  }

  // (re)starts the timing
  void startDelay() { 
    start_time = micros(); 
    sum_frames = 0;
  }

  int availableForWrite() {
    if (p_out){
      return p_out->availableForWrite();
    }
    return DEFAULT_BUFFER_SIZE;
  }

  size_t write(const uint8_t* data, size_t len){
    size_t result = p_out->write(data, len);
    delayBytes(len);
    return result;
  }

  int available() {
    if (p_in==nullptr) return 0;
    return p_in->available();
  }

  size_t readBytes(uint8_t* data, size_t len){
    if (p_in==nullptr) {
      delayBytes(len);
      return 0;
    } 
    size_t result = p_in->readBytes(data, len);
    delayBytes(len);
    return result;
  }

  // delay
  void delayBytes(size_t bytes) { delayFrames(bytes / frame_size); }

  // delay
  void delayFrames(size_t frames) {
    sum_frames += frames;
    uint64_t durationUsEff = micros() - start_time;
    uint64_t durationUsToBe = getDelayUs(sum_frames);
    int64_t waitUs = durationUsToBe - durationUsEff + cfg.correction_us;
    LOGI("wait us: %ld", (long) waitUs);
    if (waitUs > 0) {
      int64_t waitMs = waitUs / 1000;
      if (waitMs > 0) delay(waitMs);
      delayMicroseconds(waitUs - (waitMs * 1000));
    } else {
      LOGD("negative delay!")
    }
  }

  inline int64_t getDelayUs(uint64_t frames){
    return (frames * 1000000) / cfg.sample_rate;
  }

  inline int64_t getDelayMs(uint64_t frames){
    return getDelayUs(frames) / 1000;
  }

  inline int64_t getDelaySec(uint64_t frames){
    return getDelayUs(frames) / 1000000l;
  }

 protected:
  uint32_t start_time = 0;
  uint32_t sum_frames = 0;
  ThrottleConfig cfg;
  int frame_size = 0;
  Print *p_out = nullptr;
  Stream *p_in = nullptr;
};


/**
 * @brief MixerStream is mixing the input from Multiple Input Streams.
 * All streams must have the same audo format (sample rate, channels, bits per sample). 
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template<typename T>
class InputMixer : public AudioStream {
  public:
    InputMixer() = default;

    /// Adds a new input stream
    void add(Stream &in, int weight=100){
      streams.push_back(&in);
      weights.push_back(weight);
      total_weights += weight;
    }

    virtual bool begin(AudioInfo info) {
  	  setAudioInfo(info);
      frame_size = info.bits_per_sample/8 * info.channels;
      LOGI("frame_size: %d",frame_size);
  	  return frame_size>0;
    }

    /// Dynamically update the new weight for the indicated channel: If you set it to 0 it is muted (and the stream is not read any more). We recommend to use values between 1 and 100
    void setWeight(int channel, int weight){
      if (channel<size()){
        weights[channel] = weight;
        int total = 0;
        for (int j=0;j<weights.size();j++){
          total += weights[j];
        }
        total_weights = total;
      } else {
        LOGE("Invalid channel %d - max is %d", channel, size()-1);
      }
    }

    /// Remove all input streams
    void end() override {
      streams.clear();
      weights.clear();
      result_vect.clear();
      current_vect.clear();
      total_weights = 0.0;
    }

    /// Number of stremams to which are mixed together
    int size() {
      return streams.size();
    }

    /// Provides the data from all streams mixed together
    size_t readBytes(uint8_t* data, size_t len) override {
      if (total_weights==0 || frame_size==0 || len==0) {
        LOGW("readBytes: %d",(int)len);
        return 0;
      }

      if (limit_available_data){
        len = min(len, availableBytes());
      }

      // result_len must be full frames
      int result_len = len * frame_size / frame_size;
      // replace sample based with vector based implementation
      //readBytesSamples((T*)data, result_len));
      readBytesVector((T*)data, result_len);
      return result_len;
    }

    // Commented out because URLStream returns 0 or 1 most of the time
    // /// Provides the available bytes from the first stream with data
    // int available()  override {
    //   int result = 1024;
    //   for (int j=0;j<size();j++){
    //     result = streams[j]->available();
    //     if (result>0) 
    //       break;
    //   }
    //   return result;
    // }

    /// Do not wait for all data to be available
    void setLimitToAvailableData(bool flag){
      limit_available_data = flag;
    }

  protected:
    Vector<Stream*> streams{10};
    Vector<int> weights{10}; 
    int total_weights = 0;
    int frame_size = 4;
    bool limit_available_data = false;;

    Vector<int> result_vect;
    Vector<T> current_vect;

    /// mixing using a vector of samples
    void readBytesVector(T* p_data, int byteCount) {
      int samples = byteCount / sizeof(T);
      result_vect.resize(samples);
      current_vect.resize(samples);
      int stream_count = size();
      resultClear();
      for (int j=0;j<stream_count;j++){
        if (weights[j]>0){
          readSamples(streams[j],current_vect.data(), samples);
          float fact = static_cast<float>(weights[j]) / total_weights;
          resultAdd(fact);
        }
      }
      // copy result
      for (int j=0;j<samples;j++){
        p_data[j] = result_vect[j];
      }
    }

    /// Provides the available bytes from the first stream with data
    int availableBytes()  {
      int result = DEFAULT_BUFFER_SIZE;
      for (int j=0;j<size();j++){
        result = min(result, streams[j]->available());
      }
      return result;
    }

    void resultAdd(float fact){
      for (int j=0;j<current_vect.size();j++){
        current_vect[j]*=fact;
        result_vect[j] += current_vect[j];
      }
    }

    void resultClear(){
      memset(result_vect.data(), 0, sizeof(int)*result_vect.size());
    }


    // mixing using individual samples
    // void readBytesSamples(T* p_data, int result_len) {
    //     //int result_len = MIN(available(), len) * frame_size / frame_size;
    //   //LOGD("readBytes: %d",(int)len);
    //   int sample_count = result_len / sizeof(T);
    //   int sample_total = 0;
    //   int size_value = size();
    //   //LOGD("size_value: %d", size_value);
    //   for (int j=0;j<sample_count; j++){
    //     sample_total = 0.0f;
    //     for (int i=0; i<size_value; i++){
    //       T sample = readSample<T>(streams[i]);
    //       sample_total += weights[i] * sample / total_weights ;          
    //     }
    //     p_data[j] = sample_total;
    //   }
    // }

};

/**
 * @brief Merges multiple input channels. The input must be mono!
 * So if you provide 2 mono channels you get a stereo signal as result 
 * with the left channel from channel 0 and the right from channel 1
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 */
template<typename T>
class InputMerge : public AudioStream {
  public:
    /// Default constructor
    InputMerge() = default;

    /// @brief Constructor for stereo signal from to mono input stream
    /// @param left 
    /// @param right 
    InputMerge(Stream &left, Stream &right) {
      add(left);
      add(right);
    };

    virtual bool begin(AudioInfo info) {
      if (size()!=info.channels){
        info.channels = size();
        LOGW("channels corrected to %d", size());
      }
  	  setAudioInfo(info);
  	  return true;
    }

    /// Provides the data from all streams mixed together
    size_t readBytes(uint8_t* data, size_t len) override {
      LOGD("readBytes: %d",(int)len);
      T *p_data = (T*) data;
      int result_len = MIN(available(), len/size());
      int sample_count = result_len / sizeof(T);
      int size_value = size();
      int result_idx = 0;
      for (int j=0;j<sample_count; j++){
        for (int i=0; i<size_value; i++){
          p_data[result_idx++] = weights[i] * readSample<T>(streams[i]);
        }
      }
      return result_idx*sizeof(T);
    }

    /// Adds a new input stream
    void add(Stream &in, float weight=1.0){
      streams.push_back(&in);
      weights.push_back(weight);
    }

    /// Defines a new weight for the indicated channel: If you set it to 0 it is muted.
    void setWeight(int channel, float weight){
      if (channel<size()){
        weights[channel] = weight;
      } else {
        LOGE("Invalid channel %d - max is %d", channel, size()-1);
      }
    }

    /// Remove all input streams
    void end() override {
      streams.clear();
      weights.clear();
    }

    /// Number of stremams to which are mixed together = number of result channels
    int size() {
      return streams.size();
    }

    /// Provides the min available data from all streams
    int available()  override {
      int result = streams[0]->available();
      for (int j=1;j<size();j++){
        int tmp = streams[j]->available();
        if (tmp<result){
          result = tmp;
        } 
      }
      return result;
    }

  protected:
    Vector<Stream*> streams{10};
    Vector<float> weights{10}; 
};


/**
 * @brief CallbackStream: A Stream that allows to register callback methods for accessing and providing data
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CallbackStream : public AudioStream {
  public: 
    CallbackStream() = default;

    CallbackStream(size_t (*cb_read)(uint8_t* data, size_t len), size_t (*cb_write)(const uint8_t* data, size_t len)) {
      setWriteCallback(cb_write);
      setReadCallback(cb_read);
    }

    void setWriteCallback(size_t (*cb_write)(const uint8_t* data, size_t len)){
      this->cb_write = cb_write;
    }

    void setReadCallback(size_t (*cb_read)(uint8_t* data, size_t len)){
      this->cb_read = cb_read;
    }

    virtual bool begin(AudioInfo info) {
  	  setAudioInfo(info);
  	  return begin();
    }

    virtual bool begin() override {
      active = true;
      return true;
    }
    void end() override { active = false;}

    size_t readBytes(uint8_t* data, size_t len) override {
      if (!active) return 0;
      if (cb_read){
        return cb_read(data, len);
      }
      return 0;
    }

    size_t write(const uint8_t* data, size_t len) override {
      if (!active) return 0;
      if (cb_write){
        return cb_write(data, len);
      }
      return 0;
    }

  protected:
    bool active=true;
    size_t (*cb_write)(const uint8_t* data, size_t len) = nullptr;
    size_t (*cb_read)(uint8_t* data, size_t len) = nullptr;
};

/**
 * @brief Provides data from a concatenation of Streams. Please note that the provided
 * Streams can be played only once! You will need to reset them (e.g. moving the file pointer to the beginning) 
 * and readd them back if you want to process them a second time. The default timeout on the available() method
 * is set to 0. This might be not good if you use e.g. a URLStream. 
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CatStream : public AudioStream {
  public:
    CatStream(){
      _timeout = 0;
    }
    void add(Stream *stream){
      input_streams.push_back(stream);
    }
    void add(Stream &stream){
      input_streams.push_back(&stream);    
    }

    bool begin() override {
      is_active = true;
      return AudioStream::begin();
    }

    void end() override {
      is_active = false;
      return AudioStream::end();
    }

    int available() override {
      if (!is_active) return 0;
      if (!moveToNextStreamOnEnd()){
        return 0;
      }
      return availableWithTimout();
    }

    size_t readBytes(uint8_t* data, size_t len) override {
      if (!is_active) return 0;
      if (!moveToNextStreamOnEnd()){
        return 0;
      }
      return p_current_stream->readBytes(data, len);
    }

    /// Returns true if active and we still have data
    operator bool(){
      return is_active && available()>0;
    }

    void setOnBeginCallback(void (*callback)(Stream* stream) ){    
      begin_callback = callback;
    }
    void setOnEndCallback(void (*callback)(Stream* stream) ){
      end_callback = callback;    
    }

protected:
  Vector<Stream*> input_streams;
  Stream *p_current_stream = nullptr;
  bool is_active = false;
  void (*begin_callback)(Stream* stream) = nullptr;
  void (*end_callback)(Stream* stream) = nullptr;

  /// moves to the next stream if necessary: returns true if we still have a valid stream
  bool moveToNextStreamOnEnd(){
    // keep on running
    if (p_current_stream!=nullptr && p_current_stream->available()>0) return true;
    // at end?
    if ((p_current_stream==nullptr || availableWithTimout()==0)){
      if (end_callback && p_current_stream) end_callback(p_current_stream);
      if (!input_streams.empty()) {
        LOGI("using next stream");
        p_current_stream = input_streams[0];
        input_streams.pop_front();
        if (begin_callback && p_current_stream) begin_callback(p_current_stream);
      } else {
        p_current_stream = nullptr;
      }
    }
    // returns true if we have a valid stream
    return p_current_stream!=nullptr;
  }

  int availableWithTimout(){
    int result = p_current_stream->available();
    if (result==0){
      for (int j=0; j <_timeout/10;j++){
        delay(10);
        result = p_current_stream->available();
        if (result!=0) break;
      }
    }
    return result;
  }

};

/**
 * @brief  Stream to which we can apply Filters for each channel. The filter 
 * might change the result size!
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template<typename T, class TF>
class FilteredStream : public AudioStream {
  public:
        FilteredStream(Stream &stream) : AudioStream() {
          p_stream = &stream;
        }
        FilteredStream(Stream &stream, int channels) : AudioStream() {
          this->channels = channels;
          p_stream = &stream;
          p_converter = new ConverterNChannels<T,TF>(channels);
        }

        bool begin(AudioInfo info){
          setAudioInfo(info);
          this->channels = info.channels;
          if (p_converter !=nullptr && info.channels!=channels){
            LOGE("Inconsistent number of channels");
            return false;
          }
          return begin();
        }

        bool begin() override {
          if (channels ==0){
            LOGE("channels must not be 0");
            return false;
          }
          if (p_converter==nullptr){
            p_converter = new ConverterNChannels<T,TF>(channels);
          }
          return AudioStream::begin();
        }


        virtual size_t write(const uint8_t *buffer, size_t size) override { 
           if (p_converter==nullptr) return 0;
           size_t result = p_converter->convert((uint8_t *)buffer, size); 
           return p_stream->write(buffer, result);
        }

        size_t readBytes(uint8_t *data, size_t length) override {
           if (p_converter==nullptr) return 0;
           size_t result = p_stream->readBytes(data, length);
           result = p_converter->convert(data, result); 
           return result;
        }

        virtual int available() override {
          return p_stream->available();
        }

        virtual int availableForWrite() override { 
          return p_stream->availableForWrite();
        }

        /// defines the filter for an individual channel - the first channel is 0. The number of channels must have
        /// been defined before we can call this function.
        void setFilter(int channel, Filter<TF> *filter) {
          if (p_converter!=nullptr){
            p_converter->setFilter(channel, filter);
          } else {
            LOGE("p_converter is null");
          }
        }

        /// defines the filter for an individual channel - the first channel is 0. The number of channels must have
        /// been defined before we can call this function.
        void setFilter(int channel, Filter<TF> &filter) {
          setFilter(channel, &filter);
        }

    protected:
        int channels=0;
        Stream *p_stream;
        ConverterNChannels<T,TF> *p_converter;

};

#ifdef USE_TIMER
/**
 * @brief TimerCallbackAudioStream Configuration
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct TimerCallbackAudioStreamInfo : public AudioInfo {
  RxTxMode rx_tx_mode = RX_MODE;
  uint16_t buffer_size = DEFAULT_BUFFER_SIZE;
  bool use_timer = true;
  int timer_id = -1;
  TimerFunction timer_function = DirectTimerCallback;
  bool adapt_sample_rate = false;
  uint16_t (*callback)(uint8_t *data, uint16_t len) = nullptr;
};

// forward declaration: relevant only if use_timer == true
// void IRAM_ATTR timerCallback(void* obj);

/**
 * @brief Callback driven Audio Source (rx_tx_mode==RX_MODE) or Audio Sink
 * (rx_tx_mode==TX_MODE). This class allows to to integrate external libraries
 * in order to consume or generate a data stream which is based on a timer
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class TimerCallbackAudioStream : public BufferedStream {
  friend void IRAM_ATTR timerCallback(void *obj);

 public:
  TimerCallbackAudioStream() : BufferedStream(80) { TRACED(); }

  ~TimerCallbackAudioStream() {
    TRACED();
    if (timer != nullptr) delete timer;
    if (buffer != nullptr) delete buffer;
    if (frame != nullptr) delete[] frame;
  }

  /// Provides the default configuration
  TimerCallbackAudioStreamInfo defaultConfig() {
    TimerCallbackAudioStreamInfo def;
    return def;
  }

  /// updates the audio information
  virtual void setAudioInfo(AudioInfo info) {
    TRACED();
    if (cfg.sample_rate != info.sample_rate || cfg.channels != info.channels ||
        cfg.bits_per_sample != info.bits_per_sample) {
      bool do_restart = active;
      if (do_restart) end();
      cfg.sample_rate = info.sample_rate;
      cfg.channels = info.channels;
      cfg.bits_per_sample = info.bits_per_sample;
      if (do_restart) begin(cfg);
    }
  }

  /// Defines the target that needs to be notified
  void setNotifyAudioChange(AudioInfoSupport &bi) { notifyTarget = &bi; }

  /// Provides the current audio information
  TimerCallbackAudioStreamInfo audioInfoExt() { return cfg; }
  AudioInfo audioInfo() { return cfg; }

  void begin(TimerCallbackAudioStreamInfo config) {
    LOGD("%s:  %s", LOG_METHOD,
         config.rx_tx_mode == RX_MODE ? "RX_MODE" : "TX_MODE");
    this->cfg = config;
    this->frameCallback = config.callback;
    if (cfg.use_timer) {
      frameSize = cfg.bits_per_sample * cfg.channels / 8;
      frame = new uint8_t[frameSize];
      buffer = new RingBuffer<uint8_t>(cfg.buffer_size);
      timer = new TimerAlarmRepeating();
      timer->setTimerFunction(cfg.timer_function);
      if (cfg.timer_id>=0){
        timer->setTimer(cfg.timer_id);
      }
      time = AudioTime::toTimeUs(cfg.sample_rate);
      LOGI("sample_rate: %u -> time: %u milliseconds",  (unsigned int)cfg.sample_rate,  (unsigned int)time);
      timer->setCallbackParameter(this);
      timer->begin(timerCallback, time, TimeUnit::US);
    }

    notifyAudioChange();
    active = true;
  }

  /// Restart the processing
  bool begin() {
    TRACED();
    if (this->frameCallback != nullptr) {
      if (cfg.use_timer) {
        timer->begin(timerCallback, time, TimeUnit::US);
      }
      active = true;
    }
    return active;
  }

  /// Stops the processing
  void end() {
    TRACED();
    if (cfg.use_timer) {
      timer->end();
    }
    active = false;
  }

  /// Provides the effective sample rate
  uint16_t currentSampleRate() { return currentRateValue; }

 protected:
  TimerCallbackAudioStreamInfo cfg;
  AudioInfoSupport *notifyTarget = nullptr;
  bool active = false;
  uint16_t (*frameCallback)(uint8_t *data, uint16_t len);
  // below only relevant with timer
  TimerAlarmRepeating *timer = nullptr;
  RingBuffer<uint8_t> *buffer = nullptr;
  uint8_t *frame = nullptr;
  uint16_t frameSize = 0;
  uint32_t time = 0;
  unsigned long lastTimestamp = 0u;
  uint32_t currentRateValue = 0;
  uint32_t printCount = 0;

  // used for audio sink
  virtual size_t writeExt(const uint8_t *data, size_t len) override {
    if (!active) return 0;
    TRACED();
    size_t result = 0;
    if (!cfg.use_timer) {
      result = frameCallback((uint8_t *)data, len);
    } else {
      result = buffer->writeArray((uint8_t *)data, len);
    }
    if (++printCount % 10000 == 0) printSampleRate();
    return result;
  }

  // used for audio source
  virtual size_t readExt(uint8_t *data, size_t len) override {
    if (!active) return 0;
    TRACED();

    size_t result = 0;
    if (!cfg.use_timer) {
      result = frameCallback(data, len);
    } else {
      result = buffer->readArray(data, len);
    }
    if (++printCount % 10000 == 0) printSampleRate();
    return result;
  }

  /// calculates the effective sample rate
  virtual void measureSampleRate() {
    unsigned long ms = millis();
    if (lastTimestamp > 0u) {
      uint32_t diff = ms - lastTimestamp;
      if (diff > 0) {
        uint16_t rate = 1 * 1000 / diff;

        if (currentRateValue == 0) {
          currentRateValue = rate;
        } else {
          currentRateValue = (currentRateValue + rate) / 2;
        }
      }
    }
    lastTimestamp = ms;
  }

  /// log and update effective sample rate
  virtual void printSampleRate() {
    LOGI("effective sample rate: %u", (unsigned int)currentRateValue);
    if (cfg.adapt_sample_rate &&
        abs((int)currentRateValue - cfg.sample_rate) > 200) {
      cfg.sample_rate = currentRateValue;
      notifyAudioChange();
    }
  }

  /// Update Audio Information in target device
  virtual void notifyAudioChange() {
    if (notifyTarget != nullptr) {
      notifyTarget->setAudioInfo(cfg);
    }
  }

  inline static void IRAM_ATTR timerCallback(void *obj);
};

// relevant only if use_timer == true
inline void TimerCallbackAudioStream::timerCallback(void *obj) {
  TimerCallbackAudioStream *src = (TimerCallbackAudioStream *)obj;
  if (src != nullptr) {
    // LOGD("%s:  %s", LOG_METHOD, src->cfg.rx_tx_mode==RX_MODE ?
    // "RX_MODE":"TX_MODE");
    if (src->cfg.rx_tx_mode == RX_MODE) {
      // input
      uint16_t available_bytes = src->frameCallback(src->frame, src->frameSize);
      uint16_t buffer_available = src->buffer->availableForWrite();
      if (buffer_available < available_bytes) {
        // if buffer is full make space
        uint16_t to_clear = available_bytes - buffer_available;
        uint8_t tmp[to_clear];
        src->buffer->readArray(tmp, to_clear);
      }
      if (src->buffer->writeArray(src->frame, available_bytes) !=
          available_bytes) {
        assert(false);
      }
    } else {
      // output
      if (src->buffer != nullptr && src->frame != nullptr &&
          src->frameSize > 0) {
        uint16_t available_bytes =
            src->buffer->readArray(src->frame, src->frameSize);
        if (available_bytes !=
            src->frameCallback(src->frame, available_bytes)) {
          LOGE("data underflow");
        }
      }
    }
    src->measureSampleRate();
  }
}

#endif

}  // namespace audio_tools
