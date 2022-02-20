#pragma once
#include "AudioConfig.h"
#ifdef USE_URLSTREAM_TASK
#include "AudioTools/AudioStreams.h"
#include "AudioHttp/URLStream.h"

namespace audio_tools {

/**
 * A FreeRTOS task is filling the buffer from the indicated stream.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BufferedTaskStream : public AudioStream {
    public:
        BufferedTaskStream() {
            LOGI(LOG_METHOD);
            createMutex();
        };

        BufferedTaskStream(AudioStream &input){
            LOGI(LOG_METHOD);
            createMutex();
            setInput(input);
        }

        ~BufferedTaskStream(){
            LOGI(LOG_METHOD);
            stop();
        }

        virtual void begin(bool wait=true)  {
            LOGD(LOG_METHOD);
            active=true;
            ready = false;
            xTaskCreatePinnedToCore(task, "BufferedTaskStream", STACK_SIZE, this, URL_STREAM_PRIORITY, &xHandle, URL_STREAM_CORE);
            if (!wait) ready=true;
        }

        virtual void end()  {
            LOGD(LOG_METHOD);
            if (xHandle!=NULL) vTaskDelete( xHandle );
            active = false;
            ready = false;
        }

        virtual void setInput(AudioStream &input) {
            LOGD(LOG_METHOD);
            p_stream = &input;
        }

        /// writes a byte to the buffer
        virtual size_t write(uint8_t c) override {
            return 0;
        }

        /// Use this method: write an array
        virtual size_t write(const uint8_t* data, size_t len) override {    
            return 0;
        }

        /// empties the buffer
        virtual void flush() override {
        }

        /// reads a byte - to be avoided
        virtual int read() override {
            //if (!ready) return -1;
            int result = -1;
            xSemaphoreTake(mutex, portMAX_DELAY);
            result = buffers.read();
            xSemaphoreGive(mutex);
            return result;
        }

        /// peeks a byte - to be avoided
        virtual int peek() override {
            //if (!ready) return -1;
            int result = -1;
            xSemaphoreTake(mutex, portMAX_DELAY);
            result = buffers.peek();
            xSemaphoreGive(mutex);
            return result;
        };
        
        /// Use this method !!
        virtual size_t readBytes( uint8_t *data, size_t length) override { 
            //if (!ready) return 0;
            size_t result = 0;
            xSemaphoreTake(mutex, portMAX_DELAY);
            result = buffers.readArray(data, length);
            xSemaphoreGive(mutex);

            LOGD("%s: %zu -> %zu", LOG_METHOD, length, result);

            return result;
        }

        /// Returns the available bytes in the buffer: to be avoided
        virtual int available() override {
            //if (!ready) return 0;
            int result = 0;
            xSemaphoreTake(mutex, portMAX_DELAY);
            result = buffers.available();
            xSemaphoreGive(mutex);
            return result;
        }

    protected:
        AudioStream *p_stream=nullptr;
        bool active = false;
        TaskHandle_t xHandle = NULL;
        SemaphoreHandle_t mutex = NULL; // Lock access to buffer and Serial
        NBuffer<uint8_t> buffers = NBuffer<uint8_t>(DEFAULT_BUFFER_SIZE, URL_STREAM_BUFFER_COUNT);
        bool ready = false;

        void createMutex() {
            LOGD(LOG_METHOD);
            if (mutex == NULL){
                mutex = xSemaphoreCreateMutex();
                if (mutex==NULL){
                    LOGE("Could not create semaphore");
                }
            }
        }

        static void task(void * pvParameters){
            LOGD(LOG_METHOD);
            static uint8_t buffer[512];
            BufferedTaskStream* self = (BufferedTaskStream*) pvParameters;
            while(true){
                size_t available_to_write = self->buffers.availableForWrite();
                if (*(self->p_stream) && available_to_write>0){
                    size_t to_read = min(available_to_write, (size_t) 512);
                    size_t avail_read = self->p_stream->readBytes((uint8_t*)buffer, to_read);
                    xSemaphoreTake(self->mutex, portMAX_DELAY);
                    size_t written = self->buffers.writeArray(buffer, avail_read);
                    xSemaphoreGive(self->mutex);

                    if (written!=avail_read){
                        LOGE("DATA Lost! %zu reqested, %zu written!",avail_read, written);
                    }

                } else {
                    delay(100);
                }
                // buffer is full we start to provide data
                if (available_to_write==0){
                    self->ready = true;
                }
            }
        }
};

/**
 * @brief URLStream implementation for the ESP32 based on a separate FreeRTOS task 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class URLStream : public AbstractURLStream {
    public:
        URLStream(int readBufferSize=DEFAULT_BUFFER_SIZE){
            LOGD(LOG_METHOD);
            p_urlStream = new URLStreamDefault(readBufferSize);
            taskStream.setInput(*p_urlStream);
        }

        URLStream(Client &clientPar, int readBufferSize=DEFAULT_BUFFER_SIZE){
            LOGD(LOG_METHOD);
            p_urlStream = new URLStreamDefault(clientPar, readBufferSize);
            taskStream.setInput(*p_urlStream);
        }

        URLStream(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            LOGD(LOG_METHOD);
            p_urlStream = new URLStreamDefault(network, password, readBufferSize);
            taskStream.setInput(*p_urlStream);
        }

        ~URLStream(){
            LOGD(LOG_METHOD);
            if (p_urlStream!=nullptr) delete p_urlStream;
        }

        bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") {
            LOGD(LOG_METHOD);
            // start real stream
            bool result = p_urlStream->begin(urlStr, acceptMime, action,reqMime, reqData );
            // start buffer task
            taskStream.begin();
            return result;
        }

        virtual int available() {
            return taskStream.available();
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
            size_t result = taskStream.readBytes(buffer, length);
            LOGD("%s: %zu -> %zu", LOG_METHOD, length, result);
            return result;
        }

        virtual int read() {
            return taskStream.read();
        }

        virtual int peek() {
            return taskStream.peek();
        }

        virtual void flush(){
        }

        size_t write(uint8_t) {
            LOGE("URLStream write - not supported");
            return 0;
        }
    
        size_t write(const uint8_t *buffer, size_t size){
            LOGE("URLStream write - not supported");
            return 0;
        }

        void end(){
            LOGD(LOG_METHOD);
            taskStream.end();
            p_urlStream->end();
        }

        /// provides access to the HttpRequest
        HttpRequest &httpRequest(){
            return p_urlStream->httpRequest();
        }

    protected:
        BufferedTaskStream taskStream;
        URLStreamDefault* p_urlStream=nullptr;

};


}

#endif