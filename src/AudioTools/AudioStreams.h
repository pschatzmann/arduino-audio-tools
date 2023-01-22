#pragma once
#include "AudioConfig.h"
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
 
namespace audio_tools {

/**
 * @brief Base class for all Audio Streams. It support the boolean operator to
 * test if the object is ready with data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioStream : public Stream, public AudioBaseInfoDependent, public AudioBaseInfoSource {
 public:
  AudioStream() = default;
  virtual ~AudioStream() = default;
  AudioStream(AudioStream const&) = delete;
  AudioStream& operator=(AudioStream const&) = delete;


  virtual bool begin(){return true;}
  virtual void end(){}
  
  // Call from subclass or overwrite to do something useful
  virtual void setAudioInfo(AudioBaseInfo info) override {
      TRACED();
      this->info = info;
      info.logInfo();
      if (p_notify!=nullptr){
          p_notify->setAudioInfo(info);
      }
  }

  virtual void  setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
      p_notify = &bi;
  }

  virtual size_t readBytes(char *buffer, size_t length) {
    return readBytes((uint8_t *)buffer, length);
  }

  virtual size_t readBytes(uint8_t *buffer, size_t length) STREAM_WRITE_OVERRIDE = 0;

  virtual size_t write(const uint8_t *buffer, size_t size) override = 0;

  operator bool() { return available() > 0; }

  virtual AudioBaseInfo audioInfo() override {
    return info;
  }

  virtual int availableForWrite() override { return DEFAULT_BUFFER_SIZE; }

  virtual void flush() override {}

  virtual Stream* toStreamPointer() {
    return this;
  }

  /// Writes n 0 values (= silence)
  /// @param len 
  virtual void writeSilence(size_t len){
    int16_t zero = 0;
    for (int j=0;j<len/2;j++){
      write((uint8_t*)&zero,2);
    } 
  }

 protected:
  AudioBaseInfoDependent *p_notify=nullptr;
  AudioBaseInfo info;

  virtual int not_supported(int out) {
    LOGE("AudioStreamX: unsupported operation!");
    return out;
  }

};

/**
 * @brief Same as AudioStream - but we do not have any abstract methods
 *
 */
class AudioStreamX : public AudioStream {
 public:
  AudioStreamX() = default;
  virtual ~AudioStreamX() = default;
  AudioStreamX(AudioStreamX const&) = delete;
  AudioStreamX& operator=(AudioStreamX const&) = delete;

  virtual size_t readBytes(uint8_t *buffer, size_t length) override { return not_supported(0); }
  virtual size_t write(const uint8_t *buffer, size_t size) override{ return not_supported(0); }
  virtual size_t write(uint8_t) override { return not_supported(0); }
  virtual int available() override { return not_supported(0); };

  virtual int read() override { return not_supported(-1); }
  virtual int peek() override { return not_supported(-1); }
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


enum MemoryType {RAM, PS_RAM, FLASH_RAM};

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
  }

  MemoryStream(const uint8_t *buffer, int buffer_size, MemoryType memoryType = FLASH_RAM) {
    LOGD("MemoryStream: %d", buffer_size);
    this->buffer_size = buffer_size;
    this->write_pos = buffer_size;
    this->buffer = (uint8_t *)buffer;
    this->memory_type = memoryType;
  }

  ~MemoryStream() {
    TRACED();
    if (memoryCanChange() && buffer!=nullptr) free(buffer);
  }

  // resets the read pointer
  bool begin() override {
    TRACED();
    write_pos = memoryCanChange() ? 0 : buffer_size;
    if (this->buffer==nullptr){
      resize(buffer_size);
    }
    read_pos = 0;
    return true;
  }

  virtual size_t write(uint8_t byte) override {
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
      if (reset) {
        // we clear the buffer data
        memset(buffer, 0, buffer_size);
      }
    } else {
      LOGE("data is read only");
    }
  }

  virtual void setLoop(bool loop){
    is_loop = loop;
  }

  virtual void resize(size_t size){
    if (memoryCanChange()){      
      buffer_size = size;
      switch(memory_type){
  #ifdef ESP32
        case PS_RAM:
          buffer = (buffer==nullptr) ? (uint8_t*)ps_calloc(size,1) : (uint8_t*)ps_realloc(buffer, size);
          break;
  #endif
        default:
          buffer = (buffer==nullptr) ? (uint8_t*)calloc(size,1) : (uint8_t*)realloc(buffer, size);
          break;
      }
    }
  }

  virtual uint8_t* data(){
    return buffer;
  }

  void setRewindCallback(void (*cb)()){
    this->rewind = cb;
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
class DynamicMemoryStream : public AudioStreamX {
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
class GeneratedSoundStream : public AudioStreamX {
 public:
  GeneratedSoundStream() = default;
  
  GeneratedSoundStream(SoundGenerator<T> &generator) {
    TRACED();
    setInput(generator);
  }

  void setInput(SoundGenerator<T> &generator){
    this->generator_ptr = &generator;
  }

  AudioBaseInfo defaultConfig() { return this->generator_ptr->defaultConfig(); }

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
  bool begin(AudioBaseInfo cfg) {
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

  virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
    audioBaseInfoDependent = &bi;
  }

  AudioBaseInfo audioInfo() override {
    return generator_ptr->audioInfo();
  }

  /// This is unbounded so we just return the buffer size
  virtual int available() override { return DEFAULT_BUFFER_SIZE; }

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
  AudioBaseInfoDependent *audioBaseInfoDependent = nullptr;
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
 * when used as audio target
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NullStream : public BufferedStream {
 public:
  NullStream(bool measureWrite = false) : BufferedStream(100) {
    is_measure = measureWrite;
  }

  bool begin(AudioBaseInfo info, int opt = 0) {    
    return true;
  }

  AudioBaseInfo defaultConfig(int opt = 0) {
    AudioBaseInfo info;
    return info;
  }

  /// Define object which need to be notified if the basinfo is changing
  void setNotifyAudioChange(AudioBaseInfoDependent &bi) override {}

  void setAudioInfo(AudioBaseInfo info) override {}

 protected:
  size_t total = 0;
  unsigned long timeout = 0;
  bool is_measure;

  virtual size_t writeExt(const uint8_t *data, size_t len) override {
    if (is_measure) {
      if (millis() < timeout) {
        total += len;
      } else {
        LOGI("Thruput = %zu kBytes/sec", total / 1000);
        total = 0;
        timeout = millis() + 1000;
      }
    }
    return len;
  }
  virtual size_t readExt(uint8_t *data, size_t len) override {
    memset(data, 0, len);
    return len;
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
 * @brief AudioOutput class which stores the data in a temporary queue buffer.
 * The queue can be consumed e.g. by a callback function by calling readBytes();
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T>
class QueueStream : public AudioStreamX {
 public:
  /// Default constructor
  QueueStream(int bufferSize, int bufferCount, bool autoRemoveOldestDataIfFull=false)
      : AudioStreamX() {
    callback_buffer_ptr = new NBuffer<T>(bufferSize, bufferCount);
    remove_oldest_data = autoRemoveOldestDataIfFull;
  }
  /// Create stream from any BaseBuffer subclass
  QueueStream(BaseBuffer<T> &buffer){
    callback_buffer_ptr = &buffer;
  }

  virtual ~QueueStream() { delete callback_buffer_ptr; }

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
template<typename T, class ConverterT>
class ConverterStream : public AudioStreamX {

    public:
        ConverterStream(Stream &stream, ConverterT &converter) : AudioStreamX() {
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
        ConverterT *p_converter;

};

/**
 * @brief Class which measures the truput
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
 */
class MeasuringStream : public AudioStreamX {
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
    uint64_t startTime() {
      return start_time;
    }

    void setBytesPerSample(int size){
      sample_div = size;
    }

  protected:
    int max_count=0;
    int count=0;
    Stream *p_stream=nullptr;
    Print *p_print=nullptr;
    uint64_t start_time;
    int total_bytes = 0;
    int bytes_per_second = 0;
    int sample_div = 0;
    NullStream null;
    Print *p_logout=nullptr;

    size_t measure(size_t len) {
      count--;
      total_bytes+=len;

      if (count<0){
        uint64_t end_time = millis();
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
        if (sample_div==0){
          sprintf(msg, "==> Bytes per second: %d", bytes_per_second);
        } else {
          sprintf(msg, "==> Samples per second: %d", bytes_per_second/sample_div);
        }
        if (p_logout!=nullptr){
          p_logout->println(msg);
        } else {
          LOGI("%s",msg);
        }
    }
};


/**
 * @brief MixerStream is mixing the input from Multiple Input Streams.
 * All streams must have the same audo format (sample rate, channels, bits per sample) 
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template<typename T>
class InputMixer : public AudioStreamX {
  public:
    InputMixer() = default;

    /// Adds a new input stream
    void add(Stream &in, float weight=1.0){
      streams.push_back(&in);
      weights.push_back(weight);
      total_weights += weight;
    }

    virtual bool begin(AudioBaseInfo info) {
  	  setAudioInfo(info);
      frame_size = info.bits_per_sample/8 * info.channels;
  	  return true;
    }

    /// Defines a new weight for the indicated channel: If you set it to 0 it is muted.
    void setWeight(int channel, float weight){
      if (channel<size()){
        weights[channel] = weight;
        float total = 0;
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
      total_weights = 0.0;
    }

    /// Number of stremams to which are mixed together
    int size() {
      return streams.size();
    }

    /// Provides the data from all streams mixed together
    size_t readBytes(uint8_t* data, size_t len) override {
      if (total_weights==0) return 0;
      LOGD("readBytes: %d",(int)len);
      // result_len must be full frames
      int result_len = MIN(available(), len) * frame_size / frame_size;
      int sample_count = result_len / sizeof(T);
      T *p_data = (T*) data;
      float sample_total = 0;
      int size_value = size();
      for (int j=0;j<sample_count; j++){
        sample_total = 0.0f;
        for (int i=0; i<size_value; i++){
          T sample = readSample<T>(streams[i]);
          sample_total += weights[i] * sample / total_weights ;          
        }
        p_data[j] = sample_total;
      }
      return result_len;
    }

    /// Provides the available bytes from the first stream with data
    int available()  override {
      int result = 0;
      for (int j=0;j<size();j++){
        result = streams[j]->available();
        if (result>0) 
          break;
      }
      return result;
    }

  protected:
    Vector<Stream*> streams{10};
    Vector<float> weights{10}; 
    float total_weights = 0.0;
    int frame_size = 4;

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
class InputMerge : public AudioStreamX {
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

    virtual bool begin(AudioBaseInfo info) {
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

// support legicy VolumeOutput
//typedef VolumeStream VolumeOutput;

/**
 * @brief  Stream to which we can apply Filters for each channel. The filter 
 * might change the result size!
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template<typename T, class TF>
class FilteredStream : public AudioStreamX {
  public:
        FilteredStream(Stream &stream, int channels=2) : AudioStreamX() {
          this->channels = channels;
          p_stream = &stream;
          p_converter = new ConverterNChannels<T,TF>(channels);
        }

        virtual size_t write(const uint8_t *buffer, size_t size) override { 
           size_t result = p_converter->convert((uint8_t *)buffer, size); 
           return p_stream->write(buffer, result);
        }

        size_t readBytes(uint8_t *data, size_t length) override {
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

        /// defines the filter for an individual channel - the first channel is 0
        void setFilter(int channel, Filter<TF> *filter) {
          p_converter->setFilter(channel, filter);
        }

    protected:
        int channels;
        Stream *p_stream;
        ConverterNChannels<T,TF> *p_converter;

};



#ifdef USE_TIMER
/**
 * @brief TimerCallbackAudioStream Configuration
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct TimerCallbackAudioStreamInfo : public AudioBaseInfo {
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
  virtual void setAudioInfo(AudioBaseInfo info) {
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
  void setNotifyAudioChange(AudioBaseInfoDependent &bi) { notifyTarget = &bi; }

  /// Provides the current audio information
  TimerCallbackAudioStreamInfo audioInfoExt() { return cfg; }
  AudioBaseInfo audioInfo() { return cfg; }

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
  AudioBaseInfoDependent *notifyTarget = nullptr;
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
