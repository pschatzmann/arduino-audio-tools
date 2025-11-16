#pragma once

#include "AudioLogger.h"

namespace audio_tools {

/**
 * @brief We read a single line. A terminating 0 is added to the string to make
 * it compliant for c string functions.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class HttpLineReader {
 public:
  HttpLineReader() {}
  // reads up the the next CR LF - but never more then the indicated len.
  // returns the number of characters read including crlf
  virtual int readlnInternal(Stream& client, uint8_t* str, int len,
                             bool incl_nl = true) {
    int result = 0;
    LOGD("HttpLineReader %s", "readlnInternal");
    // wait for first character
    for (int w = 0; w < 200 && client.available() == 0; w++) {
      delay(10);
    }
    // if we do not have any data we stop
    if (client.available() == 0) {
      LOGW("HttpLineReader %s", "readlnInternal->no Data");
      str[0] = 0;
      return 0;
    }

    // process characters until we find a new line
    bool is_buffer_overflow = false;
    int j = 0;
    while (true) {
      int c = client.read();
      if (c == -1) {
        break;
      }

      if (j < len) {
        result++;
      } else {
        is_buffer_overflow = true;
      }

      if (c == '\n') {
        if (incl_nl) {
          str[j] = c;
          break;
        } else {
          // remove cr lf
          if (j >= 1) {
            if (str[j - 1] == '\r') {
              // remove cr
              str[j - 1] = 0;
              break;
            } else {
              // remove nl
              str[j] = 0;
              break;
            }
          }
        }
      }
      if (!is_buffer_overflow) {
        str[j] = c;
      }
      j++;
    }
    str[result - 1] = 0;
    if (is_buffer_overflow) {
      // SAFETY FIX: Don't print potentially corrupted binary data with %s
      // Binary garbage can contain terminal escape codes or invalid UTF-8 that crashes Serial.printf()
      //
      // This fix prevents ESP32 hard resets when HTTP servers send malformed headers
      // Real-world trigger: http://fast.citrus3.com:2020/stream/wtmj-radio
      //
      // Strategy:
      // 1. Sanitize the actual buffer (prevents parser poisoning downstream)
      // 2. Count printable vs binary content
      // 3. Use hex dump for binary garbage (safer than string masking)
      // 4. Limit output to 256 bytes (prevents log spam)

      int printable = 0;
      int non_printable = 0;
      int actual_len = 0;

      // First pass: count and find actual length
      for (int i = 0; i < len && str[i] != 0; i++) {
        actual_len = i + 1;
        if (str[i] >= 32 && str[i] <= 126) {
          printable++;
        } else if (str[i] != '\r' && str[i] != '\n' && str[i] != '\t') {
          non_printable++;
          // CRITICAL: Sanitize the actual buffer to prevent parser poisoning
          // Replace binary garbage with space to avoid confusing HTTP header parser
          str[i] = ' ';
        }
      }

      // Limit logging output to 256 bytes to prevent excessive serial spam
      int log_len = (actual_len > 256) ? 256 : actual_len;

      // If mostly binary garbage (>50% non-printable), use hex dump for safety
      if (non_printable > printable) {
        LOGE("Line cut off: [%d bytes, %d binary chars - showing hex dump of first %d bytes]",
             actual_len, non_printable, (log_len > 32 ? 32 : log_len));

        // Hex dump (safer than string output - never misinterpreted)
        // Show first 32 bytes maximum
        int hex_len = (log_len > 32) ? 32 : log_len;
        for (int i = 0; i < hex_len; i += 16) {
          char hex_line[64];
          int line_len = (hex_len - i > 16) ? 16 : (hex_len - i);
          int pos = 0;
          for (int j = 0; j < line_len; j++) {
            pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%02X ", (uint8_t)str[i + j]);
          }
          LOGE("  %04X: %s", i, hex_line);
        }
      } else {
        // Mostly printable - safe to log as string (already sanitized in-place above)
        // Truncate to 256 bytes for logging
        if (log_len < actual_len) {
          char saved = str[log_len];
          str[log_len] = 0;
          LOGE("Line cut off: %s... [%d more bytes]", str, actual_len - log_len);
          str[log_len] = saved;
        } else {
          LOGE("Line cut off: %s", str);
        }
      }
    }

    return result;
  }
};

}  // namespace audio_tools
