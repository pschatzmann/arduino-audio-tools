#pragma once
#ifdef ESP32
#include "AudioTools/AudioStreams.h"
#include "AudioHttp/URLStream.h"

namespace audio_tools {

/**
 *  A task is filling the buffer from the indicated stream in a background
 *  task. 
 */
class BufferedTaskStream : public AudioStream {
    public:
        BufferedTaskStream() {
            createMutex();
        };

        BufferedTaskStream(AudioStream &input){
            createMutex();
            setInput(input);
        }

        ~BufferedTaskStream(){
            stop();
        }

        void begin(bool wait=true) {
            LOGD(LOG_METHOD);
            active=true;
            ready = false;
            xTaskCreatePinnedToCore(task, "BufferedTaskStream", STACK_SIZE, this, 2, &xHandle, 0);
            if (!wait) ready=true;
        }

        void end() {
            LOGD(LOG_METHOD);
            vTaskDelete( xHandle );
            active = false;
        }

        void setInput(AudioStream &input){
            LOGD(LOG_METHOD);
            //end();
            p_stream = &input;
            //begin();           
        }

        /// writes a byte to the buffer
        virtual size_t write(uint8_t c) {
            return 0;
        }

        /// Use this method: write an array
        virtual size_t write(const uint8_t* data, size_t len) {    
            return 0;
        }

        /// empties the buffer
        virtual void flush() {
        }

        /// reads a byte - to be avoided
        virtual int read() {
            int result = -1;
            xSemaphoreTake(mutex, portMAX_DELAY);
            result = buffers.read();
            xSemaphoreGive(mutex);
            return result;
        }

        /// peeks a byte - to be avoided
        virtual int peek() {
            int result = -1;
            xSemaphoreTake(mutex, portMAX_DELAY);
            result = buffers.peek();
            xSemaphoreGive(mutex);
            return result;
        };
        
        /// Use this method !!
        size_t readBytes( uint8_t *data, size_t length) { 
            LOGD(LOG_METHOD);
            size_t result = 0;
            xSemaphoreTake(mutex, portMAX_DELAY);
            result = buffers.readArray(data, length);
            xSemaphoreGive(mutex);
            return result;
        }

        /// Returns the available bytes in the buffer: to be avoided
        virtual int available() {
            if (!ready) return 0;
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
                size_t available_to_write = self->buffers.availableToWrite();
                if (*(self->p_stream) && available_to_write>0){
                    size_t to_read = min(self->buffers.availableToWrite(), 512);
                    size_t avail_read = self->p_stream->readBytes((uint8_t*)buffer, to_read);
                    xSemaphoreTake(self->mutex, portMAX_DELAY);
                    self->buffers.writeArray(buffer, avail_read);
                    xSemaphoreGive(self->mutex);
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


class URLStream : public AudioStream {
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

        void begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") {
            LOGD(LOG_METHOD);
            p_urlStream->begin(urlStr, acceptMime, action,reqMime, reqData );
            taskStream.begin();
        }

        virtual int available() {
            return taskStream.available();
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
            LOGD(LOG_METHOD);
            return taskStream.readBytes(buffer, length);
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

        void end(){
            LOGD(LOG_METHOD);
            p_urlStream->end();
            taskStream.end();
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