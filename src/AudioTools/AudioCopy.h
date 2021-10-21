#pragma once

#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTypes.h"
#include "Buffers.h"
#include "Converter.h"
#include "AudioLogger.h"

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
        StreamCopyT(Print &to, Stream &from, int buffer_size=DEFAULT_BUFFER_SIZE){
            LOGD("StreamCopyT")
            begin(to, from);
            this->buffer_size = buffer_size;
            buffer = new uint8_t[buffer_size];
            if (buffer==nullptr){
                LOGE("Could not allocate enough memory for StreamCopy: %d bytes", buffer_size);
            }
        }

        StreamCopyT(int buffer_size=DEFAULT_BUFFER_SIZE){
            LOGD("StreamCopyT")
            this->buffer_size = buffer_size;
            buffer = new uint8_t[buffer_size];
            if (buffer==nullptr){
                LOGE("Could not allocate enough memory for StreamCopy: %d bytes", buffer_size);
            }
        }

        void begin(){     
            is_first = true;       
        }

        // assign a new output and input stream
        void begin(Print &to, Stream &from){
            this->from = &from;
            this->to = &to;
            is_first = true;
        }

        ~StreamCopyT(){
            delete[] buffer;
        }

        // copies the data from one channel from the source to 2 channels on the destination - the result is in bytes
         size_t copy(){
            if (from==nullptr || to == nullptr) 
                return 0;

            size_t result = 0;
            size_t delayCount = 0;
            size_t len = available();
            size_t bytes_to_read=0;
            size_t bytes_read=0; 

            if (len>0){
                bytes_to_read = min(len, static_cast<size_t>(buffer_size));
                size_t samples = bytes_to_read / sizeof(T);
                bytes_to_read = samples * sizeof(T);
                bytes_read = from->readBytes(buffer, bytes_to_read);

                notifyMime(buffer, bytes_to_read);
                is_first = false;

                // callback with unconverted data
                if (onWrite!=nullptr) onWrite(onWriteObj, buffer, result);
                // write data
                result = write(bytes_read, delayCount);
                LOGI("StreamCopy::copy %zu -> %zu -> %zu bytes - in %zu hops", bytes_to_read, bytes_read, result, delayCount);
            } else {

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
                for (int j=0;j<samples;j++){
                    *bufferT = temp_data[j];
                    bufferT++;
                    *bufferT = temp_data[j];
                    bufferT++;
                }
                result = write(samples * sizeof(T)*2, delayCount);
                LOGI("StreamCopy::copy %zu -> %zu bytes - in %d hops", bytes_to_read, result, delayCount);
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

        /// copies all data
        void copyAll(){
            if (from==nullptr || to == nullptr) 
                return;

            while(copy()){
                yield();
                delay(100);
            }
        }

        /// Provides the actual mime type, that was determined from the first available data
        const char* mime() {
            return actual_mime;
        }

        /// Define the callback that will notify about mime changes
        void setMimeCallback(void (*callback)(const char*)){
            this->notifyMimeCallback = callback;
        }

        /// Defines a callback that is notified with the wirtten data
        void setCallbackOnWrite(void (*onWrite)(void*obj, void*buffer, size_t len), void* obj){
            this->onWrite = onWrite;
            this->onWriteObj = obj;
        }

        void setRetry(int retry){
            retryLimit = retry;
        }

    protected:
        Stream *from = nullptr;
        Print *to = nullptr;
        uint8_t *buffer = nullptr;
        int buffer_size;
        void (*onWrite)(void*obj, void*buffer, size_t len) = nullptr;
        void (*notifyMimeCallback)(const char*mime) = nullptr;
        void *onWriteObj = nullptr;
        bool is_first = false;
        const char* actual_mime = nullptr;
        int retryLimit = 20;
        int delay_on_no_data = COPY_DELAY_ON_NODATA;

        // blocking write - until everything is processed
        size_t write(size_t len, size_t &delayCount ){
            size_t total = 0;
            int retry = 0;
            while(total<len){
                size_t written = to->write(buffer+total, len-total);
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

};

/**
 * @brief We provide the typeless StreamCopy as a subclass of StreamCopyT
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StreamCopy : public StreamCopyT<uint8_t> {
    public:
        StreamCopy(int buffer_size=DEFAULT_BUFFER_SIZE): StreamCopyT<uint8_t>(buffer_size) {            
            LOGD("StreamCopy")
        }

        StreamCopy(Print &to, Stream &from, int buffer_size=DEFAULT_BUFFER_SIZE) : StreamCopyT<uint8_t>(to, from, buffer_size){
            LOGD("StreamCopy")
        }

        /// copies a buffer length of data and applies the converter
        template<typename T>
        size_t copy(BaseConverter<T> &converter) {
            size_t result = available();
            size_t delayCount = 0;

            BaseConverter<T> *coverter_ptr = &converter;
            if (result>0){
                size_t bytes_to_read = min(result, static_cast<size_t>(buffer_size) );
                result = from->readBytes(buffer, bytes_to_read);
                // callback with unconverted data
                if (onWrite!=nullptr) onWrite(onWriteObj, buffer, result);

                // convert to pointer to array of 2
                coverter_ptr->convert((T(*)[2])buffer,  result / (sizeof(T)*2) );
                write(result, delayCount);
                LOGI("StreamCopy::copy %zu bytes - in %zu hops", result, delayCount);
            } else {
                // give the processor some time 
                delay(delay_on_no_data);
            }
            return result;
        }
        
        size_t copy() {
            return StreamCopyT<uint8_t>::copy();
        }

        int available() {
            return from->available();
        }


};

} // Namespace