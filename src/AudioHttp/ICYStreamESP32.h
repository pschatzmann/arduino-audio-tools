#pragma once
#ifdef XXESP32

#include "AudioHttp/ICYStream.h"

namespace audio_tools {

/**
 * @brief ICYStream implementation for the ESP32 based on a FreeRTOS task 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ICYStream : public AbstractURLStream {
    public:

        ICYStream(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            LOGD(LOG_METHOD);
            p_urlStream = new ICYStreamDefault(network, password, readBufferSize);
            taskStream.setInput(*p_urlStream);
        }

        ~ICYStream(){
            LOGD(LOG_METHOD);
            if (p_urlStream!=nullptr) delete p_urlStream;
        }

        bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") {
            LOGD(LOG_METHOD);
            bool result = p_urlStream->begin(urlStr, acceptMime, action, reqMime, reqData);
            taskStream.begin();
            return result;
        }

        virtual int available() override {
            return taskStream.available();
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length) override { 
            size_t result = taskStream.readBytes(buffer, length);
            LOGD("%s: %zu -> %zu", LOG_METHOD, length, result);
            return result;
        }

        virtual int read() override {
            return taskStream.read();
        }

        virtual int peek() override {
            return taskStream.peek();
        }

        /// Not implemented
        virtual void flush() override {
        }

        /// Not implemented
        virtual size_t write(uint8_t c) override {
            LOGE("N/A");
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
        ICYStreamDefault* p_urlStream=nullptr;

};

}

#endif // ESP32