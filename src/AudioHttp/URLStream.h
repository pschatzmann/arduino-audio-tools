#pragma once

#include "AudioConfig.h"
#ifdef USE_URL_ARDUINO

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else 
#include <WiFiClientSecure.h>
#endif
#include "AudioHttp/HttpRequest.h"

namespace audio_tools {

/**
 * @brief Represents the content of a URL as Stream. We use the WiFi.h API
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class URLStreamDefault : public AudioStream {
    public:

        URLStreamDefault(int readBufferSize=DEFAULT_BUFFER_SIZE){
            read_buffer = new uint8_t[readBufferSize];
        }

        URLStreamDefault(Client &clientPar, int readBufferSize=DEFAULT_BUFFER_SIZE){
            read_buffer = new uint8_t[readBufferSize];
            client = &clientPar;
        }

        URLStreamDefault(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            read_buffer = new uint8_t[readBufferSize];
            this->network = (char*)network;
            this->password = (char*)password;            
        }

        ~URLStreamDefault(){
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


        bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") {
            LOGI( "URLStream.begin %s",urlStr);
            
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

        virtual int available() {
            if (!active) return 0;
            return request.available();
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
            if (!active) return -1;

            size_t read = request.read((uint8_t*)buffer, length);
            total_read+=read;
            return read;
        }

        virtual int read() {
            if (!active) return -1;

            fillBuffer();
            total_read++;
            return isEOS() ? -1 :read_buffer[read_pos++];
        }

        virtual int peek() {
            if (!active) return -1;

            fillBuffer();
            return isEOS() ? -1 : read_buffer[read_pos];
        }

        virtual void flush(){
        }

        size_t write(uint8_t) {
            LOGE("URLStream write - not supported");
            return 0;
        }

        void end(){
            active = false;
            request.stop();
        }

        /// provides access to the HttpRequest
        HttpRequest &httpRequest(){
            return request;
        }

        operator bool() {
            return active && request.isReady();
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
        int clientTimeout = 10;

        /// Process the Http request and handle redirects
        int process(MethodID action, Url &url, const char* reqMime, const char *reqData, int len=-1) {
            request.setClient(getClient(url.isSecure()));
            int status_code = request.process(action, url, reqMime, reqData, len);
            // redirect
            if (status_code>=300 && status_code<400){
                const char *redirect_url = request.reply().get(LOCATION);
                LOGW("Rederected to: %s", redirect_url);
                url.setUrl(redirect_url);
                request.setClient(getClient(url.isSecure()));
                status_code = request.process(action, url, reqMime, reqData, len);
            }
            return status_code;
        }

        /// Determines the client 
        Client &getClient(bool isSecure){
            if (client!=nullptr) return *client;
            if (isSecure){
                if (clientSecure==nullptr){
                    clientSecure = new WiFiClientSecure();
                    clientSecure->setInsecure();
                } 
                LOGI("WiFiClientSecure");
                return *clientSecure;
            }
            if (clientInsecure==nullptr){
                clientInsecure = new WiFiClient();
            }
            LOGI("WiFiClient");
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
        }

        /// waits for some data - returns fals if the request has failed
         virtual bool waitForData() {
            if(request.available()==0 ){
                LOGI("Request written ... waiting for reply")
                while(request.available()==0){
                    // stop waiting if we got an error
                    if (request.reply().statusCode()>=300){
                        break;
                    }
                    delay(500);
                }
            }
            return request.available()>0;
        }
};
#ifndef ESP32
class URLStream : public URLStreamDefault {
    public:
        URLStream(int readBufferSize=DEFAULT_BUFFER_SIZE)
        :URLStreamDefault(readBufferSize){
        }

        URLStream(Client &clientPar, int readBufferSize=DEFAULT_BUFFER_SIZE)
        :URLStreamDefault(clientPar, readBufferSize){
        }

        URLStream(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE)
        :URLStreamDefault(network,password,readBufferSize) {            
        }
};

#endif

}

#endif
