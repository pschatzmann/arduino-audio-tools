#pragma once

#include "AudioConfig.h"
#ifdef USE_URL_ARDUINO

#if defined(ESP32)
#  include <Client.h>
#  include <WiFiClientSecure.h>
#  include <esp_wifi.h>
#elif defined(ESP8266)
#  include <ESP8266WiFi.h>
#elif defined(IS_DESKTOP)
#  include <Client.h>
#  include <WiFiClient.h>
typedef WiFiClient WiFiClientSecure;
#endif

#include "AudioHttp/HttpRequest.h"
#include "AudioHttp/AbstractURLStream.h"

#ifndef URL_CLIENT_TIMEOUT
#define URL_CLIENT_TIMEOUT 60000
#endif

#ifndef URL_HANDSHAKE_TIMEOUT
#define URL_HANDSHAKE_TIMEOUT 120000
#endif


namespace audio_tools {

/**
 * @brief Represents the content of a URL as Stream. We use the WiFi.h API
 * @author Phil Schatzmann
 * @ingroup http
 * @copyright GPLv3
 * 
 */
class URLStream : public AbstractURLStream {
    public:

        URLStream(int readBufferSize=DEFAULT_BUFFER_SIZE){
            TRACEI();
            read_buffer_size = readBufferSize;
        }

        URLStream(Client &clientPar, int readBufferSize=DEFAULT_BUFFER_SIZE){
            TRACEI();
            read_buffer_size = readBufferSize;
            client = &clientPar;
        }

        URLStream(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            TRACEI();
            read_buffer_size = readBufferSize;
            this->network = (char*)network;
            this->password = (char*)password;            
        }

        ~URLStream(){
            TRACEI();
#ifdef USE_WIFI_CLIENT_SECURE
            if (clientSecure!=nullptr){
                delete clientSecure;
                clientSecure = nullptr;
            }
#endif
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
            if (!active || !request) return 0;
            return request.available();
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length) override {
            if (!active || !request) return 0;

            size_t read = request.read((uint8_t*)&buffer[0], length);
            total_read+=read;
            return read;
        }

        virtual int read() override {
            if (!active) return -1;
            // lazy allocation since this is rarely used
            read_buffer.resize(read_buffer_size);

            fillBuffer();
            total_read++;
            return isEOS() ? -1 :read_buffer[read_pos++];
        }

        virtual int peek() override {
            if (!active) return -1;
            // lazy allocation since this is rarely used
            read_buffer.resize(read_buffer_size);

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

        /// if set to true, it activates the power save mode which comes at the cost of porformance! - By default this is deactivated. ESP32 Only!
        void setPowerSave(bool ps){
            is_power_save = ps;
        }


    protected:
        HttpRequest request;
        Url url;
        long size;
        long total_read;
        // buffered single byte read
        Vector<uint8_t> read_buffer{0};
        uint16_t read_buffer_size;
        uint16_t read_pos;
        uint16_t read_size;
        bool active = false;
        // optional 
        char* network=nullptr;
        char* password=nullptr;
        Client *client=nullptr;
        Client *clientInsecure=nullptr;
#ifdef USE_WIFI_CLIENT_SECURE
        WiFiClientSecure *clientSecure=nullptr;
#endif
        int clientTimeout = URL_CLIENT_TIMEOUT; // 60000;
        unsigned long handshakeTimeout = URL_HANDSHAKE_TIMEOUT; //120000
        bool is_power_save = false;

        void setTimeouts() {
            // set regular timeout
            getClient(url.isSecure()).setTimeout(clientTimeout/1000); // this is in seconds
        }

        /// Process the Http request and handle redirects
        int process(MethodID action, Url &url, const char* reqMime, const char *reqData, int len=-1) {
            request.setClient(getClient(url.isSecure()));
            // keep icy across redirect requests ?
            const char* icy = request.header().get("Icy-MetaData");

            // set timeout
            setTimeouts();
#ifdef ESP32
            // There is a bug in IDF 4!
            if (clientSecure!=nullptr){
                clientSecure->setHandshakeTimeout(handshakeTimeout);
            }

            // Performance optimization for ESP32
            if (!is_power_save){
                esp_wifi_set_ps(WIFI_PS_NONE);
            }
#endif

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
#ifdef USE_WIFI_CLIENT_SECURE
            if (isSecure){
                if (clientSecure==nullptr){
                    clientSecure = new WiFiClientSecure();
                    clientSecure->setInsecure();
                } 
                LOGI("WiFiClientSecure");
                return *clientSecure;
            }
#endif
#ifdef USE_WIFI
            if (clientInsecure==nullptr){
                clientInsecure = new WiFiClient();
                LOGI("WiFiClient");
            }
            return *clientInsecure;
#else       
            LOGE("Client not set");
            stop();
            return *client; // to avoid compiler warning
#endif
        }

        inline void fillBuffer() {
            if (isEOS()){
                // if we consumed all bytes we refill the buffer
                read_size = readBytes(&read_buffer[0],read_buffer_size);
                read_pos = 0;
            }
        }

        inline bool isEOS() {
            return read_pos>=read_size;
        }

        void login(){
#ifdef USE_WIFI
            LOGD("connectWiFi");
            if (network!=nullptr && password != nullptr && WiFi.status() != WL_CONNECTED){
                WiFi.begin(network, password);
                while (WiFi.status() != WL_CONNECTED){
                    Serial.print(".");
                    delay(500); 
                }
                Serial.println();
            }
            delay(500);  
#else   
            LOGE("login not supported");
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

}

#endif
