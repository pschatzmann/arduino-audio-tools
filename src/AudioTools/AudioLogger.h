#pragma once

#include "AudioConfig.h"
#include "Stream.h"

// Logging Implementation
#if USE_AUDIO_LOGGING

namespace audio_tools {

#if defined(ESP32) && defined(SYNCHRONIZED_LOGGING)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static portMUX_TYPE mutex_logger = portMUX_INITIALIZER_UNLOCKED;
#endif

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
        }

        /// checks if the logging is active
        bool isLogging(LogLevel level = Info){
            return log_stream_ptr!=nullptr && level >= log_level;
        }

        AudioLogger &prefix(const char* file, int line, LogLevel current_level){
            lock();
            printPrefix(file,line,current_level);
            return *this;
        }

        void println(){
            CHECK_MEMORY();
            log_stream_ptr->println(print_buffer);
            print_buffer[0]=0;
            unlock();
            CHECK_MEMORY();
        }

        char* str() {
            return print_buffer;
        }

        /// provides the singleton instance
        static AudioLogger &instance(){
            static AudioLogger *ptr;
            if (ptr==nullptr){
                ptr = new AudioLogger;
            }
            return *ptr;
        }

        LogLevel level() {
            return log_level;
        }



    protected:
        Stream *log_stream_ptr = &LOG_STREAM;
        const char* TAG = "AudioTools";
        LogLevel log_level = LOG_LEVEL;
        char print_buffer[LOG_PRINTF_BUFFER_SIZE];

        AudioLogger() {}

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

        void lock(){
            #if defined(ESP32) && defined(SYNCHRONIZED_LOGGING)
                portENTER_CRITICAL(&mutex_logger);
            #endif
        }

        void unlock(){
            #if defined(ESP32) && defined(SYNCHRONIZED_LOGGING)
                portEXIT_CRITICAL(&mutex_logger);
            #endif
        }
};


#define LOG_OUT(level, ...) snprintf(AudioLogger::instance().prefix(__FILE__,__LINE__, level).str(),LOG_PRINTF_BUFFER_SIZE,__VA_ARGS__); AudioLogger::instance().println();

#define LOGD(...) if (AudioLogger::instance().level()<=AudioLogger::Debug) { LOG_OUT(AudioLogger::Debug, __VA_ARGS__);}
#define LOGI(...) if (AudioLogger::instance().level()<=AudioLogger::Info) { LOG_OUT(AudioLogger::Info, __VA_ARGS__);}
#define LOGW(...) if (AudioLogger::instance().level()<=AudioLogger::Warning) { LOG_OUT(AudioLogger::Warning, __VA_ARGS__);}
#define LOGE(...) if (AudioLogger::instance().level()<=AudioLogger::Error) { LOG_OUT(AudioLogger::Error, __VA_ARGS__);}

}
    
#else

#define LOGD(...) 
#define LOGI(...) 
#define LOGW(...) 
#define LOGE(...) 

#endif
