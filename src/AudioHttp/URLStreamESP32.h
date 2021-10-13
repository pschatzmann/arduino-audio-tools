#pragma once

#ifdef ESP32
#include <esp_http_client.h>
#include "WiFi.h" // ESP32 WiFi include

namespace audio_tools {

/**
 * @brief Represents the content of a URL as Stream. We use the ESP32 ESP HTTP Client API
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class URLStream : public Stream {
    public:
        URLStream(int readBufferSize=DEFAULT_BUFFER_SIZE){
            read_buffer = new uint8_t[readBufferSize];
        }

        URLStream(Client &client, int readBufferSize=DEFAULT_BUFFER_SIZE){
            if (WiFi.status() != WL_CONNECTED){
                LOGW("The network has not been started");
            }
        }

        URLStream(const char* network, const char *password, int readBufferSize=DEFAULT_BUFFER_SIZE) {
            read_buffer = new uint8_t[readBufferSize];
            this->ssid = (char*)network;
            this->password = (char*)password;            
        }

        ~URLStream(){
            delete[] read_buffer;
            end();
        }

        int begin(const char* url, const char* accept=nullptr) {
            int result = -1;
            config.url = url;
            config.method = HTTP_METHOD_GET;
            LOGI( "URLStream.begin %s\n",url);

            if (ssid!=nullptr){
                startWIFI();
            }

            // cleanup last begin if necessary
            if (client==nullptr){
                client = esp_http_client_init(&config);
            } else {
                esp_http_client_set_url(client,url);
            }

            client = esp_http_client_init(&config);
            if (client==nullptr){
                LOGE("esp_http_client_init failed");
                return -1;
            }

            if (accept!=nullptr){
                esp_http_client_set_header(client, "Accept", accept);
            }

            int write_buffer_len = 0;
            result = esp_http_client_open(client, write_buffer_len);
            if (result != ESP_OK) {
                LOGE("http_client_open failed");
                return result;
            }
            size = esp_http_client_fetch_headers(client);
            if (size<=0) {
                LOGE("esp_http_client_fetch_headers failed");
                return result;
            }

            LOGI( "Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        
            return result;
        }

        virtual int available() {
            return size - total_read;
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
            size_t read = esp_http_client_read(client, (char*)buffer, length);
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
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
        }


    protected:
        esp_http_client_handle_t client;
        esp_http_client_config_t config;
        long size;
        long total_read;
        // buffered read
        uint8_t *read_buffer;
        uint16_t read_buffer_size;
        uint16_t read_pos;
        uint16_t read_size;
        const char* ssid = nullptr;
        const char* password = nullptr;


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

        void startWIFI() {
            if (WiFi.status() != WL_CONNECTED){
                WiFi.begin(ssid, password);
                while (WiFi.status() != WL_CONNECTED){
                    delay(500);
                }
            }
        }

};

}

#endif
