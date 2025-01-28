#pragma once
#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimer.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/BaseConverter.h"
#include "AudioTools/CoreAudio/AudioEffects/SoundGenerator.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/AudioOutput.h"

#ifndef IRAM_ATTR
#  define IRAM_ATTR
#endif


namespace audio_tools {

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

  virtual size_t readBytes(uint8_t *data, size_t len) {
      //Serial.print("Timeout audiostream: ");
      //Serial.println(p_stream->getTimeout());
    return p_stream->readBytes(data, len);
  }

  int read() { return p_stream->read(); }

  int peek() { return p_stream->peek(); }

  int available() { return p_stream->available(); }

  virtual size_t write(uint8_t c) { return p_stream->write(c); }

  virtual size_t write(const uint8_t *data, size_t len) {
    return p_stream->write(data, len);
  }

  virtual int availableForWrite() { return p_stream->availableForWrite(); }

  virtual void flush() { p_stream->flush(); }

 protected:
  Stream *p_stream;
  int32_t clientTimeout = URL_CLIENT_TIMEOUT; // 60000;
};

/**
 * @brief Abstract class: Objects can be put into a pipleline. 
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ModifyingStream : public AudioStream {
 public:
  /// Defines/Changes the input & output
  virtual void setStream(Stream& in) = 0;
  /// Defines/Changes the output target
  virtual void setOutput(Print& out) = 0;
};

/**
 * @brief A simple Stream implementation which is backed by allocated memory. 
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class MemoryStream : public AudioStream {
 public:
  // Default constructor
  MemoryStream() = default;
  /// Constructor for alloction in RAM
  MemoryStream(int buffer_size, MemoryType memoryType) {
    LOGD("MemoryStream: %d", buffer_size);
    this->buffer_size = buffer_size;
    this->memory_type = memoryType;
    resize(buffer_size);
    info.clear(); // mark audio info as unknown
  }

  /// Constructor for data from Progmem, active is set to true automatically by default.
  MemoryStream(const uint8_t *buffer, int buffer_size, bool isActive=true, MemoryType memoryType = FLASH_RAM) {
    LOGD("MemoryStream: %d", buffer_size);
    setValue(buffer, buffer_size, memoryType);
    is_active = isActive;
    info.clear(); // mark audio info as unknown
  }

  /// Copy Constructor
  MemoryStream(MemoryStream& source) : AudioStream() {
    copy(source);
  }

  /// Move Constructor
  MemoryStream(MemoryStream&& source) {
      setValue(source.buffer, source.buffer_size, source.memory_type);
      // clear source data
      source.setValue(nullptr, 0, source.memory_type);
  }

  ~MemoryStream() {
    TRACED();
    if (memoryCanChange() && buffer!=nullptr) free(buffer);
  }

  /// copy assignement operator
  MemoryStream& operator=(MemoryStream& other) {
    copy(other);
    return *this;
  }

  /// Returns true if there is still some more data
  operator bool() override { return available() > 0; }

  /// Define some audio info and start the processing
  bool begin(AudioInfo info){
    this->info = info;
    return begin();
  }

  /// resets the read pointer
  bool begin() override {
    TRACED();
    write_pos = memoryCanChange() ? 0 : buffer_size;
    if (this->buffer==nullptr && memoryCanChange()){
      resize(buffer_size);
    }
    read_pos = 0;
    is_active = true;
    return true;
  }

  virtual size_t write(uint8_t byte) override {
    if (!is_active) return 0;
    if (memory_type == FLASH_RAM) return 0;
    if (buffer==nullptr) return 0;
    int result = 0;
    if (write_pos < buffer_size) {
      result = 1;
      buffer[write_pos] = byte;
      write_pos++;
    }
    return result;
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    if (!is_active) return 0;
    if (memory_type == FLASH_RAM) return 0;
    size_t result = 0;
    for (size_t j = 0; j < len; j++) {
      if (!write(data[j])) {
        break;
      }
      result = j + 1;
    }
    return result;
  }

  virtual int available() override { 
    if (!is_active) return 0;
    if (buffer==nullptr) return 0;
    int result = write_pos - read_pos;
    if (result<=0 && is_loop){
      // rewind to start
      read_pos = rewind_pos;
      result = write_pos - read_pos;
      // call callback
      if (rewind!=nullptr) rewind();
    }
    return is_loop ? DEFAULT_BUFFER_SIZE : result;
  }

  virtual int availableForWrite() override {
    if (!is_active) return 0;
    if (memory_type == FLASH_RAM) return 0;
    return buffer_size - write_pos;
  } 

  virtual int read() override {
    int result = peek();
    if (result >= 0) {
      read_pos++;
    }
    return result;
  }

  virtual size_t readBytes(uint8_t *data, size_t len) override {
    if (!is_active) return 0;
    size_t count = 0;
    while (count < len) {
      int c = read();
      if (c < 0) break;
      *data++ = (char)c;
      count++;
    }
    return count;
  }

  virtual int peek() override {
    if (!is_active) return -1;
    int result = -1;
    if (available() > 0) {
      result = buffer[read_pos];
    }
    return result;
  }

  virtual void flush() override {}

  virtual void end() override {
    read_pos = 0;
    is_active = false;
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

  /// Automatically rewinds to the beginning when reaching the end. For wav files we move to pos 44 to ignore the header!
  virtual void setLoop(bool loop){
    is_loop = loop;
    rewind_pos = 0;
    if (buffer!=nullptr && buffer_size > 12){
      if (memcmp("WAVE", buffer+8, 4)==0){
        rewind_pos = 44;
      } 
    }
  }

  /// Automatically rewinds to the indicated position when reaching the end
  virtual void setLoop(bool loop, int rewindPos){
    is_loop = loop;
    rewind_pos = rewindPos;
  }

  /// Resizes the available memory. Returns false for PROGMEM or when allocation failed
  virtual bool resize(size_t size){
    if (!memoryCanChange()) return false;   

    buffer_size = size;
    switch(memory_type){
#if defined(ESP32) && defined(ARDUINO) 
      case PS_RAM:
        buffer = (buffer==nullptr) ? (uint8_t*)ps_calloc(size,1) : (uint8_t*)ps_realloc(buffer, size);
        break;
#endif
      default:
        buffer = (buffer==nullptr) ? (uint8_t*)calloc(size,1) : (uint8_t*)realloc(buffer, size);
        break;
    }
    return buffer != nullptr;
  }

  /// Provides access to the data array
  virtual uint8_t* data(){
    return buffer;
  }

  /// update the write_pos (e.g. when we used data() to update the array)
  virtual void setAvailable(size_t len) {
    this->write_pos = len;
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
  int rewind_pos = 0;
  uint8_t *buffer = nullptr;
  MemoryType memory_type = RAM;
  bool is_loop = false;
  void (*rewind)() = nullptr;
  bool is_active = false;

  bool memoryCanChange() {
    return memory_type!=FLASH_RAM;
  }

  void copy(MemoryStream& source) {
    if (this == &source) return;
    if (source.memory_type == FLASH_RAM){
      setValue(source.buffer, source.buffer_size, source.memory_type);
    } else {
      setValue(nullptr, source.buffer_size, source.memory_type);
      resize(buffer_size);
      memcpy(buffer, source.buffer, buffer_size);
    }
  }
};

/**
 * @brief An AudioStream backed by a Ringbuffer. We can write to the end and read from
 * the beginning of the stream
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RingBufferStream : public AudioStream {
 public:
  RingBufferStream(int size = DEFAULT_BUFFER_SIZE) { resize(size); }

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

  virtual size_t readBytes(uint8_t *data, size_t len) override {
    return buffer.readArray(data, len);
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    // LOGD("RingBufferStream::write: %zu",len);
    return buffer.writeArray(data, len);
  }

  virtual size_t write(uint8_t c) override { return buffer.write(c); }

  void resize(int size) { buffer.resize(size); }

  size_t size() { return buffer.size(); }

 protected:
  RingBuffer<uint8_t> buffer{0};
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

  void setAudioInfo(AudioInfo newInfo) override {
    if (newInfo.bits_per_sample != sizeof(T)*8){
      LOGE("Wrong bits_per_sample: %d", newInfo.bits_per_sample);
    }
    AudioStream::setAudioInfo(newInfo);
  }

  /// start the processing
  bool begin() override {
    TRACED();
    if (generator_ptr==nullptr){
      LOGE("%s",source_not_defined_error);
      return false;
    }
    generator_ptr->begin();
    notifyAudioChange(generator_ptr->audioInfo());
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
    notifyAudioChange(generator_ptr->audioInfo());
    active = true;
    return active;
  }

  /// stop the processing
  void end() override {
    TRACED();
    generator_ptr->end();
    active = true; // legacy support - most sketches do not call begin
  }

  AudioInfo audioInfo() override {
    return generator_ptr->audioInfo();
  }

  /// This is unbounded so we just return the buffer size
  virtual int available() override { return active ? DEFAULT_BUFFER_SIZE*2 : 0; }

  /// privide the data as byte stream
  size_t readBytes(uint8_t *data, size_t len) override {
    if (!active) return 0;
    LOGD("GeneratedSoundStream::readBytes: %u", (unsigned int)len);
    return generator_ptr->readBytes(data, len);
  }

  bool isActive() {return active && generator_ptr->isActive();}

  operator bool() { return isActive(); }

  void flush() override {}

 protected:
  bool active = true; // support for legacy sketches
  SoundGenerator<T> *generator_ptr;
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
class BufferedStream : public ModifyingStream {
 public:
  BufferedStream(size_t buffer_size) {
    TRACED();
    buffer.resize(buffer_size);
  }

  BufferedStream(size_t buffer_size, Print &out) {
    TRACED();
    setOutput(out);
    buffer.resize(buffer_size);
  }

  BufferedStream(size_t buffer_size, Stream &io) {
    TRACED();
    setStream(io);
    buffer.resize(buffer_size);
  }

  void setOutput(Print &out){
    p_out = &out;
  }
  void setStream(Print &out){
    setOutput(out);
  }
  void setStream(Stream &io){
    p_in = &io;
    p_out = &io;
  }

  /// writes a byte to the buffer
  size_t write(uint8_t c) override {
    if (buffer.isFull()) {
      flush();
    }
    return buffer.write(c);
  }

  /// Use this method: write an array
  size_t write(const uint8_t *data, size_t len) override {
    LOGD("%s: %zu", LOG_METHOD, len);
    int result = 0;
    for (int j=0;j<len;j++){
      result += write(data[j]);
    }
    return result;
  }

  /// empties the buffer
  void flush() override                                            {
    // just dump the memory of the buffer and clear it
    if (buffer.available() > 0) {
      writeExt(buffer.address(), buffer.available());
      buffer.reset();
    }
  }

  /// reads a byte - to be avoided
  int read() override {
    if (buffer.isEmpty()) {
      refill();
    }
    return buffer.read();
  }

  /// peeks a byte - to be avoided
  int peek() override{
    if (buffer.isEmpty()) {
      refill();
    }
    return buffer.peek();
  };

  /// Use this method !!
  size_t readBytes(uint8_t *data, size_t len) override {
    if (buffer.isEmpty()) {
      return readExt(data, len);
    } else {
      refill();
      return buffer.readArray(data, len);
    }
  }

  /// Returns the available bytes in the buffer: to be avoided
  int available() override {
    if (buffer.isEmpty()) {
      refill();
    }
    return buffer.available();
  }

  /// Clears all the data in the buffer
  void clear() { buffer.reset(); }

 protected:
  SingleBuffer<uint8_t> buffer;
  Print* p_out = nullptr;
  Stream* p_in = nullptr;

  // refills the buffer with data from i2s
  void refill() {
    size_t result = readExt(buffer.address(), buffer.size());
    buffer.setAvailable(result);
  }

  virtual size_t writeExt(const uint8_t *data, size_t len) {
    return p_out == nullptr ? 0 : p_out->write(data, len);
  }
  virtual size_t readExt(uint8_t *data, size_t len) {
    return p_in == nullptr ? 0 : p_in->readBytes(data, len);
  }
};


/**
 * @brief Both the data of the read or write
 * operations will be converted with the help of the indicated converter.
 * @ingroup transform
 * @tparam T 
 * @param out 
 * @param converter 
 */
template<typename T>
class ConverterStream : public ModifyingStream {

    public:
        ConverterStream() = default;

        ConverterStream(BaseConverter &converter)  {
            setConverter(converter);
        }

        ConverterStream(Stream &stream, BaseConverter &converter)  {
            setConverter(converter);
            setStream(stream);
        }

        ConverterStream(Print &out, BaseConverter &converter)  {
            setConverter(converter);
            setOutput(out);
        }

        void setStream(Stream &stream){
            TRACEI();
            p_stream = &stream;
            p_out = &stream;
        }

        void setOutput(Print &out){
            TRACEI();
            p_out = &out;
        }

        void setConverter(BaseConverter& cnv){
          p_converter = &cnv;
        }

        virtual int availableForWrite() { return p_out->availableForWrite(); }

        virtual size_t write(const uint8_t *data, size_t len) { 
          size_t result = p_converter->convert((uint8_t *)data, len); 
          if (result>0) {
            size_t result_written = p_out->write(data, result);
            return len * result_written / result;
          }
          return 0;
        }

        size_t readBytes(uint8_t *data, size_t len) override {
          if (p_stream==nullptr) return 0;
          size_t result = p_stream->readBytes(data, len);
          return p_converter->convert(data, result); 
        }

        /// Returns the available bytes in the buffer: to be avoided
        virtual int available() override {
          if (p_stream==nullptr) return 0;
          return p_stream->available();
        }

    protected:
        Stream *p_stream = nullptr;
        Print *p_out = nullptr;
        BaseConverter *p_converter;

};

/**
 * @brief Class which measures the thruput
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
 */
class MeasuringStream : public ModifyingStream {
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
      setOutput(print);
      start_time = millis();
      p_logout = logOut;
    }

    MeasuringStream(Stream &stream, int count=10, Print *logOut=nullptr){
      this->count = count;
      this->max_count = count;
      setStream(stream);
      start_time = millis();
      p_logout = logOut;
    }

    /// Defines/Changes the input & output
    void setStream(Stream& io) override {
      p_print = &io; 
      p_stream = &io;
    };

    /// Defines/Changes the output target
    void setOutput(Print& out) override {
      p_print = &out;
    }


        /// Provides the data from all streams mixed together 
    size_t readBytes(uint8_t* data, size_t len) override {
      total_bytes_since_begin += len;
      return measure(p_stream->readBytes(data, len));
    }

    int available()  override {
      return p_stream->available();
    }

    /// Writes raw PCM audio data, which will be the input for the volume control 
    virtual size_t write(const uint8_t *data, size_t len) override {
      total_bytes_since_begin += len;
      return measure(p_print->write(data, len));
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

    bool begin(){
      total_bytes_since_begin = 0;
      ms_at_begin = millis();
      return AudioStream::begin();
    }

    bool begin(AudioInfo info){
      setAudioInfo(info);
      return begin();
    }

    /// Trigger reporting in frames (=samples) per second
    void setFrameSize(int size){
      frame_size = size;
    }

    /// Report in bytes instead of samples
    void setReportBytes(bool flag){
      report_bytes = flag;
    }

    void setName(const char* name){
      this->name = name;
    }

    /// Provides the time in ms since the last call of begin()
    uint32_t timeSinceBegin() {
      return millis() - ms_at_begin;
    }

    /// Provides the total processed bytes since the last call of begin()
    uint32_t bytesSinceBegin() {
      return total_bytes_since_begin;
    }

    /// Provides the estimated runtime in milliseconds for the indicated total 
    uint32_t estimatedTotalTimeFor(uint32_t totalBytes) {
      if (bytesSinceBegin()==0) return 0;
      return timeSinceBegin() / bytesSinceBegin() * totalBytes;
    }

    /// Provides the estimated time from now to the end in ms
    uint32_t estimatedOpenTimeFor(uint32_t totalBytes) {
      if (bytesSinceBegin()==0) return 0;
      return estimatedTotalTimeFor(totalBytes) -timeSinceBegin();
    }

    /// Alternative update method: e.g report actual file positon: returns true if the file position was increased
    bool setProcessedBytes(uint32_t pos) {
      bool is_regular_update = true;
      if (pos < total_bytes_since_begin) {
        begin();
        is_regular_update = false;
      } 
      total_bytes_since_begin = pos;
      return is_regular_update;
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
    bool report_bytes = false;
    const char* name = "";
    uint32_t ms_at_begin = 0;
    uint32_t total_bytes_since_begin = 0;

    size_t measure(size_t len) {
      count--;
      total_bytes+=len;

      if (count<=0){
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
        if (report_bytes || frame_size==0){
          snprintf(msg, 70, "%s ==> Bytes per second: %d", name, bytes_per_second);
        } else {
          snprintf(msg, 70, "%s ==> Samples per second: %d", name, bytes_per_second/frame_size);
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
class ProgressStream : public ModifyingStream {
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
    virtual size_t write(const uint8_t *data, size_t len) override {
      if (p_print==nullptr) return 0;
      return measure(p_print->write(data, len));
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
class Throttle : public ModifyingStream {
 public:
  Throttle() = default;
  Throttle(Print &out) { setOutput(out); } 
  Throttle(Stream &io) { setStream(io); } 

  /// Defines/Changes the input & output
  void setStream(Stream& io) override {
    p_out = &io; 
    p_in = &io;
  };

  /// Defines/Changes the output target
  void setOutput(Print& out) override {
    p_out = &out;
  }

  ThrottleConfig defaultConfig() {
    ThrottleConfig c;
    return c;
  }

  bool begin(ThrottleConfig cfg) {
    LOGI("begin sample_rate: %d, channels: %d, bits: %d", (int) info.sample_rate,(int) info.channels, (int)info.bits_per_sample);
    this->info = cfg;
    this->cfg = cfg;
    return begin();
  }

  bool begin(AudioInfo info) {
    LOGI("begin sample_rate: %d, channels: %d, bits: %d", (int)info.sample_rate, (int) info.channels, (int)info.bits_per_sample);
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

  size_t readBytes(uint8_t* data, size_t len) override{
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
    LOGD("wait us: %ld", static_cast<long>(waitUs));
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

    /// Replaces a stream at the indicated channel
    void set(int channel, Stream &in){
      if (channel<size()){
        streams[channel] = &in;
      } else {
        LOGE("Invalid channel %d - max is %d", channel, size()-1);
      }
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
        len = min((int)len, availableBytes());
      }

      int result_len = 0;

      if (len > 0) {
        // result_len must be full frames
        result_len = len * frame_size / frame_size;
        // replace sample based with vector based implementation
        //readBytesSamples((T*)data, result_len));
        result_len = readBytesVector((T*)data, result_len);
      }
      return result_len;
    }

    /// Limit the copy to the available data of all streams: stops to provide data when any stream has ended
    void setLimitToAvailableData(bool flag){
      limit_available_data = flag;
    }

    /// Defines the maximum number of retrys to get data from an input before we abort the read and provide empty data
    void setRetryCount(int retry){
      retry_count = retry;
    }

  protected:
    Vector<Stream*> streams{0};
    Vector<int> weights{0}; 
    int total_weights = 0;
    int frame_size = 4;
    bool limit_available_data = false;
    int retry_count = 5;
    Vector<int> result_vect;
    Vector<T> current_vect;

    /// mixing using a vector of samples
    int readBytesVector(T* p_data, int byteCount) {
      int samples = byteCount / sizeof(T);
      result_vect.resize(samples);
      current_vect.resize(samples);
      int stream_count = size();
      resultClear();
      int samples_eff_max = 0;
      for (int j=0;j<stream_count;j++){
        if (weights[j]>0){
          int samples_eff = readSamples(streams[j],current_vect.data(), samples, retry_count);
          if (samples_eff > samples_eff_max)
            samples_eff_max = samples_eff;
          // if all weights are 0.0 we stop to output
          float factor = total_weights == 0.0f ? 0.0f : static_cast<float>(weights[j]) / total_weights;
          resultAdd(factor);
        }
      }
      // copy result
      for (int j=0;j<samples;j++){
        p_data[j] = result_vect[j];
      }
      return samples_eff_max * sizeof(T);
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
  	  return begin();
    }

    virtual bool begin() {
      // make sure that we use the correct channel count
      info.channels = size();
      return AudioStream::begin();
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
 * @brief CallbackStream: A Stream that allows to register callback methods for accessing and providing data. 
 * The callbacks can be lambda expressions.
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CallbackStream : public ModifyingStream {
  public: 
    CallbackStream() = default;

    /// Allows to change the audio before sending it to the output or before getting it from the original input
    CallbackStream(Stream &io, size_t (*cb_update)(uint8_t* data, size_t len)) {
      p_stream = &io;
      p_out = &io;
      setUpdateCallback(cb_update);
    }

    /// Allows to change the audio before sending it to the output
    CallbackStream(Print &out, size_t (*cb_update)(uint8_t* data, size_t len)) {
      p_out = &out;
      setUpdateCallback(cb_update);
    }

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

    void setUpdateCallback(size_t (*cb_update)(uint8_t* data, size_t len)){
      this->cb_update = cb_update;
    }

    // callback result negative -> no change; callbeack result >=0 provides the result
    void setAvailableCallback(int (*cb)()){
      this->cb_available = cb;
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

    int available() override {
      int result = AudioStream::available();
      // determine value from opional variable
      if (available_bytes>=0) 
        return available_bytes;
      // check if there is a callback  
      if (cb_available==nullptr) 
        return result;
      // determine value from callback
      int tmp_available = cb_available();
      if (tmp_available < 0)
        return result;

      return tmp_available;
    }

    size_t readBytes(uint8_t* data, size_t len) override {
      if (!active) return 0;
      // provide data from callback
      if (cb_read){
        return cb_read(data, len);
      }
      // provide data from source
      size_t result = 0;
      if (p_stream){
        result = p_stream->readBytes(data , len);
      }
      if (cb_update){
        result = cb_update(data, result);
      }
      return result;
    }

    size_t write(const uint8_t* data, size_t len) override {
      if (!active) return 0;
      // write to callback
      if (cb_write){
        return cb_write(data, len);
      }
      // write to output
      if(p_out){
        size_t result = len;
        if (cb_update) {
          result = cb_update((uint8_t*)data, len);
        }
        return p_out->write(data, result);
      }
      // no processing possible  
      return 0;
    }

    /// Defines/Changes the input & output
    void setStream(Stream &in){
        p_stream = &in;
        p_out = &in;
    }

    /// Defines/Changes the output target
    void setOutput(Print &out){
        p_out = &out;
    }
    
    /// same as setStream        
    void setOutput(Stream &in){
        p_stream = &in;
        p_out = &in;
    }

    /// same as set Output
    void setStream(Print &out){
        p_out = &out;
    }

    /// optioinally define available bytes for next read
    void setAvailable(int val){
      available_bytes = val;
    }



  protected:
    bool active=true;
    size_t (*cb_write)(const uint8_t* data, size_t len) = nullptr;
    size_t (*cb_read)(uint8_t* data, size_t len) = nullptr;
    size_t (*cb_update)(uint8_t* data, size_t len) = nullptr;
    int (*cb_available)() = nullptr;
    Stream *p_stream = nullptr;
    Print *p_out = nullptr;
    int available_bytes = -1;
};


/**
 * @brief  Stream to which we can apply Filters for each channel. The filter 
 * might change the result size!
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template<typename T, class TF>
class FilteredStream : public ModifyingStream {
  public:
        FilteredStream() = default;
        FilteredStream(Stream &stream) : ModifyingStream() {
          setStream(stream);
        }
        FilteredStream(Stream &stream, int channels) : ModifyingStream() {
          this->channels = channels;
          setStream(stream);
          p_converter = new ConverterNChannels<T,TF>(channels);
        }
        FilteredStream(Print &stream) : ModifyingStream() {
          setOutput(stream);
        }
        FilteredStream(Print &stream, int channels) : ModifyingStream() {
          this->channels = channels;
          setOutput(stream);
          p_converter = new ConverterNChannels<T,TF>(channels);
        }

        void setStream(Stream &stream){
          p_stream = &stream;
          p_print = &stream;
        }

        void setOutput(Print &stream){
          p_print = &stream;
        }

        bool begin(AudioInfo info){
          setAudioInfo(info);
          this->channels = info.channels;
          if (p_converter !=nullptr  && p_converter->getChannels()!=channels){
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

        virtual size_t write(const uint8_t *data, size_t len) override { 
           if (p_converter==nullptr) return 0;
           size_t result = p_converter->convert((uint8_t *)data, len); 
           return p_print->write(data, result);
        }

        size_t readBytes(uint8_t *data, size_t len) override {
           if (p_converter==nullptr) return 0;
           if (p_stream==nullptr) return 0;
           size_t result = p_stream->readBytes(data, len);
           result = p_converter->convert(data, result); 
           return result;
        }

        virtual int available() override {
           if (p_stream==nullptr) return 0;
          return p_stream->available();
        }

        virtual int availableForWrite() override { 
          return p_print->availableForWrite();
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
        Stream *p_stream = nullptr;
        Print *p_print = nullptr;
        ConverterNChannels<T,TF> *p_converter = nullptr;

};

/**
 * @brief A simple class to determine the volume. You can use it as 
 * final output or as output or input in your audio chain.
 * @ingroup io
 * @ingroup volume
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VolumeMeter : public ModifyingStream {
public:
  VolumeMeter() = default;  
  VolumeMeter(AudioStream& as) {
    addNotifyAudioChange(as);
    setStream(as);
  }
  VolumeMeter(AudioOutput& ao) {
    addNotifyAudioChange(ao);
    setOutput(ao);
  }
  VolumeMeter(Print& print) {
    setOutput(print);
  }
  VolumeMeter(Stream& stream) {
    setStream(stream);
  }

  bool begin(AudioInfo info){
    setAudioInfo(info);
    return begin();
  }

  bool begin() override {
    return true;
  }

  void setAudioInfo(AudioInfo info) {
    ModifyingStream::setAudioInfo(info);
    if (info.channels > 0) {
      volumes.resize(info.channels);
      volumes_tmp.resize(info.channels);
    }
  }

  size_t write(const uint8_t *data, size_t len) {
    updateVolumes(data, len);
    size_t result = len;
    if (p_out!=nullptr){
      result = p_out->write(data, len);
    }
    return result;
  }

  size_t readBytes(uint8_t *data, size_t len){
    if (p_stream==nullptr) return 0;
    size_t result = p_stream->readBytes(data, len);
    updateVolumes((const uint8_t*)data, len);
    return result;
  }

  /// Determines the volume (max amplitude). The range depends on the
  /// bits_per_sample.
  float volume() { return f_volume; }

  /// Determines the volume for the indicated channel. You must call the begin
  /// method to define the number of channels
  float volume(int channel) {
    if (volumes.size() == 0) {
      LOGE("begin not called!");
      return 0.0f;
    }
    if (channel >= volumes.size()) {
      LOGE("invalid channel %d", channel);
      return 0.0f;
    }
    return volumes[channel];
  }


  /// Volume Ratio: max amplitude is 1.0
  float volumeRatio() { return  volume() / NumberConverter::maxValue(info.bits_per_sample);}

  /// Volume Ratio of indicated channel: max amplitude is 1.0
  float volumeRatio(int channel) { return volume(channel) / NumberConverter::maxValue(info.bits_per_sample);}

  /// Volume in db: max amplitude is 0 (range: -1000 to 0)
  float volumeDB() { 
    // prevent infinite value
    if (volumeRatio()==0) return -1000;
    return 20.0f * log10(volumeRatio());
  }

  /// Volume of indicated channel in db: max amplitude is 0
  float volumeDB(int channel) { 
    // prevent infinite value
    if (volumeRatio(channel)==0) return -1000;
    return 20.0f * log10(volumeRatio(channel));
  }

  /// Volume in %: max amplitude is 100
  float volumePercent() { return 100.0f * volumeRatio();}

  /// Volume of indicated channel in %: max amplitude is 100
  float volumePercent(int channel) { return 100.0f * volumeRatio(channel);}


  /// Resets the actual volume
  void clear() {
    f_volume_tmp = 0;
    for (int j = 0; j < info.channels; j++) {
      volumes_tmp[j] = 0;
    }
  }

  void setOutput(Print &out) override {
    p_out = &out;
  }
  void setStream(Stream &io) override {
    p_out = &io;
    p_stream = &io;
  }

protected:
  float f_volume_tmp = 0;
  float f_volume = 0;
  Vector<float> volumes{0};
  Vector<float> volumes_tmp{0};
  Print* p_out = nullptr;
  Stream* p_stream = nullptr;

  void updateVolumes(const uint8_t *data, size_t len){
    clear();
    switch (info.bits_per_sample) {
    case 16:
      updateVolumesT<int16_t>(data, len);
      break;
    case 24:
      updateVolumesT<int24_t>(data, len);
      break;
    case 32:
      updateVolumesT<int32_t>(data, len);
      break;
    default:
      LOGE("Unsupported bits_per_sample: %d", info.bits_per_sample);
      break;
    }
  }

  template <typename T> void updateVolumesT(const uint8_t *buffer, size_t size) {
    T *bufferT = (T *)buffer;
    int samplesCount = size / sizeof(T);
    for (int j = 0; j < samplesCount; j++) {
      float tmp = abs(static_cast<float>(bufferT[j]));
      updateVolume(tmp, j);
    }
    commit();
  }

  void updateVolume(float tmp, int j) {
    if (tmp > f_volume_tmp) {
      f_volume_tmp = tmp;
    }
    if (volumes_tmp.size() > 0 && info.channels > 0) {
      int ch = j % info.channels;
      if (tmp > volumes_tmp[ch]) {
        volumes_tmp[ch] = tmp;
      }
    }
  }

  void commit() {
    f_volume = f_volume_tmp;
    for (int j = 0; j < info.channels; j++) {
      volumes[j] = volumes_tmp[j];
    }
  }
};

// legacy names
using VolumePrint = VolumeMeter;
using VolumeOutput = VolumeMeter;

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
static void  timerCallback(void *obj);
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
  friend void  timerCallback(void *obj);

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

    notifyAudioChange(cfg);
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
        abs((int)currentRateValue - (int)cfg.sample_rate) > 200) {
      cfg.sample_rate = currentRateValue;
      notifyAudioChange(cfg);
    }
  }
};

// relevant only if use_timer == true
void IRAM_ATTR timerCallback(void *obj) {
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
