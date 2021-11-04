#pragma once
#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTypes.h"
#include "Buffers.h"

namespace audio_tools {

/**
 * @brief Base class for all Audio Streams. It support the boolean operator to test if the object
 * is ready with data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioStream : public Stream, public AudioBaseInfoDependent {
    public:
        // overwrite to do something useful
        virtual void setAudioInfo(AudioBaseInfo info) {
	 		LOGD(LOG_METHOD);
            info.logInfo();
        }

        operator bool() {
            return available()>0;
        }
};

/**
 * @brief A simple Stream implementation which is backed by allocated memory
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MemoryStream : public AudioStream {
    public: 
        MemoryStream(int buffer_size = 512){
	 		LOGD("MemoryStream: %d", buffer_size);
            this->buffer_size = buffer_size;
            this->buffer = new uint8_t[buffer_size];
            this->owns_buffer = true;
        }

        MemoryStream(const uint8_t *buffer, int buffer_size){
	 		LOGD("MemoryStream: %d", buffer_size);
            this->buffer_size = buffer_size;
            this->write_pos = buffer_size;
            this->buffer = (uint8_t*)buffer;
            this->owns_buffer = false;
        }

        ~MemoryStream(){
	 		LOGD(LOG_METHOD);
            if (owns_buffer) delete[] buffer;
        }

        // resets the read pointer
        void begin() {
	 		LOGD(LOG_METHOD);
            write_pos = buffer_size;
            read_pos = 0;
        }

        virtual size_t write(uint8_t byte) {
            int result = 0;
            if (write_pos<buffer_size){
                result = 1;
                buffer[write_pos] = byte;
                write_pos++;
            }
            return result;
        }

        virtual size_t write(const uint8_t *buffer, size_t size){
            size_t result = 0;
            for (size_t j=0;j<size;j++){
                if(!write(buffer[j])){
                    break;
                }
                result = j;
            }
            return result;
        }

        virtual int available() {
            return write_pos - read_pos;
        }

        virtual int read() {
            int result = peek();
            if (result>=0){
                read_pos++;
            }
            return result;
        }

        virtual size_t readBytes(char *buffer, size_t length){
            size_t count = 0;
            while (count < length) {
                int c = read();
                if (c < 0) break;
                *buffer++ = (char)c;
                count++;
            }
            return count;
        }


        virtual int peek() {
            int result = -1;
            if (available()>0){
                result = buffer[read_pos];
            }
            return result;
        }

        virtual void flush(){
        }

        virtual void clear(bool reset=false){
            write_pos = 0;
            read_pos = 0;
            if (reset){
                // we clear the buffer data
                memset(buffer,0,buffer_size);
            }
        }

    protected:
        int write_pos = 0;
        int read_pos = 0;
        int buffer_size = 0;
        uint8_t *buffer = nullptr;
        bool owns_buffer=false;
};

/**
 * @brief Source for reading generated tones. Please note 
 * - that the output is for one channel only! 
 * - we do not support reading of individual characters!
 * - we do not support any write operations
 * @param generator 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class T>
class GeneratedSoundStream : public AudioStream, public AudioBaseInfoSource {
    public:
        GeneratedSoundStream(SoundGenerator<T> &generator){
	 		LOGD(LOG_METHOD);
            this->generator_ptr = &generator;
        }

        AudioBaseInfo defaultConfig(){
            return this->generator_ptr->defaultConfig();
        }
        
        /// start the processing
        void begin() {
	 		LOGD(LOG_METHOD);
            generator_ptr->begin();
            if (audioBaseInfoDependent!=nullptr) audioBaseInfoDependent->setAudioInfo(generator_ptr->audioInfo());
            active = true;
        }

        /// start the processing
        void begin(AudioBaseInfo cfg) {
	 		LOGD(LOG_METHOD);
            generator_ptr->begin(cfg);
            if (audioBaseInfoDependent!=nullptr) audioBaseInfoDependent->setAudioInfo(generator_ptr->audioInfo());
            active = true;
        }

        /// stop the processing
        void end() {
	 		LOGD(LOG_METHOD);
            generator_ptr->stop();
            active = false;
        }

        virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi){
            audioBaseInfoDependent = &bi;
        }

        /// unsupported operations
        virtual size_t write(uint8_t) {
            return not_supported();
        };
        /// unsupported operations
        virtual int availableForWrite() {       
            return not_supported();
        }

        /// unsupported operations
        virtual size_t write(const uint8_t *buffer, size_t size) { 
            return not_supported();
        }

        /// unsupported operations
        virtual int read() {
            return -1;
        }        
        /// unsupported operations
        virtual int peek() {
            return -1;
        }
        /// This is unbounded so we just return the buffer size
        virtual int available() {
            return DEFAULT_BUFFER_SIZE;
        }

        /// privide the data as byte stream
        size_t readBytes( char *buffer, size_t length) {
            LOGD("GeneratedSoundStream::readBytes: %zu", length);
            return generator_ptr->readBytes((uint8_t*)buffer, length);
        }

        /// privide the data as byte stream
        size_t readBytes( uint8_t *buffer, size_t length) {
            LOGD("GeneratedSoundStream::readBytes: %zu", length);
            return generator_ptr->readBytes(buffer, length);
        }

        operator bool() {
            return active;
        }

        void flush(){
        }

    protected:
        SoundGenerator<T> *generator_ptr;  
        bool active = false;
        AudioBaseInfoDependent *audioBaseInfoDependent=nullptr;

        int not_supported() {
            LOGE("GeneratedSoundStream-unsupported operation!");
            return 0;
        } 

};

/**
 * @brief The Arduino Stream supports operations on single characters. This is usually not the best way to push audio information, but we 
 * will support it anyway - by using a buffer. On reads: if the buffer is empty it gets refilled - for writes
 * if it is full it gets flushed.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BufferedStream : public AudioStream {
    public:
        BufferedStream(size_t buffer_size){
	 		LOGD(LOG_METHOD);
            buffer = new SingleBuffer<uint8_t>(buffer_size);
        }

        ~BufferedStream() {
	 		LOGD(LOG_METHOD);
            if (buffer!=nullptr){
                delete buffer;
            }
        }

        /// writes a byte to the buffer
        virtual size_t write(uint8_t c) {
        if (buffer->isFull()){
                flush();
            }
            return buffer->write(c);
        }

        /// Use this method: write an array
        virtual size_t write(const uint8_t* data, size_t len) {    
	 		LOGD(LOG_METHOD);
            flush();
            return writeExt(data, len);
        }

        /// empties the buffer
        virtual void flush() {
            // just dump the memory of the buffer and clear it
            if (buffer->available()>0){
                writeExt(buffer->address(), buffer->available());
                buffer->reset();
            }
        }

        /// reads a byte - to be avoided
        virtual int read() {
            if (buffer->isEmpty()){
                refill();
            }
            return buffer->read(); 
        }

        /// peeks a byte - to be avoided
        virtual int peek() {
            if (buffer->isEmpty()){
                refill();
            }
            return buffer->peek();
        };
        
        /// Use this method !!
        size_t readBytes( uint8_t *data, size_t length) { 
            if (buffer->isEmpty()){
                return readExt(data, length);
            } else {
                return buffer->readArray(data, length);
            }
        }

        /// Returns the available bytes in the buffer: to be avoided
        virtual int available() {
            if (buffer->isEmpty()){
                refill();
            }
            return buffer->available();
        }

    protected:
        SingleBuffer<uint8_t> *buffer=nullptr;

        // refills the buffer with data from i2s
        void refill() {
            size_t result = readExt(buffer->address(), buffer->size());
            buffer->setAvailable(result);
        }

        virtual size_t writeExt(const uint8_t* data, size_t len) = 0;
        virtual size_t readExt( uint8_t *data, size_t length) = 0;

};


/**
 * @brief The Arduino Stream which provides silence and simulates a null device when used as audio target
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NullStream : public BufferedStream {
    public:
        NullStream(bool measureWrite=false) : BufferedStream(100){
            is_measure = measureWrite;
        }

        void begin(AudioBaseInfo info, int opt=0) {
        }

        void begin(){
        }

        AudioBaseInfo defaultConfig(int opt=0) {
            AudioBaseInfo info;
            return info;
        }
        
        /// Define object which need to be notified if the basinfo is changing
        void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
        }

        void setAudioInfo(AudioBaseInfo info){
        }

    protected:
        size_t total = 0;
        unsigned long timeout=0;
        bool is_measure;

        virtual size_t writeExt(const uint8_t* data, size_t len) {
            if (is_measure){
                if (millis()<timeout){
                    total += len;
                } else {
                    LOGI("Thruput = %zu kBytes/sec", total/1000);
                    total = 0;
                    timeout = millis()+1000;
                }
            }
            return len;
        }
        virtual size_t readExt( uint8_t *data, size_t len) {
            memset(data, 0, len);
            return len;
        }
};


/**
 * @brief A Stream backed by a Ringbuffer. We can write to the end and read from the beginning of the stream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RingBufferStream : public AudioStream {
    public:
        RingBufferStream(int size=DEFAULT_BUFFER_SIZE) {
            buffer = new RingBuffer<uint8_t>(size);
        }

        ~RingBufferStream(){
            if (buffer!=nullptr){
                delete buffer;
            }
        }

        virtual int available (){
            //LOGD("RingBufferStream::available: %zu",buffer->available());
            return buffer->available();
        }
        
        virtual void flush (){
        }
        
        virtual int peek() {
            return buffer->peek();
        }        
        virtual int read() {
            return buffer->read();
        }
        
        virtual size_t readBytes(uint8_t *data, size_t length) {
            return buffer->readArray(data, length);
        }
        
        virtual size_t write(const uint8_t *data, size_t len){
            //LOGD("RingBufferStream::write: %zu",len);
            return buffer->writeArray(data, len);
        }
        
        virtual size_t 	write(uint8_t c) {
            return buffer->write(c);
        }

    protected:
        RingBuffer<uint8_t> *buffer=nullptr;

};

/**
 * @brief A Stream backed by a SingleBufferStream. We assume that the memory is externally allocated and that we can submit only
 * full buffer records, which are then available for reading.
 *  
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ExternalBufferStream : public AudioStream {
    public:
        ExternalBufferStream() {
	 		LOGD(LOG_METHOD);
        }

        virtual int available (){
            return buffer.available();
        }
        
        virtual void flush (){
        }
        
        virtual int peek() {
            return buffer.peek();
        }   
             
        virtual int read() {
            return buffer.read();
        }
        
        virtual size_t readBytes(uint8_t *data, size_t length) {
            return buffer.readArray(data, length);
        }
        
        virtual size_t write(const uint8_t *data, size_t len){
            buffer.onExternalBufferRefilled((void*)data, len);
            return len;
        }
        
        virtual size_t write(uint8_t c) {
            LOGE("not implemented: %s", LOG_METHOD);
            return 0;
        }

    protected:
        SingleBuffer<uint8_t> buffer;

};



/**
 * @brief AudioOutput class which stores the data in a temporary buffer. 
 * The buffer can be consumed e.g. by a callback function by calling read();

 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T>
class CallbackStream :  public BufferedStream {
    public:
        // Default constructor
        CallbackStream(int bufferSize, int bufferCount ):BufferedStream(bufferSize) {
            callback_buffer_ptr = new NBuffer<T>(bufferSize, bufferCount);
        }

        virtual ~CallbackStream() {    
            delete callback_buffer_ptr;
        }

        /// Activates the output
        virtual bool begin() { 
            active = true;
            return true;
        }
        
        /// stops the processing
        virtual bool stop() {
            active = false;
            return true;
        };
  
  protected:
        NBuffer<T> *callback_buffer_ptr;
        bool active;

        virtual size_t writeExt(const uint8_t* data, size_t len) {    
            return callback_buffer_ptr->writeArray(data, len/sizeof(T));
        }

        virtual size_t readExt( uint8_t *data, size_t len) { 
            return callback_buffer_ptr->readArray(data, len/sizeof(T));;
        }

};

}

