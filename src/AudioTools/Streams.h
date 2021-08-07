#pragma once
#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTypes.h"
#include "Buffers.h"
#include "AudioI2S.h"

namespace audio_tools {

/**
 * @brief A simple Stream implementation which is backed by allocated memory
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MemoryStream : public Stream {
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
	 		LOGD(__FUNCTION__);
            if (owns_buffer)
                delete[] buffer;
        }

        // resets the read pointer
        void begin() {
	 		LOGD(__FUNCTION__);
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

        size_t readBytes(char *buffer, size_t length){
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

        void clear(bool reset=false){
            write_pos = 0;
            read_pos = 0;
            if (reset){
                // we clear the buffer data
                memset(buffer,0,buffer_size);
            }
        }


        operator bool() {
            return available()>0;
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
class GeneratedSoundStream : public Stream {
    public:
        GeneratedSoundStream(SoundGenerator<T> &generator){
	 		LOGD(__FUNCTION__);
            this->generator_ptr = &generator;
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

        /// start the processing
        void begin() {
	 		LOGD(__FUNCTION__);
            generator_ptr->begin();
        }

        /// stop the processing
        void end() {
	 		LOGD(__FUNCTION__);
            generator_ptr->stop();
        }

        void flush(){
        }

    protected:
        SoundGenerator<T> *generator_ptr;  

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
class BufferedStream : public Stream {
    public:
        BufferedStream(size_t buffer_size){
            buffer = new SingleBuffer<uint8_t>(buffer_size);
        }

        ~BufferedStream() {
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
 * @brief Stream Wrapper which can be used to print the values as readable ASCII to the screen to be analyzed in the Serial Plotter
 * The frames are separated by a new line. The channels in one frame are separated by a ,
 * @tparam T 
  * @author Phil Schatzmann
 * @copyright GPLv3
*/
template<typename T>
class CsvStream : public BufferedStream, public AudioBaseInfoDependent  {

    public:
        CsvStream(int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) : BufferedStream(buffer_size){
            this->active = active;
        }

        /// Constructor
        CsvStream(Print &out, int channels, int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) : BufferedStream(buffer_size){
            this->channels = channels;
            this->out_ptr = &out;
            this->active = active;
        }

        void begin(){
	 		LOGD(__FUNCTION__);
            this->active = true;
        }

        void begin(AudioBaseInfo info){
	 		LOGD(__FUNCTION__);
            this->active = true;
            this->channels = info.channels;
        }

        void begin(int channels, Print &out=Serial){
	 		LOGD(__FUNCTION__);
            this->channels = channels;
            this->out_ptr = &out;
            this->active = true;
        }


        /// Sets the CsvStream as inactive 
        void end() {
	 		LOGD(__FUNCTION__);
            active = false;
        }

        /// defines the number of channels
        virtual void setAudioInfo(AudioBaseInfo info) {
	 		LOGD(__FUNCTION__);
            this->channels = info.channels;
        };


    protected:
        T *data_ptr;
        Print *out_ptr = &Serial;
        int channels = 1;
        bool active = false;

        virtual size_t writeExt(const uint8_t* data, size_t len) {   
            if (!active) return 0;
            size_t lenChannels = len / (sizeof(T)*channels); 
            data_ptr = (T*)data;
            for (int j=0;j<lenChannels;j++){
                for (int ch=0;ch<channels;ch++){
                    out_ptr->print(*data_ptr);
                    data_ptr++;
                    if (ch<channels-1) Serial.print(", ");
                }
                Serial.println();
            }
            return len;
        }

        virtual size_t readExt( uint8_t *data, size_t length) { 
            LOGE("not implemented: %s", __FUNCTION__);
            return 0;
        }


};

/**
 * @brief Construct a new Encoded Stream object which is supporting defined Audio File types
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioOutputStream : public BufferedStream {
    public:
        AudioOutputStream() : BufferedStream(DEFAULT_BUFFER_SIZE){
            active = true;
        }        

        AudioOutputStream(AudioWriter &writer) : BufferedStream(DEFAULT_BUFFER_SIZE){
            decoder_ptr = &writer;
            active = true;
        }
    
        void begin() {
            active = true;
        }

        void end() {
            active = false;
        }

        operator bool() {
            return active && *decoder_ptr;
        }

    protected:
        AudioWriter *decoder_ptr;
        bool active;

        virtual size_t writeExt(const uint8_t* data, size_t len) {  
            return (decoder_ptr!=nullptr && active) ? decoder_ptr->write(data, len) : 0;
        }

        virtual size_t readExt( uint8_t *data, size_t len) { 
            LOGE("not implemented: %s", __FUNCTION__);
            return 0;
        }
};

/**
 * @brief A Stream backed by a Ringbuffer. We can write to the end and read from the beginning of the stream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RingBufferStream : public Stream {
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
class ExternalBufferStream : public Stream {
    public:
        ExternalBufferStream() {
	 		LOGD(__FUNCTION__);
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
            LOGE("not implemented: %s", __FUNCTION__);
        }

    protected:
        SingleBuffer<uint8_t> buffer;

};


/**
 * @brief A more natural Stream class to process encoded data (aac, wav, mp3...).
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncodedAudioStream : public Stream { 
    public: 
        /**
         * @brief Construct a new Encoded Stream object - used for decoding
         * 
         * @param outputStream 
         * @param decoder 
         */
        EncodedAudioStream(Stream &outputStream, AudioDecoder &decoder) {
	 		LOGD(__FUNCTION__);
            decoder_ptr = &decoder;
            decoder_ptr->setOutputStream(outputStream);
            writer_ptr = decoder_ptr;
            active = false;
        }

        /**
         * @brief Construct a new Encoded Audio Stream object - used for decoding
         * 
         * @param inputStream 
         * @param decoder 
         */
        EncodedAudioStream(Stream &outputStream, AudioDecoder *decoder) {
	 		LOGD(__FUNCTION__);
            decoder_ptr = decoder;
            decoder_ptr->setOutputStream(outputStream);
            writer_ptr = decoder_ptr;
            active = false;
        }

        /**
         * @brief Construct a new Encoded Audio Stream object - used for encoding
         * 
         * @param outputStream 
         * @param encoder 
         */
        EncodedAudioStream(Stream &outputStream, AudioEncoder &encoder) {
	 		LOGD(__FUNCTION__);
            encoder_ptr = &encoder;
            encoder_ptr->setOutputStream(outputStream);
            writer_ptr = encoder_ptr;
            active = false;
        }

        /**
         * @brief Construct a new Encoded Audio Stream object - used for encoding
         * 
         * @param outputStream 
         * @param encoder 
         */
        EncodedAudioStream(Stream &outputStream, AudioEncoder *encoder) {
	 		LOGD(__FUNCTION__);
            encoder_ptr = encoder;
            encoder_ptr->setOutputStream(outputStream);
            writer_ptr = encoder_ptr;
            active = false;
        }

        /**
         * @brief Destroy the Encoded Audio Stream object
         * 
         */
        ~EncodedAudioStream(){
            if (write_buffer!=nullptr){
                delete [] write_buffer;
            }
        }

        /// Define object which need to be notified if the basinfo is changing
        void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
	 		LOGD(__FUNCTION__);
            decoder_ptr->setNotifyAudioChange(bi);
        }

        /// Starts the processing - sets the status to active
        void begin() {
	 		LOGD(__FUNCTION__);
            decoder_ptr->begin();
            encoder_ptr->begin();
            active = true;
        }

        /// Ends the processing
        void end() {
	 		LOGD(__FUNCTION__);
            decoder_ptr->end();
            encoder_ptr->end();
            active = false;
        }

        /// note: always return 0!
        virtual int available(){
            return 0;
        }
        
        /// writes out any buffered data
        virtual void flush (){
            if (write_buffer!=nullptr){
                write(write_buffer, write_buffer_pos);
                write_buffer_pos = 0;
            }
        }
        
        /// not implemented
        virtual int peek() {
            return -1;
        }     

        /// not implemented
        virtual int read() {
            return -1;
        }
        
        /// not implemented
        virtual size_t readBytes(uint8_t *data, size_t length) {
            return 0;
        }
        
        /// encode the data
        virtual size_t write(const uint8_t *data, size_t len){
            return writer_ptr!=nullptr ? writer_ptr->write(data,len) : 0;
        }
        
        /// encode the data
        virtual size_t write(uint8_t c) {
            // this method is usually not used - so we allocate memory only on request
            if (write_buffer == nullptr){
                write_buffer = new uint8_t[write_buffer_size];
            }
            // add the char to the buffer
            write_buffer[write_buffer_pos++] = c;

            // if the buffer is full, we flush
            if (write_buffer_pos==write_buffer_size){
                flush();
            }
            return 0;
        }

        /// Returns true if status is active and we still have data to be processed
        operator bool() {
            return active;
        }

        /// Provides the initialized decoder
        AudioDecoder &decoder() {
            return *decoder_ptr;
        }

        /// Provides the initialized encoder
        AudioEncoder &encoder() {
            return *encoder_ptr;
        }

    protected:
        //ExternalBufferStream ext_buffer; 
        AudioDecoder *decoder_ptr = CodecNOP::instance();  // decoder
        AudioEncoder *encoder_ptr = CodecNOP::instance();  // decoder
        AudioWriter *writer_ptr = nullptr ;

        Stream *input_ptr; // data source for encoded data
        uint8_t *write_buffer = nullptr;
        int write_buffer_pos = 0;
        const int write_buffer_size = 256;
        bool active;
        
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


#ifdef ESP32

/**
 * @brief We support the Stream interface for the ADC class
 * 
 * @tparam T 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AnalogAudioStream : public BufferedStream, public AudioBaseInfoDependent  {

    public:
        AnalogAudioStream() : BufferedStream(DEFAULT_BUFFER_SIZE){
        }

        /// Provides the default configuration
        AnalogConfig defaultConfig(RxTxMode mode) {
            return adc.defaultConfig(mode);
        }

        void begin(AnalogConfig cfg) {
            adc.begin(cfg);
            // unmute
            mute(false);
        }

        void end() {
            mute(true);
            adc.end();
        }

    protected:
        AnalogAudio adc;
        int mute_pin;

        /// set mute pin on or off
        void mute(bool is_mute){
            if (mute_pin>0) {
                digitalWrite(mute_pin, is_mute ? SOFT_MUTE_VALUE : !SOFT_MUTE_VALUE );
            }
        }

        virtual size_t writeExt(const uint8_t* data, size_t len) {
            return adc.writeBytes(data, len);
        }

        virtual size_t readExt( uint8_t *data, size_t length) { 
            return adc.readBytes(data, length);
        }

};

#endif



#ifdef I2S_SUPPORT

/**
 * @brief We support the Stream interface for the I2S access. In addition we allow a separate mute pin which might also be used
 * to drive a LED... 
 * 
 * @tparam T 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class I2SStream : public BufferedStream, public AudioBaseInfoDependent  {

    public:
        I2SStream(int mute_pin=PIN_I2S_MUTE) : BufferedStream(DEFAULT_BUFFER_SIZE){
            this->mute_pin = mute_pin;
            if (mute_pin>0) {
                pinMode(mute_pin, OUTPUT);
                mute(true);
            }
        }

        /// Provides the default configuration
        I2SConfig defaultConfig(RxTxMode mode) {
            return i2s.defaultConfig(mode);
        }

        void begin(I2SConfig cfg) {
            i2s.begin(cfg);
            // unmute
            mute(false);
        }

        void end() {
            mute(true);
            i2s.end();
        }

        /// updates the sample rate dynamically 
        virtual void setAudioInfo(AudioBaseInfo info) {
            I2SConfig cfg = i2s.config();
            if (cfg.sample_rate != info.sample_rate
                || cfg.channels != info.channels
                || cfg.bits_per_sample != info.bits_per_sample) {
                cfg.sample_rate = info.sample_rate;
                cfg.bits_per_sample = info.bits_per_sample;
                cfg.channels = info.channels;

                i2s.end();
                i2s.begin(cfg);        
            }
        }

    protected:
        I2SBase i2s;
        int mute_pin;

        /// set mute pin on or off
        void mute(bool is_mute){
            if (mute_pin>0) {
                digitalWrite(mute_pin, is_mute ? SOFT_MUTE_VALUE : !SOFT_MUTE_VALUE );
            }
        }

        virtual size_t writeExt(const uint8_t* data, size_t len) {    
            return i2s.writeBytes(data, len);
        }

        virtual size_t readExt( uint8_t *data, size_t length) { 
            return i2s.readBytes(data, length);
        }

};

#endif

}

