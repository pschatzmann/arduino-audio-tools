#pragma once

#include "AudioTools/CoreAudio/AudioRuntime.h"
#include "AudioToolsConfig.h"

#if USE_AUDIO_LOGGING

namespace audio_tools {

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
   * @brief Supported log levels. You can change the default log level with the
   * help of the PICO_LOG_LEVEL define.
   *
   */
  enum LogLevel { Debug, Info, Warning, Error };

  /// activate the logging
  void begin(Print& out, LogLevel level = LOG_LEVEL) {
    this->log_print_ptr = &out;
    this->log_level = level;
  }

  // defines the log level
  void setLogLevel(LogLevel level) { this->log_level = level; }

  /// checks if the logging is active
  bool isLogging(LogLevel level = Info) {
    return log_print_ptr != nullptr && level >= log_level;
  }

  AudioLogger& prefix(const char* file, int line, LogLevel current_level) {
    printPrefix(file, line, current_level);
    return *this;
  }

  void println() {
#if defined(IS_DESKTOP) || defined(IS_DESKTOP_WITH_TIME_ONLY)
    fprintf(stderr, "%s\n", print_buffer);
    fflush(stderr);
#elif defined(IS_ZEPHYR)
    if (log_print_ptr != nullptr) {
      log_print_ptr->println(print_buffer);
      log_print_ptr->flush();
    } else {
      printk(print_buffer);
      printk("\n");
    }
#else
    log_print_ptr->println(print_buffer);
    log_print_ptr->flush();
#endif
    print_buffer[0] = 0;
  }

  char* str() { return print_buffer; }

  /// provides the singleton instance
  static AudioLogger& instance() {
    static AudioLogger* ptr;
    if (ptr == nullptr) {
      ptr = new AudioLogger;
    }
    return *ptr;
  }

  LogLevel level() { return log_level; }

  void print(const char* c) { log_print_ptr->print(c); }

  void printChar(char c) { log_print_ptr->print(c); }

  void printCharHex(char c) {
    char tmp[5];
    unsigned char val = c;
    snprintf(tmp, 5, "%02X ", val);
    log_print_ptr->print(tmp);
  }

 protected:
  Print* log_print_ptr = &LOG_STREAM;
  LogLevel log_level = LOG_LEVEL;
  char print_buffer[LOG_PRINTF_BUFFER_SIZE];

  AudioLogger() {}

  const char* levelName(LogLevel level) const {
    switch (level) {
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
};

/// static logging instance
static AudioLogger& AudioToolsLogger = AudioLogger::instance();

/// synonym for LogLevel
using AudioToolsLogLevel = AudioLogger::LogLevel;

/// Class specific custom log level
class CustomLogLevel {
 public:
  AudioLogger::LogLevel getActual() { return actual; }

  /// Defines a custom level
  void set(AudioLogger::LogLevel level) {
    active = true;
    original = AudioLogger::instance().level();
    actual = level;
  }

  /// sets the defined log level
  void set() {
    if (active) {
      AudioLogger::instance().begin(Serial, actual);
    }
  }
  /// resets to the original log level
  void reset() {
    if (active) {
      AudioLogger::instance().begin(Serial, original);
    }
  }

 protected:
  bool active = false;
  AudioLogger::LogLevel original;
  AudioLogger::LogLevel actual;
};

}  // namespace audio_tools

#define LOG_OUT_PGMEM(level, fmt, ...)                                  \
  {                                                                     \
    ::audio_tools::AudioToolsLogger.prefix(__FILE__, __LINE__, level);                 \
    snprintf(::audio_tools::AudioToolsLogger.str(), LOG_PRINTF_BUFFER_SIZE, PSTR(fmt), \
             ##__VA_ARGS__);                                            \
    ::audio_tools::AudioToolsLogger.println();                                         \
  }

#define LOG_OUT(level, fmt, ...)                                  \
  {                                                               \
    ::audio_tools::AudioToolsLogger.prefix(__FILE__, __LINE__, level);           \
    snprintf(::audio_tools::AudioToolsLogger.str(), LOG_PRINTF_BUFFER_SIZE, fmt, \
             ##__VA_ARGS__);                                      \
    ::audio_tools::AudioToolsLogger.println();                                   \
  }
#define LOG_MIN(level)                                  \
  {                                                     \
    ::audio_tools::AudioToolsLogger.prefix(__FILE__, __LINE__, level); \
    ::audio_tools::AudioToolsLogger.println();                         \
  }

#ifdef LOG_NO_MSG
#define LOGD(fmt, ...)                                  \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Debug) { \
    LOG_MIN(AudioLogger::Debug);                        \
  }
#define LOGI(fmt, ...)                                 \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Info) { \
    LOG_MIN(AudioLogger::Info);                        \
  }
#define LOGW(fmt, ...)                                    \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Warning) { \
    LOG_MIN(AudioLogger::Warning);                        \
  }
#define LOGE(fmt, ...)                                  \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Error) { \
    LOG_MIN(AudioLogger::Error);                        \
  }
#else
// Log statments which store the fmt string in Progmem
#define LOGD(fmt, ...)                                     \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Debug) {    \
    LOG_OUT_PGMEM(AudioLogger::Debug, fmt, ##__VA_ARGS__); \
  }
#define LOGI(fmt, ...)                                    \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Info) {    \
    LOG_OUT_PGMEM(AudioLogger::Info, fmt, ##__VA_ARGS__); \
  }
#define LOGW(fmt, ...)                                       \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Warning) {    \
    LOG_OUT_PGMEM(AudioLogger::Warning, fmt, ##__VA_ARGS__); \
  }
#define LOGE(fmt, ...)                                     \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Error) {    \
    LOG_OUT_PGMEM(AudioLogger::Error, fmt, ##__VA_ARGS__); \
  }
#endif

// Just log file and line
#if defined(NO_TRACED) || defined(NO_TRACE)
#define TRACED()
#else
#define TRACED()                                        \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Debug) { \
    LOG_OUT(AudioLogger::Debug, LOG_METHOD);            \
  }
#endif

#if defined(NO_TRACEI) || defined(NO_TRACE)
#define TRACEI()
#else
#define TRACEI()                                       \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Info) { \
    LOG_OUT(AudioLogger::Info, LOG_METHOD);            \
  }
#endif

#if defined(NO_TRACEW) || defined(NO_TRACE)
#define TRACEW()
#else
#define TRACEW()                                          \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Warning) { \
    LOG_OUT(AudioLogger::Warning, LOG_METHOD);            \
  }
#endif

#if defined(NO_TRACEE) || defined(NO_TRACE)
#define TRACEE()
#else
#define TRACEE()                                        \
  if (::audio_tools::AudioToolsLogger.level() <= AudioLogger::Error) { \
    LOG_OUT(AudioLogger::Error, LOG_METHOD);            \
  }
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
