#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "AudioHttp/HttpRequest.h"

namespace audio_tools {

/**
 * @brief Represents the content of a URL as Stream. We use the WiFi.h API
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class URLStream : public Stream {
    public:

        URLStream(int readBufferSize=DEFAULT_BUFFER_SIZE){
            read_buffer = new uint8_t[readBufferSize];
            request.setClient(client);
            // do not store reply header lines
            request.reply().setAutoCreateLines(false);
        }

        URLStream(Client &client, int readBufferSize=DEFAULT_BUFFER_SIZE){
            read_buffer = new uint8_t[readBufferSize];
            request.setClient(client);
            // do not store reply header lines
            request.reply().setAutoCreateLines(false);
        }

        URLStream(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            read_buffer = new uint8_t[readBufferSize];
            this->network = (char*)network;
            this->password = (char*)password;            
            // do not store reply header lines
            request.setClient(client);
            request.reply().setAutoCreateLines(false);
        }

        ~URLStream(){
            delete[] read_buffer;
            end();
        }


        bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") {
            LOGI( "URLStream.begin %s",urlStr);
            
            client.setInsecure();
            url.setUrl(urlStr);
            int result = -1;

            // optional: login if necessary
            login();

            if (acceptMime!=nullptr){
                request.setAcceptMime(acceptMime);
            }
            result = request.process(action, url, reqMime, reqData);
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


    protected:
        HttpRequest request;
        Url url;
        long size;
        long total_read;
        // buffered read
        uint8_t *read_buffer;
        uint16_t read_buffer_size;
        uint16_t read_pos;
        uint16_t read_size;
        bool active = false;
        // optional 
        char* network;
        char* password;
        WiFiClientSecure client;

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
            request.setClient(client);
            delay(500);            
        }

         virtual void waitForData() {
            if(request.available()==0){
                LOGI("Request written ... waiting for reply")
                while(request.available()==0){
                    delay(500);
                }
            }
        }
       

};

}
