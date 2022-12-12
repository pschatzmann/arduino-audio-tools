#pragma once

#include "AudioHttp/HttpTypes.h" 
#include "AudioHttp/HttpHeader.h" 
#include "AudioHttp/HttpChunkReader.h"
#include "AudioHttp/Url.h"
#include "AudioTools/AudioLogger.h" 

#ifndef URL_CLIENT_TIMEOUT
#define URL_CLIENT_TIMEOUT 60000
#endif

#ifndef URL_HANDSHAKE_TIMEOUT
#define URL_HANDSHAKE_TIMEOUT 120000
#endif

namespace audio_tools {


/**
 * @brief Simple API to process get, put, post, del http requests
 * I tried to use Arduino HttpClient, but I  did not manage to extract the mime
 * type from streaming get requests.
 * 
 * The functionality is based on the Arduino Client class.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class HttpRequest {
    public:
        friend class URLStream;

        HttpRequest() {
             LOGI("HttpRequest");
        }

        HttpRequest(Client &client){
            LOGI("HttpRequest");
            setClient(client);
        }

        void setClient(Client &client){
            this->client_ptr = &client;
            this->client_ptr->setTimeout(clientTimeout);
        }

        // the requests usually need a host. This needs to be set if we did not provide a URL
        void setHost(const char* host){
            LOGI("setHost %s", host);
            this->host_name = host;
        }

        operator bool() {
            return client_ptr != nullptr ? (bool)*client_ptr : false;
        }

        virtual bool connected(){
            return client_ptr != nullptr ? (bool)*client_ptr && client_ptr->connected() : false;
        } 

        virtual int available() {
            if (reply_header.isChunked()){
                return chunk_reader.available();
            }
            return client_ptr != nullptr ? client_ptr->available() : 0;
        }

        virtual void stop(){
            if (connected()){
                LOGI("stop");
                client_ptr->stop();
            }
        }

        virtual int post(Url &url, const char* mime, const char *data, int len=-1){
            LOGI("post %s", url.url());
            return process(POST, url, mime, data, len);
        }

        virtual int put(Url &url, const char* mime, const char *data, int len=-1){
            LOGI("put %s", url.url());
            return process(PUT, url, mime, data, len);
        }

        virtual int del(Url &url,const char* mime=nullptr, const char *data=nullptr, int len=-1) {
            LOGI("del %s", url.url());
            return process(DELETE, url, mime, data, len);
        }

        virtual int get(Url &url,const char* acceptMime=nullptr, const char *data=nullptr, int len=-1) {
            LOGI("get %s", url.url());
            this->accept = acceptMime;
            return process(GET, url, nullptr, data, len);
        }

        virtual int head(Url &url,const char* acceptMime=nullptr, const char *data=nullptr, int len=-1) {
            LOGI("head %s", url.url());
            this->accept = acceptMime;
            return process(HEAD, url, nullptr, data, len);
        }

        // reads the reply data
        virtual int read(uint8_t* str, int len){            
            if (reply_header.isChunked()){
                return chunk_reader.read(*client_ptr, str, len);
            } else {
                return client_ptr->read(str, len);
            }
        }

        // read the reply data up to the next new line. For Chunked data we provide 
        // the full chunk!
        virtual int readln(uint8_t* str, int len, bool incl_nl=true){
            if (reply_header.isChunked()){
                return chunk_reader.readln(*client_ptr, str, len);
            } else {
                return chunk_reader.readlnInternal(*client_ptr, str, len, incl_nl);
            }
        }

        // provides the head information of the reply
        virtual HttpReplyHeader &reply(){
            return reply_header;
        }

        virtual HttpRequestHeader &header(){
            return request_header;
        }

        virtual void setAgent(const char* agent){
            this->agent = agent;
        }

        virtual void setConnection(const char* connection){
            this->connection = connection;
        }

        virtual void setAcceptsEncoding(const char* enc){
            this->accept_encoding = enc;
        }

        virtual void setAcceptMime(const char* mime){
            this->accept = mime;
        }

        size_t getReceivedContentLength() {
            const char *len_str = reply().get(CONTENT_LENGTH);
            int len = 0;
            if (len_str != nullptr) {
                len = atoi(len_str);
            } else {
                LOGW("no CONTENT_LENGTH found in reply");
            }
            return len;
        }

        /// returns true when the request has completed and ready for the data to be requested
        bool isReady() {
            return is_ready;
        }
   
    protected:
        Client *client_ptr;
        Url url;
        HttpRequestHeader request_header;
        HttpReplyHeader reply_header;
        HttpChunkReader chunk_reader = HttpChunkReader(reply_header);
        const char *agent = nullptr;
        const char *host_name=nullptr;
        const char *connection = CON_CLOSE;
        const char *accept = ACCEPT_ALL;
        const char *accept_encoding = nullptr;
        bool is_ready = false;
        int32_t clientTimeout = URL_CLIENT_TIMEOUT; // 60000;

        // opens a connection to the indicated host
        virtual int connect(const char *ip, uint16_t port, int32_t timeout) {
            client_ptr->setTimeout(timeout);
            int is_connected = this->client_ptr->connect(ip, port);
            LOGI("connected %d timeout %d", is_connected, (int) timeout);
            return is_connected;
        }

        // sends request and reads the reply_header from the server
        virtual int process(MethodID action, Url &url, const char* mime, const char *data, int len=-1){
            is_ready = false;
            if (client_ptr==nullptr){
                LOGE("The client has not been defined");
                return -1;
            }
            if (!this->connected()){
                LOGI("process connecting to host %s port %d", url.host(), url.port());
                int is_connected = connect(url.host(), url.port(), clientTimeout);
                if (is_connected!=1){
                    LOGE("Connect failed");
                    return -1;
                }
            } else {
                LOGI("process is already connected");
            }

#ifdef ESP32
            LOGI("Free heap: %u", ESP.getFreeHeap());
#endif

            host_name = url.host();                
            request_header.setValues(action, url.path());
            if (len>0 && data!=nullptr){
                len = strlen(data);
                request_header.put(CONTENT_LENGTH, len);
            }
            request_header.put(HOST_C, host_name);                
            request_header.put(CONNECTION, connection);
            request_header.put(USER_AGENT, agent);
            request_header.put(ACCEPT_ENCODING, accept_encoding);
            request_header.put(ACCEPT, accept);
            request_header.put(CONTENT_TYPE, mime);
            request_header.write(*client_ptr);

            if (len>0){
                LOGI("Writing data: %d bytes", len);
                client_ptr->write((const uint8_t*)data,len);
                LOGD("%s",data);
            }
            
            LOGI("Request written ... waiting for reply")
            client_ptr->flush();
            reply_header.read(*client_ptr);

            // if we use chunked tranfer we need to read the first chunked length
            if (reply_header.isChunked()){
                chunk_reader.open(*client_ptr);
            };

            // wait for data
            is_ready = true;
            return reply_header.statusCode();
        }

};

}
