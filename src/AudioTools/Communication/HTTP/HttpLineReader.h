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
  /// Default constructor
  HttpLineReader() = default;

  // reads up the the next CR LF - but never more then the indicated len.
  // returns the number of characters read including crlf
  virtual int readlnInternal(Stream& client, uint8_t* str, int len,
                             bool incl_nl = true) {
    int result = 0;
    LOGD("HttpLineReader %s", "readlnInternal");

    // process characters until we find a new line
    bool is_buffer_overflow = false;
    int j = 0;
    while (true) {
      int c = client.read();
      if (c == -1) {
        if (result == 0) {
          LOGW("HttpLineReader %s", "readlnInternal->no Data");
          str[0] = 0;
          return -1;
        }
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
          if (j >= 1 && str[j - 1] == '\r') {
            str[j - 1] = 0;
          } else {
            str[j] = 0;
          }
          break;
        }
      }
      if (!is_buffer_overflow) {
        str[j] = c;
      }
      j++;
    }
    if (result > 0) {
      str[result - 1] = 0;
    } else {
      str[0] = 0;
    }
    if (is_buffer_overflow) {
      LOGE("HttpLineReader %s", "readlnInternal->cut off too long line");
    }

    return result;
  }
};

}  // namespace audio_tools
