#pragma once
#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTypes.h"
#include "Buffers.h"
#include "AudioI2S.h"

#ifdef ESP32
#include <esp_http_client.h>
#endif

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
            this->buffer_size = buffer_size;
            this->buffer = new uint8_t[buffer_size];
            this->owns_buffer = true;
        }

        MemoryStream(const uint8_t *buffer, int buffer_size){
            this->buffer_size = buffer_size;
            this->write_pos = buffer_size;
            this->buffer = (uint8_t*)buffer;
            this->owns_buffer = false;
        }

        ~MemoryStream(){
            if (owns_buffer)
                delete[] buffer;
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
            for (int j=0;j<size;j++){
                if(!write(buffer[j])){
                    break;
                }
                result = j;
            }
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

    protected:
        int write_pos;
        int read_pos;
        int buffer_size;
        uint8_t *buffer;
        bool owns_buffer;
};

/**
 * @brief Source for reading generated tones. Please note 
 * - that the output is for one channel only! 
 * - we do not support reading of individual characters!
 * - we do not support any write operations
 * @param generator 
 */

template <class T>
class GeneratedSoundStream : public Stream {
    public:
        GeneratedSoundStream(SoundGenerator<T> &generator, uint8_t channels=2){
            this->generator_ptr = &generator;
            this->channels = channels;
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
            return generator_ptr->readBytes((uint8_t*)buffer, length, channels);
        }

        /// start the processing
        void begin() {
            generator_ptr->begin();
        }

        /// stop the processing
        void stop() {
            generator_ptr->stop();
        }

        void flush(){
        }

    protected:
        SoundGenerator<T> *generator_ptr;  
        uint8_t channels; 

        int not_supported() {
            LOGE("GeneratedSoundStream-unsupported operation!");
            return 0;
        } 

};



/**
 * @brief The Arduino Stream supports operations on single characters. This is usually not the best way to push audio information, but we 
 * will support it anyway - by using a buffer. On reads: if the buffer is empty it gets refilled - for writes
 * if it is full it gets flushed.
 * 
 */
class BufferedStream : public Stream {
    public:
        BufferedStream(size_t buffer_size){
            buffer = new SingleBuffer<uint8_t>(buffer_size);
        }

        ~BufferedStream() {
            delete [] buffer;
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

        // refills the buffer with data from i2s
        void refill() {
            size_t result = readExt(buffer->address(), buffer->size());
            buffer->setAvailable(result);
        }

        /// Returns the available bytes in the buffer: to be avoided
        virtual int available() {
            if (buffer->isEmpty()){
                refill();
            }
            return buffer->available();
        }

    protected:
        SingleBuffer<uint8_t> *buffer;

        virtual size_t writeExt(const uint8_t* data, size_t len) = 0;
        virtual size_t readExt( uint8_t *data, size_t length) = 0;


};


/**
 * @brief Stream Wrapper which can be used to print the values as readable ASCII to the screen to be analyzed in the Serial Plotter
 * The frames are separated by a new line. The channels in one frame are separated by a ,
 * @tparam T 
 */
template<typename T>
class CsvStream : public BufferedStream, public AudioBaseInfoDependent  {

    public:
        /// Constructor
        CsvStream(Print &out, int channels, int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) : BufferedStream(buffer_size){
            this->channels = channels;
            this->out_ptr = &out;
            this->active = active;
        }

        /// Sets the CsvStream as active 
        void begin(){
            active = true;
        }

        /// Sets the CsvStream as inactive 
        void end() {
            active = false;
        }

    protected:
        T *data_ptr;
        Print *out_ptr;
        int channels;
        bool active;

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
            return 0;
        }


};


/**
 * @brief We support the Stream interface for the I2S access. In addition we allow a separate mute pin which might also be used
 * to drive a LED... 
 * 
 * @tparam T 
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
        virtual void setAudioBaseInfo(AudioBaseInfo info) {
            bool is_update = false;
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

#ifdef ESP32

/**
 * @brief Represents the content of a URL as Stream. We use the ESP32 ESP HTTP Client API
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class UrlStream : public Stream {
    public:
        UrlStream(int readBufferSize=DEFAULT_BUFFER_SIZE){
            read_buffer = new uint8_t[readBufferSize];
        }

        ~UrlStream(){
            delete[] read_buffer;
            end();
        }

        int begin(const char* url) {
            int result = -1;
            config.url = url;
            config.method = HTTP_METHOD_GET;
            LOGI( "UrlStream.begin %s\n",url);

            // cleanup last begin if necessary
            if (client==nullptr){
                client = esp_http_client_init(&config);
            } else {
                esp_http_client_set_url(client,url);
            }

            client = esp_http_client_init(&config);
            if (client==nullptr){
                LOGE("esp_http_client_init failed");
                return -1;
            }

            int write_buffer_len = 0;
            result = esp_http_client_open(client, write_buffer_len);
            if (result != ESP_OK) {
                LOGE("http_client_open failed");
                return result;
            }
            size = esp_http_client_fetch_headers(client);
            if (size<=0) {
                LOGE("esp_http_client_fetch_headers failed");
                return result;
            }

            LOGI( "Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        
            return result;
        }

        virtual int available() {
            return size - total_read;
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
            size_t read = esp_http_client_read(client, (char*)buffer, length);
            total_read+=read;
            return read;
        }

        virtual int read() {
            fillBuffer();
            total_read++;
            return isEOS() ? -1 :read_buffer[read_pos++];
        }

        virtual int peek() {
            fillBuffer();
            return isEOS() ? -1 : read_buffer[read_pos];
        }

        virtual void flush(){
        }

        size_t write(uint8_t) {
            LOGE("UrlStream write - not supported");
        }

        void end(){
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
        }


    protected:
        esp_http_client_handle_t client;
        esp_http_client_config_t config;
        long size;
        long total_read;
        // buffered read
        uint8_t *read_buffer;
        uint16_t read_buffer_size;
        uint16_t read_pos;
        uint16_t read_size;

        inline void fillBuffer() {
            if (isEOS()){
                // if we consumed all bytes we refill the buffer
                read_size = readBytes(read_buffer,read_buffer_size);
                read_pos = 0;
            }
        }

        inline bool isEOS() {
            return read_pos>=read_size;
        }

};


/**
 * @brief We support the Stream interface for the ADC class
 * 
 * @tparam T 
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

}

