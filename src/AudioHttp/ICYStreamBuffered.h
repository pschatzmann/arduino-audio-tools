#pragma once
#ifdef ESP32
#include "AudioConfig.h"
#include "AudioHttp/ICYStream.h"

namespace audio_tools {

/**
 * @brief ICYStream implementation for the ESP32 based on a FreeRTOS task 
 * This is a Icecast/Shoutcast Audio Stream which splits the data into metadata and audio data. The Audio data is provided via the
 * regular stream functions. The metadata is handled with the help of the MetaDataICY state machine and provided via a callback method.
 * 
 * This is basically just a URLStream with the metadata turned on.
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ICYStreamBuffered : public AbstractURLStream {
    public:

        ICYStreamBuffered(int readBufferSize=DEFAULT_BUFFER_SIZE){
            TRACEI();
            p_urlStream = new ICYStream(readBufferSize);
            taskStream.setInput(*p_urlStream);
        }
        
        ICYStreamBuffered(Client &clientPar, int readBufferSize=DEFAULT_BUFFER_SIZE){
            TRACEI();
            p_urlStream = new ICYStream(clientPar, readBufferSize);
            taskStream.setInput(*p_urlStream);
        }

        ICYStreamBuffered(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            TRACEI();
            p_urlStream = new ICYStream(network, password, readBufferSize);
            taskStream.setInput(*p_urlStream);
        }

        ~ICYStreamBuffered(){
            TRACEI();
            if (p_urlStream!=nullptr) delete p_urlStream;
        }

        virtual bool setMetadataCallback(void (*fn)(MetaDataType info, const char* str, int len)) override {
            TRACED();
            return p_urlStream->setMetadataCallback(fn);
        }

        virtual bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") override {
            TRACED();
            // start real stream
            bool result = p_urlStream->begin(urlStr, acceptMime, action, reqMime, reqData);
            // start reader task
            taskStream.begin();
            return result;
        }

        virtual void end() override{
            TRACED();
            taskStream.end();
            p_urlStream->end();
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

        virtual size_t write(const uint8_t*, size_t) override {
            LOGE("N/A");
            return 0;
        }

        /// provides access to the HttpRequest
        virtual HttpRequest &httpRequest() override {
            return p_urlStream->httpRequest();
        }

    protected:
        BufferedTaskStream taskStream;
        ICYStream* p_urlStream=nullptr;

};

}

#endif // ESP32