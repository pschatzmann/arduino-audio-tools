#pragma once

#include <WiFi.h>
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
            WiFiClient client;
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
            request.reply().setAutoCreateLines(false);
        }

        ~URLStream(){
            delete[] read_buffer;
            end();
        }

        bool begin(const char* urlStr, MethodID action = GET, const char* reqMime="", const char*reqData="") {
            login();
            Url url(urlStr);
            int result = -1;

            LOGI( "URLStream.begin %s",urlStr);
            result = request.process(action, url, reqMime, reqData);
            size = request.getReceivedContentLength();
            LOGI("size: %d", (int)size);
            if (size>=0){
                waitForData();
            }
            return result == 200;
        }

        virtual int available() {
            return request.available();
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
            size_t read = request.read((uint8_t*)buffer, length);
            total_read+=read;
            return read;
        }

        virtual int read() {
            fillBuffer();
            total_read++;
            return isEOS() ? -1 :read_buffer[read_pos++];
        }

        virtual int peek() {
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
            request.stop();
        }


    protected:
        HttpRequest request;
        long size;
        long total_read;
        // buffered read
        uint8_t *read_buffer;
        uint16_t read_buffer_size;
        uint16_t read_pos;
        uint16_t read_size;
        char* network;
        char* password;
        WiFiClient client;

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
