#pragma once

#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/Converter.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Typed Stream Copy which supports the conversion from channel to 2 channels. We make sure that we
 * allways copy full samples
 * @tparam T 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T>
class StreamCopyT {
    public:
        StreamCopyT(Print &to, AudioStream &from, int buffer_size=DEFAULT_BUFFER_SIZE){
            LOGD(LOG_METHOD);
            begin(to, from);
            this->buffer_size = buffer_size;
            buffer = new uint8_t[buffer_size];
            if (buffer==nullptr){
                LOGE("Could not allocate enough memory for StreamCopy: %d bytes", buffer_size);
            }
        }

        StreamCopyT(Print &to, Stream &from, int buffer_size=DEFAULT_BUFFER_SIZE){
            LOGD(LOG_METHOD);
            begin(to, from);
            this->buffer_size = buffer_size;
            buffer = new uint8_t[buffer_size];
            if (buffer==nullptr){
                LOGE("Could not allocate enough memory for StreamCopy: %d bytes", buffer_size);
            }
        }

        StreamCopyT(int buffer_size=DEFAULT_BUFFER_SIZE){
            LOGD(LOG_METHOD);
            this->buffer_size = buffer_size;
            buffer = new uint8_t[buffer_size];
            if (buffer==nullptr){
                LOGE("Could not allocate enough memory for StreamCopy: %d bytes", buffer_size);
            }
        }

        void begin(){     
            is_first = true;   
            LOGI("buffer_size=%d",buffer_size);    
        }

        void end() {
            this->from = nullptr;
            this->to = nullptr;
        }

        void begin(Print &to, Stream &from){
            this->from = new AudioStreamWrapper(from);
            this->to = &to;
            is_first = true;
            LOGI("buffer_size=%d",buffer_size);    
        }

        // assign a new output and input stream
        void begin(Print &to, AudioStream &from){
            this->from = &from;
            this->to = &to;
            is_first = true;
            LOGI("buffer_size=%d",buffer_size);    
        }

        Stream *getFrom(){
            return from;
        }

        Stream *getTo() {
            return to;
        }

        ~StreamCopyT(){
            delete[] buffer;
        }

        // copies the data from one channel from the source to 2 channels on the destination - the result is in bytes
        inline size_t copy(){
            LOGD(LOG_METHOD);
            size_t result = 0;
            size_t delayCount = 0;
            size_t len = available();
            size_t bytes_to_read = buffer_size;
            size_t bytes_read = 0; 
            int to_write = to->availableForWrite();

            if (len>0){
                bytes_to_read = min(len, static_cast<size_t>(buffer_size));
                size_t samples = bytes_to_read / sizeof(T);
                bytes_to_read = samples * sizeof(T);
                // don't overflow buffer
                if (to_write>0){
                    bytes_to_read = min((int)bytes_to_read, to_write);
                }

                // get the data now
                bytes_read = from->readBytes((uint8_t*)buffer, bytes_to_read);

                // determine mime
                notifyMime(buffer, bytes_to_read);
                is_first = false;

                // write data
                result = write(bytes_read, delayCount);

                // callback with unconverted data
                if (onWrite!=nullptr) onWrite(onWriteObj, buffer, result);
                #ifndef COPY_LOG_OFF
                LOGI("StreamCopy::copy %u -> %u -> %u bytes - in %u hops", (unsigned int)bytes_to_read,(unsigned int) bytes_read, (unsigned int)result, (unsigned int)delayCount);
                #endif
            } else {
                // give the processor some time 
                delay(delay_on_no_data);
            }
            return result;
        }


        // copies the data from one channel from the source to 2 channels on the destination - the result is in bytes
        size_t copy2(){
            if (from==nullptr || to == nullptr) 
                return 0;
            size_t result = 0;
            size_t delayCount = 0;
            size_t bytes_read;
            size_t len = available();
            size_t bytes_to_read;
            
            if (len>0){
                bytes_to_read = min(len, static_cast<size_t>(buffer_size / 2));
                size_t samples = bytes_to_read / sizeof(T);
                bytes_to_read = samples * sizeof(T);

                T temp_data[samples];
                bytes_read = from->readBytes((uint8_t*)temp_data, bytes_to_read);
                // callback with unconverted data
                if (onWrite!=nullptr) onWrite(onWriteObj, temp_data, bytes_read);

                T* bufferT = (T*) buffer;
                for (size_t j=0;j<samples;j++){
                    *bufferT = temp_data[j];
                    bufferT++;
                    *bufferT = temp_data[j];
                    bufferT++;
                }
                result = write(samples * sizeof(T)*2, delayCount);
                #ifndef COPY_LOG_OFF
                    LOGI("StreamCopy::copy %u -> %u bytes - in %d hops", (unsigned int)bytes_to_read, (unsigned int)result, delayCount);
                #endif
            } else {
                delay(delay_on_no_data);
            }
            return result;
        }

        /// available bytes in the data source
        int available() {
            return from == nullptr ? 0 : from->available();
        }

        /// Defines the dealy that is used if no data is available
        void setDelayOnNoData(int delayMs){
            delay_on_no_data = delayMs;
        }

        /// copies all data - returns true if we copied anything
        bool copyAll(int delayWithDataMs=5, int delayNoDataMs=1000){
            LOGD(LOG_METHOD);
            bool result = false;
            if (from==nullptr || to == nullptr) 
                return result;

            // copy whily source has data available
            size_t available = 1024;
            while(available){
                if (copy()) {
                    result = true;
                    delay(delayWithDataMs);
                } else {
                    delay(delayNoDataMs);
                }
                available = from->available();
            }
            return result;
        }

        /// Provides the actual mime type, that was determined from the first available data
        const char* mime() {
            return actual_mime;
        }

        /// Define the callback that will notify about mime changes
        void setMimeCallback(void (*callback)(const char*)){
            LOGD(LOG_METHOD);
            this->notifyMimeCallback = callback;
        }

        /// Defines a callback that is notified with the wirtten data
        void setCallbackOnWrite(void (*onWrite)(void*obj, void*buffer, size_t len), void* obj){
            LOGD(LOG_METHOD);
            this->onWrite = onWrite;
            this->onWriteObj = obj;
        }

        /// Defines the max number of retries 
        void setRetry(int retry){
            retryLimit = retry;
        }

        /// Provides the buffer size
        int bufferSize() {
            return buffer_size;
        }

    protected:
        AudioStream *from = nullptr;
        Print *to = nullptr;
        uint8_t *buffer = nullptr;
        int buffer_size;
        void (*onWrite)(void*obj, void*buffer, size_t len) = nullptr;
        void (*notifyMimeCallback)(const char*mime) = nullptr;
        void *onWriteObj = nullptr;
        bool is_first = false;
        const char* actual_mime = nullptr;
        int retryLimit = COPY_RETRY_LIMIT;
        int delay_on_no_data = COPY_DELAY_ON_NODATA;

        // blocking write - until everything is processed
        size_t write(size_t len, size_t &delayCount ){
            if (buffer==nullptr) return 0;
            size_t total = 0;
            int retry = 0;
            while(total<len){
                size_t written = to->write((const uint8_t*)buffer+total, len-total);
                total += written;
                delayCount++;

                if (retry++ > retryLimit){
                    LOGE("write to target has failed!");
                    break;
                }
                
                if (retry>1) {
                    delay(5);
                    LOGI("try write - %d ",retry);
                }

            }
            return total;
        }

        /// Update the mime type
        void notifyMime(void* data, size_t len){
            if (len>4) {
                const char *start = (const char *) data;
                actual_mime = "audio/basic";
                if (start[0]==0xFF && start[1]==0xF1){
                    actual_mime = "audio/aac";
                } else if (strncmp(start,"ID3",3) || start[0]==0xFF || start[0]==0xFE ){
                    actual_mime = "audio/mpeg";
                } else if (strncmp(start,"RIFF",4)){
                    actual_mime = "audio/vnd.wave";
                }
                if (notifyMimeCallback!=nullptr){
                    notifyMimeCallback(actual_mime);
                }
            }
        }

};

/**
 * @brief We provide the typeless StreamCopy as a subclass of StreamCopyT
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StreamCopy : public StreamCopyT<uint8_t> {
    public:
        StreamCopy(int buffer_size=DEFAULT_BUFFER_SIZE): StreamCopyT<uint8_t>(buffer_size) {            
             LOGD(LOG_METHOD);
        }

        StreamCopy(Print &to, AudioStream &from, int buffer_size=DEFAULT_BUFFER_SIZE) : StreamCopyT<uint8_t>(to, from, buffer_size){
             LOGD(LOG_METHOD);
        }

        StreamCopy(Print &to, Stream &from, int buffer_size=DEFAULT_BUFFER_SIZE) : StreamCopyT<uint8_t>(to, from, buffer_size){
             LOGD(LOG_METHOD);
        }

        /// copies a buffer length of data and applies the converter
        template<typename T>
        size_t copy(BaseConverter<T> &converter) {
            size_t result = available();
            size_t delayCount = 0;

            BaseConverter<T> *coverter_ptr = &converter;
            if (result>0){
                size_t bytes_to_read = min(result, static_cast<size_t>(buffer_size) );
                result = from->readBytes((uint8_t*)buffer, bytes_to_read);

                // determine mime
                notifyMime(buffer, bytes_to_read);
                is_first = false;

                // callback with unconverted data
                if (onWrite!=nullptr) onWrite(onWriteObj, buffer, result);

                // convert data
                coverter_ptr->convert((uint8_t*)buffer,  result );
                write(result, delayCount);
                #ifndef COPY_LOG_OFF
                    LOGI("StreamCopy::copy %u bytes - in %u hops", (unsigned int)result,(unsigned int) delayCount);
                #endif
            } else {
                // give the processor some time 
                delay(delay_on_no_data);
            }
            return result;
        }
        
        inline size_t copy() {
            return StreamCopyT<uint8_t>::copy();
        }

        int available() {
            return from == nullptr ? 0 : from->available();
        }
};

} // Namespace