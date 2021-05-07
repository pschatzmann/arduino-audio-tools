#pragma once

#include "AudioConfig.h"
#include "Stream.h"

#ifndef SOUND_LOG_LEVEL
#define SOUND_LOG_LEVEL Error
#endif

#ifndef PRINTF_BUFFER_SIZE
#define PRINTF_BUFFER_SIZE 160
#endif


namespace audio_tools {

/**
 * @brief A simple Logger that writes messages dependent on the log level
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class AudioLogger {
    public:
        /**
         * @brief Supported log levels. You can change the default log level with the help of the PICO_LOG_LEVEL define.
         * 
         */
        enum LogLevel { 
            Debug,
            Info, 
            Warning, 
            Error
        };


        /// activate the logging
        void begin(Stream& out, LogLevel level=SOUND_LOG_LEVEL) {
            this->log_stream_ptr = &out;
            this->log_level = level;
            this->active = true;
        }

        /// stops the logger
        void end(){
            this->active = false;
        }

        /// checks if the logging is active
        bool isLogging(LogLevel level = Info){
            return log_stream_ptr!=nullptr && level >= log_level;
        }

        /// logs an error
        void error(const char *str) const {
            printf(Error,"%s\n", str);
        }
            
        /// logs an info message    
        void info(const char *str) const {
            printf(Info,"%s\n", str);
        }

        /// logs an warning    
        void warning(const char *str) const {
            printf(Warning,"%s\n", str);
        }

        /// writes an debug message    
        void debug(const char *str) const {
            printf(Debug,"%s\n", str);
        }

        /// printf support
        int printf(LogLevel current_level, const char* fmt, ...) const {
            char serial_printf_buffer[PRINTF_BUFFER_SIZE] = {0};
            int len = 0;
            va_list args;

#ifdef USE_ESP32_LOGGER
            va_start(args,fmt);
            //Serial.println(serial_printf_buffer);
            // TODO why is the following not working
            const char* msg = serial_printf_buffer;
            esp_log_level_t level;
            switch(current_level){
                case Error:
                    level = ESP_LOG_ERROR;
                    break;
                case Info:
                    level = ESP_LOG_INFO;
                    break;
                case Warning:
                    level = ESP_LOG_WARN;
                    break;
                case Debug:
                    level = ESP_LOG_DEBUG;
                    break;
                default:
                    level = ESP_LOG_DEBUG;
                    break;
            }
            esp_log_writev(level, TAG, fmt, args);
            va_end(args);   

#else
            if (this->active && log_stream_ptr!=nullptr && current_level >= log_level){
                char serial_printf_buffer[PRINTF_BUFFER_SIZE] = {0};
                va_start(args,fmt);
                len = vsnprintf(serial_printf_buffer,PRINTF_BUFFER_SIZE, fmt, args);
                log_stream_ptr->print(serial_printf_buffer);
                va_end(args);
            }
#endif
            return len;
        }

        /// provides the singleton instance
        static AudioLogger &instance(){
            static AudioLogger *ptr;
            if (ptr==nullptr){
                ptr = new AudioLogger;
            }
            return *ptr;
        }


    protected:
        Stream *log_stream_ptr = nullptr;
        const char* TAG = "AudioTools";
        LogLevel log_level = Error;
        bool active = false;  

        AudioLogger(){
        }

};

}    
