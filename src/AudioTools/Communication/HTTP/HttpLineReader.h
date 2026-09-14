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

  /// reads up the the next CR LF - but never more then the indicated len.
  virtual int readlnInternal(Stream& client, uint8_t* str, int len,
                             bool incl_nl = true) {
    LOGD("HttpLineReader %s", "readlnInternal");
    // readBytesUntil uses timedRead() which respects setTimeout()
    int result = client.readBytesUntil('\n', (char*)str, len);
    // Return -1 if no data was read. 
    if (result == 0) {
      LOGW("HttpLineReader %s", "readlnInternal->no Data");
      str[0] = 0;
      return -1;
    }
    if (incl_nl) {
      // we need to add the new line character since it was removed by readBytesUntil
      if (result < len) {
        str[result++] = '\n';
      }
    } else {
      // remove trailing CR
      if (result > 0 && str[result - 1] == '\r') {
        result--;
      }
    }
    str[result] = 0;
    return result;
  }
};

}  // namespace audio_tools
