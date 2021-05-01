#pragma once

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
        void error(const char *str, const char* str1=nullptr, const char* str2=nullptr) const {
            log(Error, str, str1, str2);
        }
            
        /// logs an info message    
        void info(const char *str, const char* str1=nullptr, const char* str2=nullptr) const {
            log(Info, str, str1, str2);
        }

        /// logs an warning    
        void warning(const char *str, const char* str1=nullptr, const char* str2=nullptr) const {
            log(Warning, str, str1, str2);
        }

        /// writes an debug message    
        void debug(const char *str, const char* str1=nullptr, const char* str2=nullptr) const {
            log(Debug, str, str1, str2);
        }

        /// printf support
        int printf(LogLevel current_level, const char* fmt, ...) const {
            int len = 0;
            if (this->active && log_stream_ptr!=nullptr && current_level >= log_level){
                char serial_printf_buffer[PRINTF_BUFFER_SIZE] = {0};
                va_list args;
                va_start(args,fmt);
                len = vsnprintf(serial_printf_buffer,PRINTF_BUFFER_SIZE, fmt, args);
                log_stream_ptr->print(serial_printf_buffer);
                va_end(args);
            }
            return len;
        }


        /// write an message to the log
        void log(LogLevel current_level, const char *str, const char* str1=nullptr, const char* str2=nullptr) const {
            if (this->active && log_stream_ptr!=nullptr){
                if (current_level >= log_level){
                    log_stream_ptr->print((char*)str);
                    if (str1!=nullptr){
                        log_stream_ptr->print(" ");
                        log_stream_ptr->print(str1);
                    }
                    if (str2!=nullptr){
                        log_stream_ptr->print(" ");
                        log_stream_ptr->print(str2);
                    }
                    log_stream_ptr->println();
                    log_stream_ptr->flush();
                }
            }
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
        Stream *log_stream_ptr;
        LogLevel log_level;
        bool active;  

        AudioLogger(){

        }


};

}    
