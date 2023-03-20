#pragma once
#if defined(ESP32) && defined(USE_URL_ARDUINO)
#include "AudioConfig.h"
#include "AudioTools/SynchronizedBuffers.h"
#include "AudioTools/AudioStreams.h"
#include "AudioHttp/URLStream.h"

namespace audio_tools {

/**
 * @brief A FreeRTOS task is filling the buffer from the indicated stream. Only to be used on the ESP32
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BufferedTaskStream : public AudioStream {
    public:
        BufferedTaskStream() {
            TRACEI();
        };

        BufferedTaskStream(AudioStream &input){
            TRACEI();
            setInput(input);
        }

        ~BufferedTaskStream(){
            TRACEI();
            stop();
        }

        virtual void begin(bool wait=true)  {
            TRACED();
            active=true;
            ready = false;
            xTaskCreatePinnedToCore(task, "BufferedTaskStream", STACK_SIZE, this, URL_STREAM_PRIORITY, &xHandle, URL_STREAM_CORE);
            if (!wait) ready=true;
        }

        virtual void end()  {
            TRACED();
            if (xHandle!=NULL) vTaskDelete( xHandle );
            active = false;
            ready = false;
        }

        virtual void setInput(AudioStream &input) {
            TRACED();
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
            result = buffers.read();
            return result;
        }

        /// peeks a byte - to be avoided
        virtual int peek() override {
            //if (!ready) return -1;
            int result = -1;
            result = buffers.peek();
            return result;
        };
        
        /// Use this method !!
        virtual size_t readBytes( uint8_t *data, size_t length) override { 
            //if (!ready) return 0;
            size_t result = 0;
            result = buffers.readArray(data, length);
            LOGD("%s: %zu -> %zu", LOG_METHOD, length, result);
            return result;
        }

        /// Returns the available bytes in the buffer: to be avoided
        virtual int available() override {
            //if (!ready) return 0;
            int result = 0;
            result = buffers.available();
            return result;
        }

    protected:
        AudioStream *p_stream=nullptr;
        bool active = false;
        TaskHandle_t xHandle = NULL;
        SynchronizedNBuffer<uint8_t> buffers {DEFAULT_BUFFER_SIZE, URL_STREAM_BUFFER_COUNT};
        bool ready = false;

        static void task(void * pvParameters){
            TRACED();
            static uint8_t buffer[512];
            BufferedTaskStream* self = (BufferedTaskStream*) pvParameters;
            while(true){
                size_t available_to_write = self->buffers.availableForWrite();
                if (*(self->p_stream) && available_to_write>0){
                    size_t to_read = min(available_to_write, (size_t) 512);
                    size_t avail_read = self->p_stream->readBytes((uint8_t*)buffer, to_read);
                    size_t written = self->buffers.writeArray(buffer, avail_read);

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
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class URLStreamBuffered : public AbstractURLStream {
    public:
        URLStreamBuffered(int readBufferSize=DEFAULT_BUFFER_SIZE){
            TRACED();
            urlStream.setReadBufferSize(readBufferSize);
            taskStream.setInput(urlStream);
        }

        URLStreamBuffered(Client &clientPar, int readBufferSize=DEFAULT_BUFFER_SIZE){
            TRACED();
            urlStream.setReadBufferSize(readBufferSize);
            setClient(clientPar);
            taskStream.setInput(urlStream);
        }

        URLStreamBuffered(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            TRACED();
            urlStream.setReadBufferSize(readBufferSize);
            setSSID(network);
            setPassword(password);
            taskStream.setInput(urlStream);
        }

        bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") {
            TRACED();
            // start real stream
            bool result = urlStream.begin(urlStr, acceptMime, action,reqMime, reqData );
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

        void end(){
            TRACED();
            taskStream.end();
            urlStream.end();
        }

        /// provides access to the HttpRequest
        HttpRequest &httpRequest(){
            return urlStream.httpRequest();
        }

        /// (Re-)defines the client
        void setClient(Client &client) override {
            urlStream.setClient(client);
        }

        /// Sets the ssid that will be used for logging in (when calling begin)
        void setSSID(const char* ssid) override{
            urlStream.setSSID(ssid);
        }

        /// Sets the password that will be used for logging in (when calling begin)
        void setPassword(const char* password) override {
            urlStream.setPassword(password);
        }

    protected:
        BufferedTaskStream taskStream;
        URLStream urlStream;
};


}

#endif
