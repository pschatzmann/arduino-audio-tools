
#pragma once

#if __cplusplus >= 202002L
// C++20 or newer is supported
#include <format>
#else

#include "Str.h"
namespace std {

/**
 * @brief Drop in pre c++20 Fromatter replacement for Arduino that supports {}
 * placeholders and escaped {{}} braces which supports e.g
 * Serial.println(Format("State: {} | Temp: {} C | Level: {}%", status, temp,
 * battery).c_str());
 *
 */

class Format {
 public:
  template <typename... Args>
  Format(const char* fmt, Args... args) {
    parse(fmt, args...);
  }

  /// Returns raw C-string pointer suitable for Serial.println() or C APIs
  const char* c_str() { return buffer_.c_str(); }

  /// Implicit conversion to const char*
  operator const char*() { return buffer_.c_str(); }

 private:
  audio_tools::Str buffer_;

  // Base case: process remaining format string when arguments run out
  void parse(const char* fmt) {
    while (*fmt != '\0') {
      // Check for escaped '{{' or '}}'
      if (*fmt == '{' && *(fmt + 1) == '{') {
        buffer_ += '{';
        fmt += 2;
        continue;
      }
      if (*fmt == '}' && *(fmt + 1) == '}') {
        buffer_ += '}';
        fmt += 2;
        continue;
      }
      buffer_ += *fmt++;
    }
  }

  // Recursive variadic template worker
  template <typename T, typename... Args>
  void parse(const char* fmt, T head_value, Args... tail_args) {
    while (*fmt != '\0') {
      // Check for '{}' placeholder
      if (*fmt == '{' && *(fmt + 1) == '}') {
        buffer_ += head_value;         // Append parameter
        parse(fmt + 2, tail_args...);  // Recurse onto remaining arguments
        return;
      }
      // Check for escaped '{{' or '}}'
      if (*fmt == '{' && *(fmt + 1) == '{') {
        buffer_ += '{';
        fmt += 2;
        continue;
      }
      if (*fmt == '}' && *(fmt + 1) == '}') {
        buffer_ += '}';
        fmt += 2;
        continue;
      }

      buffer_ += *fmt++;
    }
  }
};

}  // namespace std
#endif