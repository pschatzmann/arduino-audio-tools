#pragma once

#include "AudioConfig.h"
#if defined(ARDUINO) && !defined(IS_MIN_DESKTOP)
#  include "Print.h"
#endif
// Logging Implementation
#if USE_AUDIO_LOGGING

namespace audio_tools {

#if defined(ESP32) && defined(SYNCHRONIZED_LOGGING)
#  include "freertos/FreeRTOS.h"
#  include "freertos/task.h"
static portMUX_TYPE mutex_logger = portMUX_INITIALIZER_UNLOCKED;
#endif


/**
 * @brief A simple Logger that writes messages dependent on the log level
 * @ingroup tools
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
        void begin(Print& out, LogLevel level=LOG_LEVEL) {
            this->log_print_ptr = &out;
            this->log_level = level;
        }

        // defines the log level
        void setLogLevel(LogLevel level){
            this->log_level = level;
        }

        /// checks if the logging is active
        bool isLogging(LogLevel level = Info){
            return log_print_ptr!=nullptr && level >= log_level;
        }

        AudioLogger &prefix(const char* file, int line, LogLevel current_level){
            lock();
            printPrefix(file,line,current_level);
            return *this;
        }

        void println(){
#if defined(IS_DESKTOP) || defined(IS_DESKTOP_WITH_TIME_ONLY)
            fprintf( stderr, "%s\n", print_buffer);
#else
            log_print_ptr->println(print_buffer);
#endif
            print_buffer[0]=0;
            unlock();
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

        void print(const char *c){
            log_print_ptr->print(c);
        }

        void printChar(char c){
            log_print_ptr->print(c);
        }

        void printCharHex(char c){
            char tmp[5];
            unsigned char val = c;
            snprintf(tmp, 5, "%02X ", val);
            log_print_ptr->print(tmp);
        }

    protected:
        Print *log_print_ptr = &LOG_STREAM;
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
            int len = log_print_ptr->print("[");
            len += log_print_ptr->print(level_code);
            len += log_print_ptr->print("] ");
            len += log_print_ptr->print(file_name);
            len += log_print_ptr->print(" : ");
            len += log_print_ptr->print(line);
            len += log_print_ptr->print(" - ");
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

/// Class specific custom log level
class CustomLogLevel {
public:
    AudioLogger::LogLevel getActual(){
        return actual;
    }

    /// Defines a custom level
    void set(AudioLogger::LogLevel level){
      active = true;
      original = AudioLogger::instance().level();
      actual = level;
    }

    /// sets the defined log level
    void set(){
        if (active){
            AudioLogger::instance().begin(Serial, actual);
        }
    }
    /// resets to the original log level
    void reset(){
        if (active){
            AudioLogger::instance().begin(Serial, original);
        }
    }
protected:
    bool active=false;
    AudioLogger::LogLevel original;
    AudioLogger::LogLevel actual;

};



}


//#define LOG_OUT(level, fmt, ...) {AudioLogger::instance().prefix(__FILE__,__LINE__, level);cont char PROGMEM *fmt_P=F(fmt); snprintf_P(AudioLogger::instance().str(), LOG_PRINTF_BUFFER_SIZE, fmt,  ##__VA_ARGS__); AudioLogger::instance().println();}
#define LOG_OUT_PGMEM(level, fmt, ...) { \
    AudioLogger::instance().prefix(__FILE__,__LINE__, level); \
    snprintf(AudioLogger::instance().str(), LOG_PRINTF_BUFFER_SIZE, PSTR(fmt),  ##__VA_ARGS__); \
    AudioLogger::instance().println();\
}

#define LOG_OUT(level, fmt, ...) { \
    AudioLogger::instance().prefix(__FILE__,__LINE__, level); \
    snprintf(AudioLogger::instance().str(), LOG_PRINTF_BUFFER_SIZE, fmt,  ##__VA_ARGS__); \
    AudioLogger::instance().println();\
}
#define LOG_MIN(level) { \
    AudioLogger::instance().prefix(__FILE__,__LINE__, level); \
    AudioLogger::instance().println();\
}

#ifdef LOG_NO_MSG
#define LOGD(fmt, ...) if (AudioLogger::instance().level()<=AudioLogger::Debug) { LOG_MIN(AudioLogger::Debug);}
#define LOGI(fmt, ...) if (AudioLogger::instance().level()<=AudioLogger::Info) { LOG_MIN(AudioLogger::Info);}
#define LOGW(fmt, ...) if (AudioLogger::instance().level()<=AudioLogger::Warning) { LOG_MIN(AudioLogger::Warning);}
#define LOGE(fmt, ...) if (AudioLogger::instance().level()<=AudioLogger::Error) { LOG_MIN(AudioLogger::Error);}
#else
// Log statments which store the fmt string in Progmem
#define LOGD(fmt, ...) if (AudioLogger::instance().level()<=AudioLogger::Debug) { LOG_OUT_PGMEM(AudioLogger::Debug, fmt, ##__VA_ARGS__);}
#define LOGI(fmt, ...) if (AudioLogger::instance().level()<=AudioLogger::Info) { LOG_OUT_PGMEM(AudioLogger::Info, fmt, ##__VA_ARGS__);}
#define LOGW(fmt, ...) if (AudioLogger::instance().level()<=AudioLogger::Warning) { LOG_OUT_PGMEM(AudioLogger::Warning, fmt, ##__VA_ARGS__);}
#define LOGE(fmt, ...) if (AudioLogger::instance().level()<=AudioLogger::Error) { LOG_OUT_PGMEM(AudioLogger::Error, fmt, ##__VA_ARGS__);}
#endif

// Just log file and line 
#if defined(NO_TRACED) || defined(NO_TRACE)
#  define TRACED()
#else
#  define TRACED() if (AudioLogger::instance().level()<=AudioLogger::Debug) { LOG_OUT(AudioLogger::Debug, LOG_METHOD);}
#endif

#if  defined(NO_TRACEI) || defined(NO_TRACE)
#  define TRACEI()
#else 
#  define TRACEI() if (AudioLogger::instance().level()<=AudioLogger::Info) { LOG_OUT(AudioLogger::Info, LOG_METHOD);}
#endif

#if  defined(NO_TRACEW) || defined(NO_TRACE)
#  define TRACEW()
#else 
#  define TRACEW() if (AudioLogger::instance().level()<=AudioLogger::Warning) { LOG_OUT(AudioLogger::Warning, LOG_METHOD);}
#endif

#if  defined(NO_TRACEE) || defined(NO_TRACE)
#  define TRACEE()
#else 
#define TRACEE() if (AudioLogger::instance().level()<=AudioLogger::Error) { LOG_OUT(AudioLogger::Error, LOG_METHOD);}
#endif



#else

// Switch off logging
#define LOGD(...) 
#define LOGI(...) 
#define LOGW(...) 
#define LOGE(...) 
#define TRACED()
#define TRACEI()
#define TRACEW()
#define TRACEE()

#endif


