#pragma once

#include "AudioConfig.h"
#ifdef USE_URL_ARDUINO

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(IS_DESKTOP)
#include <Client.h>
#include <WiFiClient.h>
typedef WiFiClient WiFiClientSecure;
#else 
#include <Client.h>
#include <WiFiClientSecure.h>
#endif

#include "AudioHttp/HttpRequest.h"
#include "AudioHttp/AbstractURLStream.h"

namespace audio_tools {


/**
 * @brief Represents the content of a URL as Stream. We use the WiFi.h API
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class URLStreamDefault : public AbstractURLStream {
    public:

        URLStreamDefault(int readBufferSize=DEFAULT_BUFFER_SIZE){
            LOGI(LOG_METHOD);
            read_buffer = new uint8_t[readBufferSize];
        }

        URLStreamDefault(Client &clientPar, int readBufferSize=DEFAULT_BUFFER_SIZE){
            LOGI(LOG_METHOD);
            read_buffer = new uint8_t[readBufferSize];
            client = &clientPar;
        }

        URLStreamDefault(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            LOGI(LOG_METHOD);
            read_buffer = new uint8_t[readBufferSize];
            this->network = (char*)network;
            this->password = (char*)password;            
        }

        ~URLStreamDefault(){
            LOGI(LOG_METHOD);
            if (read_buffer!=nullptr){
                delete[] read_buffer;
                read_buffer = nullptr;
            }
            if (clientSecure!=nullptr){
                delete clientSecure;
                clientSecure = nullptr;
            }
            if (clientInsecure!=nullptr){
                delete clientInsecure;
                clientInsecure = nullptr;
            }
            end();
        }


        virtual bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="")  override{
            LOGI( "%s: %s",LOG_METHOD, urlStr);
            
            url.setUrl(urlStr);
            int result = -1;

            // optional: login if necessary
            login();

            //request.reply().setAutoCreateLines(false);
            if (acceptMime!=nullptr){
                request.setAcceptMime(acceptMime);
            }
            result = process(action, url, reqMime, reqData);
            if (result>0){
                size = request.getReceivedContentLength();
                LOGI("size: %d", (int)size);
                if (size>=0){
                    waitForData();
                }
            }
            active = result == 200;
            return active;
        }

        virtual void end() override {
            active = false;
            request.stop();
        }

        virtual int available() override {
            if (!active) return 0;
            return request.available();
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length) override {
            if (!active) return -1;

            size_t read = request.read((uint8_t*)buffer, length);
            total_read+=read;
            return read;
        }

        virtual int read() override {
            if (!active) return -1;

            fillBuffer();
            total_read++;
            return isEOS() ? -1 :read_buffer[read_pos++];
        }

        virtual int peek() override {
            if (!active) return -1;

            fillBuffer();
            return isEOS() ? -1 : read_buffer[read_pos];
        }

        virtual void flush() override {
        }

        virtual size_t write(uint8_t) override {
            LOGE("URLStream write - not supported");
            return 0;
        }

        virtual size_t write(const uint8_t*,size_t) override {
            LOGE("URLStream write - not supported");
            return 0;
        }

        /// provides access to the HttpRequest
        virtual HttpRequest &httpRequest() override {
            return request;
        }

        operator bool() {
            return active && request.isReady();
        }

        /// Defines the client timeout
        virtual void setTimeout(int ms){
            clientTimeout = ms;
         }


    protected:
        HttpRequest request;
        Url url;
        long size;
        long total_read;
        // buffered read
        uint8_t *read_buffer=nullptr;
        uint16_t read_buffer_size;
        uint16_t read_pos;
        uint16_t read_size;
        bool active = false;
        // optional 
        char* network=nullptr;
        char* password=nullptr;
        Client *client=nullptr;
        WiFiClient *clientInsecure=nullptr;
        WiFiClientSecure *clientSecure=nullptr;
        int clientTimeout = 60000;

        /// Process the Http request and handle redirects
        int process(MethodID action, Url &url, const char* reqMime, const char *reqData, int len=-1) {
            request.setClient(getClient(url.isSecure()));
            // keep icy across redirect requests ?
            const char* icy = request.header().get("Icy-MetaData");

            // set timeout
            getClient(url.isSecure()).setTimeout(clientTimeout);
            int status_code = request.process(action, url, reqMime, reqData, len);
            // redirect
            while (request.reply().isRedirectStatus()){
                const char *redirect_url = request.reply().get(LOCATION);
                if (redirect_url!=nullptr) {
                    LOGW("Redirected to: %s", redirect_url);
                    url.setUrl(redirect_url);
                    Client* p_client =  &getClient(url.isSecure()); 
                    p_client->stop();
                    request.setClient(*p_client);
                    if (icy){
                        request.header().put("Icy-MetaData", icy);
                    }
                    status_code = request.process(action, url, reqMime, reqData, len);
                } else {
                    LOGE("Location is null");
                    break;
                }
            }
            return status_code;
        }

        /// Determines the client 
        Client &getClient(bool isSecure){
            if (client!=nullptr) return *client;
            if (isSecure){
                if (clientSecure==nullptr){
#ifndef IS_DESKTOP
                    clientSecure = new WiFiClientSecure();
                    clientSecure->setInsecure();
#else
                    clientSecure = new WiFiClient();
#endif
                } 
                LOGI("WiFiClientSecure");
                return *clientSecure;
            }
            if (clientInsecure==nullptr){
                clientInsecure = new WiFiClient();
                LOGI("WiFiClient");
            }
            return *clientInsecure;
        }

        inline void fillBuffer() {
            if (isEOS()){
                // if we consumed all bytes we refill the buffer
                read_size = readBytes(read_buffer,read_buffer_size);
                read_pos = 0;
            }
        }

        inline bool isEOS() {
            return read_pos>=read_size;
        }

        void login(){
#ifndef IS_DESKTOP
            LOGD("connectWiFi");
            if (WiFi.status() != WL_CONNECTED && network!=nullptr && password != nullptr){
                WiFi.begin(network, password);
                while (WiFi.status() != WL_CONNECTED){
                    Serial.print(".");
                    delay(500); 
                }
                Serial.println();
            }
            delay(500);  
#endif          
        }

        /// waits for some data - returns fals if the request has failed
         virtual bool waitForData() {
            if(request.available()==0 ){
                LOGI("Request written ... waiting for reply")
                while(request.available()==0){
                    // stop waiting if we got an error
                    if (request.reply().statusCode()>=300){
                        LOGE("Error code recieved ... stop waiting for reply");
                        break;
                    }
                    delay(500);
                }
            }
            return request.available()>0;
        }
};

#ifndef USE_URLSTREAM_TASK

/**
 * @brief URLStream implementation for all envionments except ESP32
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class URLStream : public URLStreamDefault {
    public:
        URLStream(int readBufferSize=DEFAULT_BUFFER_SIZE)
        :URLStreamDefault(readBufferSize){
            LOGI(LOG_METHOD);
        }

        URLStream(Client &clientPar, int readBufferSize=DEFAULT_BUFFER_SIZE)
        :URLStreamDefault(clientPar, readBufferSize){
            LOGI(LOG_METHOD);
        }

        URLStream(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE)
        :URLStreamDefault(network,password,readBufferSize) {            
            LOGI(LOG_METHOD);
        }
};

#endif

}

#endif
