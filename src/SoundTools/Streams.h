#pragma once

namespace sound_tools {


/**
 * @brief A simple Stream implementation which is backed by allocated memory
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MemoryStream : public Stream {
    public: 
        MemoryStream(int buffer_size = 512){
            this->buffer_size = buffer_size;
            this->buffer = new uint8_t[buffer_size];
            this->owns_buffer = true;
        }

        MemoryStream(const uint8_t *buffer, int buffer_size){
            this->buffer_size = buffer_size;
            this->write_pos = buffer_size;
            this->buffer = (uint8_t*)buffer;
            this->owns_buffer = false;
        }

        ~MemoryStream(){
            if (owns_buffer)
                delete[] buffer;
        }

        virtual size_t write(uint8_t byte) {
            int result = 0;
            if (write_pos<buffer_size){
                result = 1;
                buffer[write_pos] = byte;
                write_pos++;
            }
            return result;
        }

        virtual int available() {
            return write_pos - read_pos;
        }

        virtual int read() {
            int result = peek();
            if (result>0){
                read_pos++;
            }
            return result;
        }

        virtual int peek() {
            int result = -1;
            if (available()>0 && read_pos < write_pos){
                result = buffer[read_pos];
            }
            return result;
        }

        virtual void flush(){
        }

        void clear(bool reset=false){
            write_pos = 0;
            read_pos = 0;
            if (reset){
                // we clear the buffer data
                memset(buffer,0,buffer_size);
            }
        }


    protected:
        int write_pos;
        int read_pos;
        int buffer_size;
        uint8_t *buffer;
        bool owns_buffer;

};

#ifdef ESP32
/**
 * @brief Represents the content of a URL as Stream. We use the ESP32 ESP HTTP Client API
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class UrlStream : public Stream {
    public:
        UrlStream(int readBufferSize=512){
            read_buffer = new uint8_t[readBufferSize];
        }

        ~UrlStream(){
            delete[] read_buffer;
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
        }

        int begin(const char* url) {
            int result = -1;
            config.url = url;
            config.method = HTTP_METHOD_GET;
            Logger.info("UrlStream.begin ",url);

            // cleanup last begin if necessary
            if (client==nullptr){
                client = esp_http_client_init(&config);
            } else {
                esp_http_client_set_url(client,url);
            }

            client = esp_http_client_init(&config);
            if (client==nullptr){
                Logger.error("esp_http_client_init failed");
                return -1;
            }

            int write_buffer_len = 0;
            result = esp_http_client_open(client, write_buffer_len);
            if (result != ESP_OK) {
                Logger.error("http_client_open failed");
                return result;
            }
            size = esp_http_client_fetch_headers(client);
            if (size<=0) {
                Logger.error("esp_http_client_fetch_headers failed");
                return result;
            }

            Logger.printf(SoundLogger::Info, "Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        
            return result;
        }

        virtual int available() {
            return size - read_pos;
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
            return esp_http_client_read(client, (char*)buffer, length);
        }

        virtual int read() {
            fillBuffer();
            return isEOS() ? -1 : read_buffer[read_pos++];
        }

        virtual int peek() {
            fillBuffer();
            return isEOS() ? -1 : read_buffer[read_pos];
        }

        virtual void flush(){
        }

        size_t write(uint8_t) {
            Logger.error("UrlStream write - not supported");
        }


    protected:
        esp_http_client_handle_t client;
        esp_http_client_config_t config;
        long size;
        // buffered read
        uint8_t *read_buffer;
        uint16_t read_buffer_size;
        uint16_t read_pos;
        uint16_t read_size;


        inline void fillBuffer() {
            if (isEOS()){
                read_size = readBytes(read_buffer,read_buffer_size);
                read_pos = 0;
            }
        }

        inline bool isEOS() {
            return read_pos>=read_size;
        }

};

#endif

/**
 * @brief Helper class which Copies an input stream to a output stream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StreamCopy {
    public:
        StreamCopy(Stream &from, Stream &to, int buffer_size){
            this->from = &from;
            this->to = &to;
            this->buffer_size = buffer_size;
            buffer = new uint8_t[buffer_size];
        }

        ~StreamCopy(){
            delete[] buffer;
        }

        /// copies the available bytes from the input stream to the ouptut stream
        size_t copy() {
            size_t total_bytes = available();
            size_t result = total_bytes;
            while (total_bytes>0){
                size_t bytes_to_read = min(total_bytes,static_cast<size_t>(buffer_size) );
                from->readBytes(buffer, bytes_to_read);
                to->write(buffer, bytes_to_read);
                total_bytes -= bytes_to_read;
            }
            return result;
        }

        int available() {
            return from->available();
        }

    protected:
        Stream *from;
        Stream *to;
        uint8_t *buffer;
        int buffer_size;

};


}

