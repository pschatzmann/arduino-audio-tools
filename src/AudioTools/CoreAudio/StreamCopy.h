#pragma once

#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/BaseConverter.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioMetaData/MimeDetector.h"

#define NOT_ENOUGH_MEMORY_MSG "Could not allocate enough memory: %d bytes"

namespace audio_tools {

/**
 * @brief Typed Stream Copy which supports the conversion from channel to 2 channels. We make sure that we
 * allways copy full samples
 * @ingroup tools
 * @tparam T 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T>
class StreamCopyT {
    public:
        
        StreamCopyT(Print &to, AudioStream &from, int bufferSize=DEFAULT_BUFFER_SIZE){
            TRACED();
            this->buffer_size = bufferSize;
            begin(to, from);
        }

        StreamCopyT(Print &to, Stream &from, int bufferSize=DEFAULT_BUFFER_SIZE){
            TRACED();
            this->buffer_size = bufferSize;
            begin(to, from);
        }

        StreamCopyT(int bufferSize=DEFAULT_BUFFER_SIZE){
            TRACED();
            this->buffer_size = bufferSize;
            begin();
        }

        ~StreamCopyT() {
            end();
        }

        /// (Re)starts the processing
        void begin(){     
            TRACED();
            if (p_mime_detector!=nullptr) {
                p_mime_detector->begin();
            };
            resize(buffer_size);   
            if (buffer){
                LOGI("buffer_size=%d",buffer_size);    
            } else {
                LOGE(NOT_ENOUGH_MEMORY_MSG, buffer_size);
            }
        }

        /// Ends the processing
        void end() {
            this->from = nullptr;
            this->to = nullptr;
        }

        /// assign a new output and input stream
        void begin(Print &to, Stream &from){
            this->from = &from;
            this->to = &to;
            begin();
        }

        /// assign a new output and input stream
        void begin(Print &to, AudioStream &from){
            this->from = &from;
            this->from_audio = &from;
            this->to = &to;
            begin();
        }

        /// Provides a pointer to the copy source. Can be used to check if the source is defined.
        Stream *getFrom(){
            return from;
        }

        /// Provides a pointer to the copy target. Can be used to check if the target is defined.
        Print *getTo() {
            return to;
        }

        /// copies the data from the source to the destination and returns the processed number of bytes
        inline size_t copy() {
            p_converter = nullptr;
            return copyBytes(buffer_size);
        }

        /// copies the data from the source to the destination and applies the converter - the result is the processed number of bytes
        inline size_t copy(BaseConverter &converter) {
            p_converter = &converter;
            return copyBytes(buffer_size);
        }

        /// copies the inicated number of bytes from the source to the destination and returns the processed number of bytes
        inline size_t copyBytes(size_t bytes){
            LOGD("copy %d bytes %s", (int) bytes, log_name);
            if (!active) return 0;
            // if not initialized we do nothing
            if (from==nullptr && to==nullptr) return 0;

            // synchronize AudioInfo
            syncAudioInfo();

            // E.g. if we try to write to a server we might not have any output destination yet
            int to_write = to->availableForWrite();
            if (check_available_for_write && to_write==0){
                 delay(500);
                 return 0;
            }

            // resize copy buffer if necessary
            if (buffer.size() < bytes){
                LOGI("Resize to %d", (int) bytes);
                buffer.resize(bytes);
            }

            size_t result = 0;
            size_t delayCount = 0;
            size_t len = bytes;
            if (check_available) {
                len = available();
            }
            size_t bytes_to_read = bytes;
            size_t bytes_read = 0; 

            if (len > 0){
                bytes_to_read = min(len, static_cast<size_t>(buffer_size));
                // don't overflow buffer
                if (to_write > 0){
                    bytes_to_read = min((int)bytes_to_read, to_write);
                }

                // round to full frames 
                int copy_size = minCopySize();
                if (copy_size > 0){
                    size_t samples = bytes_to_read / minCopySize();
                    bytes_to_read = samples * minCopySize();
                }

                // get the data now
                bytes_read = 0;
                if (bytes_to_read>0){
                    bytes_read = from->readBytes((uint8_t*)&buffer[0], bytes_to_read);
                }

                // determine mime
                if (p_mime_detector != nullptr){
                    p_mime_detector->write(buffer.data(), bytes_to_read);
                }

                // convert data
                if (p_converter!=nullptr) p_converter->convert((uint8_t*)buffer.data(),  bytes_read );

                // write data
                result = write(bytes_read, delayCount);

                // callback with unconverted data
                if (onWrite!=nullptr) onWrite(onWriteObj, &buffer[0], result);

                #ifndef COPY_LOG_OFF
                LOGI("StreamCopy::copy %s %u -> %u -> %u bytes - in %u hops",log_name, (unsigned int)bytes_to_read,(unsigned int) bytes_read, (unsigned int)result, (unsigned int)delayCount);
                #endif
                //TRACED();

                if (result == 0){
                    TRACED();
                    // give the processor some time 
                    delay(delay_on_no_data);
                }

                //TRACED();
                CHECK_MEMORY();
            } else {
                // give the processor some time 
                delay(delay_on_no_data);
                LOGD("no data %s", log_name);
            }
            //TRACED();
            return result;
        }

        /// Copies pages * buffersize samples: returns the processed number of bytes
        size_t copyN(size_t pages){
            if (!active) return 0;
            size_t total=0;
            for (size_t j=0;j<pages;j++){
                total+=copy();
            }
            return total;
        }

        /// Copies audio for the indicated number of milliseconds: note that the resolution is determined by the buffer_size
        size_t copyMs(size_t millis, AudioInfo info){
            if (!active) return 0;
            size_t pages = AudioTime::toBytes(millis, info) / buffer_size;
            return copyN(pages);
        }

        /// copies all data - returns the number of processed bytes
        size_t copyAll(int retryCount=5, int retryWaitMs=200){
            TRACED();
            if (!active) return 0;
            size_t result = 0;
            int retry = 0;

            if (from==nullptr || to == nullptr) 
                return result;

            // copy while source has data available
            int count=0;
            while (true){
                count = copy();
                result += count;
                if (count==0){
                    // wait for more data
                    retry++;
                    delay(retryWaitMs);
                } else {
                    retry = 0; // after we got new data we restart the counting
                }
                // stop the processing if we passed the retry limit
                if (retry>retryCount){
                    break;
                }
            }
            return result;
        }

        /// available bytes of the data source
        int available() {
            int result = 0;
            if (from!=nullptr) {
                if (availableCallback!=nullptr){
                    result = availableCallback((Stream*)from);
                } else {
                    result = from->available();
                }
            } else {
                LOGW("source not defined");
            }
            LOGD("available: %d", result);
            return result;
        }

        /// Defines the dealy that is used if no data is available
        void setDelayOnNoData(int delayMs){
            delay_on_no_data = delayMs;
        }

        /// Defines a callback that is notified with the wirtten data
        void setCallbackOnWrite(void (*onWrite)(void*obj, void*buffer, size_t len), void* obj){
            TRACED();
            this->onWrite = onWrite;
            this->onWriteObj = obj;
        }

        /// Defines a callback that provides the available bytes at the source
        void setAvailableCallback(int (*callback)(Stream*stream)){
            availableCallback = callback;
        }

        /// Defines the max number of retries 
        void setRetry(int retry){
            retryLimit = retry;
        }

        /// Provides the buffer size
        int bufferSize() {
            return buffer_size;
        }

        /// Activates the check that we copy only if available for write returns a value
        void setCheckAvailableForWrite(bool flag){
            check_available_for_write = flag;
        }

        /// Is Available for Write check activated ?
        bool isCheckAvailableForWrite() {
            return check_available_for_write;
        }

        /// Activates the check that we copy only if available returns a value
        void setCheckAvailable(bool flag){
            check_available = flag;
        }

        /// Is Available check activated ?
        bool isCheckAvailable() {
            return check_available;
        }

        /// resizes the copy buffer
        void resize(int len){
            buffer_size = len;
            buffer.resize(buffer_size);
        }

        /// deactivate/activate copy - active by default
        void setActive(bool flag){
            active = flag;
        }

        /// Check if copier is active
        bool isActive(){
            return active;
        }

        /// Defines a name which will be printed in the log to identify the copier
        void setLogName(const char* name){
            log_name = name;
        }

        /// Defines the delay that is added before we retry an incomplete copy
        void setRetryDelay(int delay){
            retry_delay = delay;
        }

        /// Determine frame size
        int minCopySize() {
            if (min_copy_size==0 && from_audio != nullptr){
             AudioInfo info = from_audio->audioInfoOut();
             min_copy_size = info.bits_per_sample / 8 * info.channels;
            }
            return min_copy_size;
        }

        /// Defines the minimum frame size that is used to round the copy size: 0 will automatically try to determine the value
        void setMinCopySize(int size){
            min_copy_size = size;
        }

        /// Activate the synchronization from the AudioInfo form the source to the target
        void setSynchAudioInfo(bool active){
            is_sync_audio_info = active;
        }  
        
        /// Define a mime detector
        void setMimeDetector(MimeDetector &mime){
            p_mime_detector = &mime;
        }

    protected:
        Stream *from = nullptr;
        AudioStream *from_audio = nullptr;
        Print *to = nullptr;
        Vector<uint8_t> buffer{0};
        int buffer_size = DEFAULT_BUFFER_SIZE;
        void (*onWrite)(void*obj, void*buffer, size_t len) = nullptr;
        int (*availableCallback)(Stream*stream)=nullptr;
        void *onWriteObj = nullptr;
        bool check_available_for_write = false;
        bool check_available = true;
        int retryLimit = COPY_RETRY_LIMIT;
        int delay_on_no_data = COPY_DELAY_ON_NODATA;
        bool active = true;
        const char* log_name = "";
        int retry_delay = 10;
        int channels = 0;
        int min_copy_size = 1;
        bool is_sync_audio_info = false;
        AudioInfoSupport *p_audio_info_support = nullptr;
        BaseConverter* p_converter = nullptr;
        MimeDetector* p_mime_detector = nullptr;

        void syncAudioInfo(){
            // synchronize audio info
            if (is_sync_audio_info && from_audio != nullptr && p_audio_info_support != nullptr){
                AudioInfo info_from = from_audio->audioInfoOut();    
                AudioInfo info_to = p_audio_info_support->audioInfo();
                if (info_from != info_to){
                    LOGI("--> StreamCopy: ");
                    p_audio_info_support->setAudioInfo(info_from);
                }
            }
        }

        /// blocking write - until everything is processed
        size_t write(size_t len, size_t &delayCount ){
            if (!buffer || len==0) return 0;
            LOGD("write: %d", (int)len);
            size_t total = 0;
            long open = len;
            int retry = 0;
            while(open > 0){
                size_t written = to->write((const uint8_t*)buffer.data()+total, open);
                LOGD("write: %d -> %d", (int) open, (int) written);
                total += written;
                open -= written;
                delayCount++;

                if (open > 0){
                    // if we still have progress we reset the retry counter
                    if (written>0) retry = 0;

                    // abort if we reached the retry limit
                    if (retry++ > retryLimit){
                        LOGE("write %s to target has failed after %d retries! (%ld bytes)", log_name, retry, open);
                        break;
                    }
                    
                    // wait a bit
                    if (retry>1) {
                        delay(retry_delay);
                        LOGI("try write %s - %d (open %ld bytes) ",log_name, retry, open);
                    }
                }

                CHECK_MEMORY();
            }
            return total;
        }
};

/**
 * @brief We provide the typeless StreamCopy 
 * @ingroup tools
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
using StreamCopy = StreamCopyT<uint8_t>;


} // Namespace