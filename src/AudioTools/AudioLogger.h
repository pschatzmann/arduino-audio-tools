#pragma once

#include "AudioConfig.h"

#include "Stream.h"
//#if __has_include (<cstdarg>)
//#include <cstdarg>
//#define VARARG_SUPPORT false
//#else
//#define VARARG_SUPPORT false
//#endif

#define LOG_LEVEL AudioLogger::Warning
#define LOG_STREAM Serial

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
        void begin(Stream& out, LogLevel level=LOG_LEVEL) {
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

// #if VARARG_SUPPORT

//         /// logs an error
//         void error(const char *str) const {
//             printf(Error,"%s\n", str);
//         }
            
//         /// logs an info message    
//         void info(const char *str) const {
//             printf(Info,"%s\n", str);
//         }

//         /// logs an warning    
//         void warning(const char *str) const {
//             printf(Warning,"%s\n", str);
//         }

//         /// writes an debug message    
//         void debug(const char *str) const {
//             printf(Debug,"%s\n", str);
//         }


//         /// printf support
//         int printf(LogLevel current_level, const char* fmt, ...) const {
//             char serial_printf_buffer[PRINTF_BUFFER_SIZE] = {0};
//             int len = 0;
//             va_list args;

//             if (this->active && log_stream_ptr!=nullptr && current_level >= log_level){
//                 char serial_printf_buffer[PRINTF_BUFFER_SIZE] = {0};
//                 va_start(args,fmt);
//                 len = vsnprintf(serial_printf_buffer,PRINTF_BUFFER_SIZE, fmt, args);
//                 log_stream_ptr->print(serial_printf_buffer);
//                 va_end(args);
//             }
//             return len;
//         }

//         int printLog(const char* file, int line, LogLevel current_level, const char* fmt, ...) const {
//             char serial_printf_buffer[PRINTF_BUFFER_SIZE] = {0};
//             int len = 0;
   
//             if (this->active && log_stream_ptr!=nullptr && current_level >= log_level){
//                 // content
//                 char serial_printf_buffer[PRINTF_BUFFER_SIZE] = {0};
//                 // prefix
//                 len += printPrefix(file, line, current_level);

//                 va_list args;
//                 va_start(args,fmt);
//                 len += vsnprintf(serial_printf_buffer,PRINTF_BUFFER_SIZE, fmt, args);
//                 log_stream_ptr->print(serial_printf_buffer);
//                 va_end(args);
//                 // newline
//                 len += log_stream_ptr->println();
//             }
//             return len;
//         }

// #else

        AudioLogger &prefix(const char* file, int line, LogLevel current_level){
            printPrefix(file,line,current_level);
            return *this;
        }

        void println(){
            log_stream_ptr->println(print_buffer);
            print_buffer[0]=0;
        }

        char* str() {
            return print_buffer;
        }


//#endif

        /// provides the singleton instance
        static AudioLogger &instance(){
            static AudioLogger *ptr;
            if (ptr==nullptr){
                ptr = new AudioLogger;
            }
            return *ptr;
        }


    protected:
        Stream *log_stream_ptr = &LOG_STREAM;
        const char* TAG = "AudioTools";
        LogLevel log_level = LOG_LEVEL;
        bool active = true;  
//#if !VARARG_SUPPORT
        char print_buffer[PRINTF_BUFFER_SIZE];
//#endif
        AudioLogger(){
        }

        const char* levelName(LogLevel level) const {
            switch(level){
                case Debug:
                    return "D";
                case Info:
                    return "I";
                case Warning:
                    return "W";
                case Error:
                    return "E";
            }
            return "";
        }

        int printPrefix(const char* file, int line, LogLevel current_level) const {
            const char* file_name = strrchr(file, '/') ? strrchr(file, '/') + 1 : file;
            const char* level_code = levelName(current_level);
            int len = log_stream_ptr->print("[");
            len += log_stream_ptr->print(level_code);
            len += log_stream_ptr->print("] ");
            len += log_stream_ptr->print(file_name);
            len += log_stream_ptr->print(" : ");
            len += log_stream_ptr->print(line);
            len += log_stream_ptr->print(" - ");
            return len;
        }

};

}    
