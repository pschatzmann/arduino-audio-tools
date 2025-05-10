#pragma once
#include <stdint.h>

#include "AudioTools/CoreAudio/AudioBasic/Net.h"
#include "string.h"

namespace audio_tools {

struct BinaryData {
  uint8_t *data = nullptr;
  int len = 0;
  BinaryData(uint8_t *data, int len) {
    this->data = data;
    this->len = len;
  }
  BinaryData() = default;
};

/**
 * @brief Simple OSC Data composer and parser. A OSC data starts with an
 * address string followed by a format string. This is followed by the data.and
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class OSCData {
 public:
  OSCData() = default;
  OSCData(uint8_t *data, int maxlen) {
    this->data_buffer = data;
    max_len = maxlen;
  }
  void setAddress(const char *address) {
    int adr_len = strlen(address);
    strcpy((char *)data_buffer, address);
    int padding = adr_len % 4;
    for (int i = 0; i < padding; i++) {
      data_buffer[adr_len + i] = 0;
    }
    write_pos = adr_len + padding;
  }
  void setFormat(const char *format) {
    int fmt_len = strlen(format);
    data_buffer[write_pos] = ',';
    read_format_start = data_buffer + write_pos + 1;
    strcpy((char *)read_format_start, format);
    int padding = fmt_len % 4;
    for (int i = 0; i < padding; i++) {
      data_buffer[fmt_len + i] = 0;
    }
    write_pos += (fmt_len + padding);
    read_data = data_buffer + write_pos;
  }
  bool write(int32_t number) {
    int32_t number_net = htonl(number);
    if (write_pos + sizeof(int32_t) > max_len) {
      return false;
    }
    memcpy(data_buffer + write_pos, &number_net, sizeof(int32_t));
    write_pos += sizeof(int32_t);
    return true;
  }

  bool write(int64_t number) {
    if (write_pos + sizeof(int64_t) > max_len) {
      return false;
    }
    int64_t number_net = htonll(number);
    memcpy(data_buffer + write_pos, &number_net, sizeof(int64_t));
    write_pos += sizeof(int64_t);
    return true;
  }

  bool write(double fp64) { return write(*(int64_t *)&fp64); }

  bool write(const char *str) {
    int str_len = strlen(str) + 1;
    int padding = str_len % 4;
    if (write_pos + str_len + padding > max_len) {
      return false;
    }

    strcpy((char *)data_buffer + write_pos, str);
    for (int i = 0; i < padding; i++) {
      data_buffer[write_pos + str_len + i] = 0;
    }
    write_pos += (str_len + padding);
    return true;
  }

  bool write(const uint8_t *dataArg, int len) {
    int len_size = sizeof(int);
    int size = len + len_size;
    int padding = size % 4;
    if (len + len_size + padding > max_len) {
      return false;
    }

    int len_net = htonl(len);
    memcpy((data_buffer + write_pos), &len_net, len_size);
    memcpy((data_buffer + write_pos + len_size), dataArg, len);
    for (int i = 0; i < padding; i++) {
      data_buffer[size + i] = 0;
    }
    len += (size + padding);
    return true;
  }

  int size() { return write_pos; }

  void clear() {
    write_pos = 0;
    read_format_start = nullptr;
    read_data = nullptr;
  }

  uint8_t *data() { return data_buffer; }

  /// parse the data: start the reading
  bool parse(uint8_t *data, int len) {
    // store data
    data_buffer = data;
    max_len = len;

    if (data[0] != '/') return false;
    if (strnlen((char *)data, len) == len) return false;

    // determine start of format
    const char *end = getAddress() + strlen(getAddress());
    while (*end == 0) end++;
    read_format_start = (uint8_t *)end;

    // check start of format
    if (*read_format_start != ',') {
      return false;
    }
    read_format_start++;

    // determine start of data
    int format_len = strlen((char *)read_format_start);
    end = (const char *)read_format_start + format_len;
    while (*end == 0) end++;
    read_data = (uint8_t *)end;

    return true;
  }

  /// provides the address: after calling parse
  const char *getAddress() { return (const char *)data_buffer; }

  /// provides the format string: after calling parse
  const char *getFormat() { return (const char *)read_format_start; }

  /// reads the next int
  int32_t readInt32() {
    int32_t number;
    memcpy(&number, read_data, sizeof(int32_t));
    read_data += sizeof(int32_t);
    return ntohl(number);
  }

  /// reads the next long
  int64_t readInt64() {
    int64_t number;
    memcpy(&number, read_data, sizeof(int64_t));
    read_data += sizeof(int64_t);
    return ntohll(number);
  }

  double readDouble() {
    int64_t number = readInt64();
    return *((double *)&number);
  }

  /// reads the next string
  const char *readString() {
    const char *str = (const char *)read_data;
    int str_len = strlen(str) + 1;  // length incl trailing 0
    int null_count = str_len % 4;
    read_data += (str_len + null_count);
    return str;
  }

  /// reads the next binary data blob.
  const BinaryData readData() {
    BinaryData result;
    int32_t len = 0;
    memcpy(&len, read_data, sizeof(int32_t));
    result.len = ntohl(len);
    result.data = read_data + sizeof(int32_t);
    int null_count = result.len % 4;
    read_data += (len + null_count);
    return result;
  }

 protected:
  uint8_t *data_buffer = nullptr;
  int max_len = 0;
  int write_pos = 0;
  uint8_t *read_format_start = nullptr;
  uint8_t *read_data = nullptr;

  uint8_t *getEnd() { return data_buffer + max_len; }
};

}  // namespace audio_tools