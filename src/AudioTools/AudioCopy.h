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
 */
template <class T>
class StreamCopyT {
    public:
        StreamCopyT(Print &to, Stream &from, int buffer_size=DEFAULT_BUFFER_SIZE){
            this->from = &from;
            this->to = &to;
            this->buffer_size = buffer_size;
            buffer = new uint8_t[buffer_size];
            if (buffer==nullptr){
                ESP_LOGE("Could not allocate enough memory for StreamCopy: %d bytes", buffer_size);
            }
        }

        ~StreamCopyT(){
            delete[] buffer;
        }

        // copies the data from one channel from the source to 2 channels on the destination - the result is in bytes
         size_t copy(){
            size_t result = 0;
            size_t delayCount = 0;
            size_t len = available();
            size_t bytes_to_read;
            ESP_LOGI("StreamCopyT::copy -  begin");

            if (len>0){
                bytes_to_read = min(len, static_cast<size_t>(buffer_size));
                size_t samples = bytes_to_read / sizeof(T);
                bytes_to_read = samples * sizeof(T);
                size_t bytes_read = from->readBytes(buffer, bytes_to_read);
                result = write(bytes_read,delayCount);
            } 
            ESP_LOGI("StreamCopy::copy %u -> %u bytes - in %d hops\n", bytes_to_read, result, delayCount);
            return result;
        }


        // copies the data from one channel from the source to 2 channels on the destination - the result is in bytes
        size_t copy2(){
            size_t result = 0;
            size_t delayCount = 0;
            size_t bytes_read;
            size_t len = available();
            size_t bytes_to_read;
            ESP_LOGI("StreamCopyT::copy2 -  begin");
            
            if (len>0){
                bytes_to_read = min(len, static_cast<size_t>(buffer_size / 2));
                size_t samples = bytes_to_read / sizeof(T);
                bytes_to_read = samples * sizeof(T);

                T temp_data[samples];
                bytes_read = from->readBytes((uint8_t*)temp_data, bytes_to_read);

                T* bufferT = (T*) buffer;
                for (int j=0;j<samples;j++){
                    *bufferT = temp_data[j];
                    bufferT++;
                    *bufferT = temp_data[j];
                    bufferT++;
                }
                result = write(samples * sizeof(T)*2, delayCount);
            } 
            ESP_LOGI("StreamCopy::copy %u -> %u bytes - in %d hops", bytes_to_read, result, delayCount);
            return result;
        }

        /// available bytes in the data source
        int available() {
            return from->available();
        }

    protected:
        Stream *from;
        Print *to;
        uint8_t *buffer;
        int buffer_size;

        // blocking write - until everything is processed
        size_t write(size_t len, size_t &delayCount ){
            size_t total = 0;
            while(total<len){
                size_t written = to->write(buffer+total, len-total);
                total += written;
                yield();
                delayCount++;
            }
            return total;
        }

};

/**
 * @brief We provide the typeless StreamCopy as a subclass of StreamCopyT
 * 
 */
class StreamCopy : public StreamCopyT<uint8_t> {
    public:
        StreamCopy(Print &to, Stream &from, int buffer_size=DEFAULT_BUFFER_SIZE) : StreamCopyT<uint8_t>(to, from, buffer_size){
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
                // convert to pointer to array of 2
                coverter_ptr->convert((T(*)[2])buffer,  result / (sizeof(T)*2) );
                write(result, delayCount);
            } 

            ESP_LOGI("StreamCopy::copy %d bytes - in %d hops\n", result, delayCount);
            return result;
        }
        
        size_t copy() {
            StreamCopyT<uint8_t>::copy();
        }

        int available() {
            return from->available();
        }


};

} // Namespace