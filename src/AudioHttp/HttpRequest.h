#pragma once
#include "AudioConfig.h"
#ifdef USE_URL_ARDUINO

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
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class HttpRequest {
    public:
        friend class URLStream;

        HttpRequest() = default;

        ~HttpRequest() {
            HttpHeader::end();
        }

        HttpRequest(Client &client){
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
            return client_ptr == nullptr ? false : client_ptr->connected();
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

        /// http post
        virtual int post(Url &url, const char* mime, const char *data, int len=-1){
            LOGI("post %s", url.url());
            return process(POST, url, mime, data, len);
        }

        /// http post
        virtual int post(Url &url, const char* mime, Stream &data, int len=-1){
            LOGI("post %s", url.url());
            return process(POST, url, mime, data, len);
        }

        /// http put
        virtual int put(Url &url, const char* mime, const char *data, int len=-1){
            LOGI("put %s", url.url());
            return process(PUT, url, mime, data, len);
        }

        /// http put
        virtual int put(Url &url, const char* mime, Stream &data, int len=-1){
            LOGI("put %s", url.url());
            return process(PUT, url, mime, data, len);
        }

        /// http del
        virtual int del(Url &url,const char* mime=nullptr, const char *data=nullptr, int len=-1) {
            LOGI("del %s", url.url());
            return process(DELETE, url, mime, data, len);
        }

        /// http get
        virtual int get(Url &url,const char* acceptMime=nullptr, const char *data=nullptr, int len=-1) {
            LOGI("get %s", url.url());
            this->accept = acceptMime;
            return process(GET, url, nullptr, data, len);
        }

        /// http head
        virtual int head(Url &url,const char* acceptMime=nullptr, const char *data=nullptr, int len=-1) {
            LOGI("head %s", url.url());
            this->accept = acceptMime;
            return process(HEAD, url, nullptr, data, len);
        }

        // reads the reply data
        virtual int read(uint8_t* str, int len){
            TRACED();            
            if (reply_header.isChunked()){
                return chunk_reader.read(*client_ptr, str, len);
            } else {
                return client_ptr->read(str, len);
            }
        }

        size_t readBytesUntil(char terminator, char *buffer, size_t length){
            return client_ptr->readBytesUntil(terminator, buffer, length);
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

        /// provides access to the request header
        virtual HttpRequestHeader &header(){
            return request_header;
        }

        /// Defines the agent
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

        size_t contentLength() {
            const char *len_str = reply().get(CONTENT_LENGTH);
            int len = 0;
            if (len_str != nullptr) {
                len = atoi(len_str);
            } else {
                LOGI("no CONTENT_LENGTH found in reply");
            }
            return len;
        }

        /// returns true when the request has completed and ready for the data to be requested
        bool isReady() {
            return is_ready;
        }

        /// Adds/Updates a request header
        void addRequestHeader(const char* header, const char* value){
            request_header.put(header, value);
        }

        Client &client() {
            return *client_ptr;
        }

        // process http request and reads the reply_header from the server
        virtual int process(MethodID action, Url &url, const char* mime, const char *data, int lenData=-1){
            int len = lenData;
            if (data!=nullptr && len<=0){
                len = strlen(data);
            }
            processBegin(action, url, mime, len);
            // posting data parameter
            if (len>0 && data!=nullptr){
                LOGI("Writing data: %d bytes", len);
                client_ptr->write((const uint8_t*)data, len);
                LOGD("%s",data);
            }
            return processEnd();
        }

        // process http request and reads the reply_header from the server
        virtual int process(MethodID action, Url &url, const char* mime, Stream &stream, int len=-1){
            if (len==-1){
                len = stream.available();
            }
            processBegin(action, url, mime, len);
            processPost(stream);
            return processEnd();
        }

        /// starts http request processing
        virtual bool processBegin(MethodID action, Url &url, const char* mime, int lenData=-1){
            TRACED();
            int len = lenData;
            is_ready = false;
            if (client_ptr==nullptr){
                LOGE("The client has not been defined");
                return false;
            }
            if (http_connect_callback){
                http_connect_callback(*this, url, request_header);
            }
            if (!this->connected()){
                LOGI("process connecting to host %s port %d", url.host(), url.port());
                int is_connected = connect(url.host(), url.port(), clientTimeout);
                if (!is_connected){
                    LOGE("Connect failed");
                    return false;
                }
            } else {
                LOGI("process is already connected");
            }

#if defined(ESP32) && defined(ARDUINO)
            LOGI("Free heap: %u", ESP.getFreeHeap());
#endif

            reply_header.setProcessed();


            host_name = url.host();                
            request_header.setValues(action, url.path());
            if (lenData>0){
                request_header.put(CONTENT_LENGTH, lenData);
            }
            request_header.put(HOST_C, host_name);                
            request_header.put(CONNECTION, connection);
            request_header.put(USER_AGENT, agent);
            request_header.put(ACCEPT_ENCODING, accept_encoding);
            request_header.put(ACCEPT, accept);
            request_header.put(CONTENT_TYPE, mime);
            request_header.write(*client_ptr);

            return true;
        }

        /// Posts the data of the indicated stream after calling processBegin
        virtual void processPost(Stream &stream){
            uint8_t buffer[512];
            int total = 0;
            while(stream.available()>0){
                int result_len = stream.readBytes(buffer, 512);
                total += result_len;
                client_ptr->write(buffer, result_len);
            }
            LOGI("Written data: %d bytes", total);
        }

        /// Write data to the client: can be used to post data after calling processBegin
        virtual size_t write(uint8_t* data, size_t len){
            return client_ptr->write(data, len);
        }

        /// Ends the http request processing and returns the status code
        virtual int processEnd(){
            LOGI("Request written ... waiting for reply")
            //Commented out because this breaks the RP2040 W
            //client_ptr->flush(); 
            reply_header.read(*client_ptr);

            // if we use chunked tranfer we need to read the first chunked length
            if (reply_header.isChunked()){
                chunk_reader.open(*client_ptr);
            };

            // wait for data
            is_ready = true;
            return reply_header.statusCode();
        }

        /// Callback which allows you to add additional paramters dynamically
        void setOnConnectCallback(void (*callback)(HttpRequest &request,Url &url, HttpRequestHeader &request_header)){
            http_connect_callback = callback;
        }

        void setTimeout(int timeout){
            clientTimeout = timeout;
        }

    protected:
        Client *client_ptr = nullptr;
        Url url;
        HttpRequestHeader request_header;
        HttpReplyHeader reply_header;
        HttpChunkReader chunk_reader = HttpChunkReader(reply_header);
        const char *agent = nullptr;
        const char *host_name = nullptr;
        const char *connection = CON_KEEP_ALIVE;
        const char *accept = ACCEPT_ALL;
        const char *accept_encoding = IDENTITY;
        bool is_ready = false;
        int32_t clientTimeout = URL_CLIENT_TIMEOUT; // 60000;
        void (*http_connect_callback)(HttpRequest &request,Url &url, HttpRequestHeader &request_header) = nullptr;

        // opens a connection to the indicated host
        virtual int connect(const char *ip, uint16_t port, int32_t timeout) {
            client_ptr->setTimeout(timeout);
            int is_connected = this->client_ptr->connect(ip, port);
            LOGI("connected %d timeout %d", is_connected, (int) timeout);
            return is_connected;
        }
       
};

}

#endif
