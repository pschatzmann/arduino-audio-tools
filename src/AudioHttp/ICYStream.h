#pragma once
#ifdef USE_URL_ARDUINO

#include "AudioConfig.h"
#include "AudioHttp/URLStreamESP32.h"
#include "AudioMetaData/MetaDataICY.h"

namespace audio_tools {

/**
 * @brief Icecast/Shoutcast Audio Stream which splits the data into metadata and audio data. The Audio data is provided via the
 * regular stream functions. The metadata is handled with the help of the MetaDataICY state machine and provided via a callback method.
 * 
 * This is basically just a URLStream with the metadata turned on.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


class ICYStreamDefault : public AbstractURLStream {
        
    public:
        /// Default constructor
        ICYStreamDefault(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            LOGI(LOG_METHOD);
            url = new URLStreamDefault(network, password, readBufferSize);
            if (url==nullptr){
                LOGE("Not enough memory!");
            }
        }

        ~ICYStreamDefault(){
            LOGI(LOG_METHOD);
            if (url!=nullptr) delete url;
        }

        /// Defines the meta data callback function
        virtual bool setMetadataCallback(void (*fn)(MetaDataType info, const char* str, int len)) override {
            LOGD(LOG_METHOD);
            callback = fn;
            icy.setCallback(fn);
            return true;
        }

        // Icy http get request to the indicated url 
        virtual bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") override {
            LOGD(LOG_METHOD);
            // accept metadata
            url->httpRequest().header().put("Icy-MetaData","1");
            bool result = url->begin(urlStr, acceptMime, action, reqMime, reqData);

            if (result) {
                // setup icy
                ICYUrlSetup icySetup;
                int iceMetaint = icySetup.setup(url->httpRequest());
                // callbacks from http request
                icySetup.executeCallback(callback);
                icy.setIcyMetaInt(iceMetaint);
                icy.begin();

                if (!icy.hasMetaData()){
                    LOGW("url does not provide metadata");
                } 
            }
            return result;
        }

        /// Ends the processing
        virtual void end() override {
            LOGD(LOG_METHOD);
            url->end();
            icy.end();
        }

        /// provides the available method from the URLStreamDefault
        virtual int available() override  {
            return url->available();
        }

        /// reads the audio bytes
        virtual size_t readBytes(uint8_t* buffer, size_t len) override {
            size_t result = 0;
            if (icy.hasMetaData()){
                // get data
                int read = url->readBytes(buffer, len);
                // remove metadata from data
                int pos = 0;
                for (int j=0; j<read; j++){
                    icy.processChar(buffer[j]);
                    if (icy.isData()){
                        buffer[pos++] = buffer[j];
                    }
                }    
                result = pos;            
            } else {
                // fast access if there is no metadata
                result = url->readBytes(buffer, len);
            }
            LOGD("%s: %zu -> %zu", LOG_METHOD, len, result);
            return result;
        }


        /// Not Supported!
        virtual int peek() override {
            LOGE("not supported");
            return -1;
        }

        // Read character and processes using the MetaDataICY state engine
        virtual int read() override {
            int ch = -1;
            if (url==nullptr) return -1;

            // get next data byte 
            do {
                ch = url->read();
                if (ch==-1){
                    break;
                }

                icy.processChar(ch);
            } while (!icy.isData());
            return ch;
        }

        /// not implemented 
        virtual void flush() override {
        }

        /// not implemented
        virtual size_t write(uint8_t) override {
            LOGE("N/A");
            return 0;
        }
        
        /// not implemented
        virtual size_t write(const uint8_t *buffer, size_t size) override {
            LOGE("N/A");
            return 0;
         }

        operator bool() {
            return *url;
        }

        /// provides access to the HttpRequest
        virtual HttpRequest &httpRequest() override {
            return url->httpRequest();
        }


    protected:
        URLStreamDefault *url = nullptr; 
        MetaDataICY icy; // icy state machine
        void (*callback)(MetaDataType info, const char* str, int len)=nullptr;

};

#ifndef USE_URLSTREAM_TASK
/**
 * @brief ICYStream for all environment except ESP32
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ICYStream : public ICYStreamDefault {
    public:
        ICYStream(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE)
        :ICYStreamDefault(network,password,readBufferSize) {            
            LOGI(LOG_METHOD);
        }
};

#endif

} // namespace
#endif