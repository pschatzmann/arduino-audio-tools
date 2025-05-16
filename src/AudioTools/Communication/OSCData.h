#pragma once
#include <stdint.h>
#include <string.h>
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"

namespace audio_tools {

/**
 * @brief Simple structure to hold binary data.
 * @ingroup aoo-utils
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

struct OSCBinaryData {
  uint8_t *data = nullptr;
  size_t len = 0;
  OSCBinaryData(uint8_t *data, int len) {
    this->data = data;
    this->len = len;
  }
  OSCBinaryData() = default;
};

enum class OSCCompare { Matches, Equals, StartsWith, EndsWith, Contains };

/**
 * @brief A simple OSC Data composer and parser. A OSC data starts with an
 * address string followed by a format string. This is followed by the data.
 * You need to call the read and write methods in the sequence defined by the
 * format string. There is no validation for this, so you need to be careful
 * and test your code properly.
 * To compose a message call:
 * - setAddress() to set the address
 * - setFormat() to set the format string
 * - write() to write the data
 * ...
 *  you can access the content via data() and size()
 *
 * To parse a message call:
 * - parse() to parse the message
 * - getAddress() to get the address
 * - getFormat() to get the format string
 * - readXxxx() to read the data
 * ...
 * I recommend to register for each address and process the reads in the
 * callback
 *
 * OSC V1.0 Specification: https://opensoundcontrol.stanford.edu/spec-1_0.html
 *
 * @ingroup aoo-utils
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class OSCData {
 public:
  /// Data for receiving:
  OSCData() = default;
  /// Data for sending
  OSCData(uint8_t *data, uint32_t maxlen) {
    binary_content.data = data;
    binary_content.len = maxlen;
  }

  /// Defines the address string (e.g. /test)
  void setAddress(const char *address) {
    int adr_len = OSCData::oscSize(address);
    memset(binary_content.data, 0, adr_len);
    strcpy((char *)binary_content.data, address);
    write_pos = adr_len;
  }

  /// Defines the format string (e.g. ,iif)
  void setFormat(const char *format) {
    int fmt_len = OSCData::oscFormatSize(strlen(format));
    memset(binary_content.data + write_pos, 0, fmt_len);
    binary_content.data[write_pos] = ',';
    read_format_start = binary_content.data + write_pos + 1;
    strcpy((char *)read_format_start, format);
    write_pos += fmt_len;
    read_data = binary_content.data + write_pos;
  }

  /// write an int32_t number (i)
  bool write(int32_t number) {
    int32_t number_net = htonl(number);
    if (write_pos + sizeof(int32_t) > binary_content.len) {
      return false;
    }
    memcpy(binary_content.data + write_pos, &number_net, sizeof(int32_t));
    write_pos += sizeof(int32_t);
    return true;
  }

  /// write an int64_t number
  bool write(int64_t number) {
    if (write_pos + sizeof(int64_t) > binary_content.len) {
      return false;
    }
    int64_t number_net = htonll(number);
    memcpy(binary_content.data + write_pos, &number_net, sizeof(int64_t));
    write_pos += sizeof(int64_t);
    return true;
  }

#ifndef IS_DESKTOP

  /// write a unsigned long number
  bool write(unsigned long number) { return write((uint64_t)number); }

#endif

  /// write a timetag (t) data type
  bool write(uint64_t number) {
    if (write_pos + sizeof(uint64_t) > binary_content.len) {
      return false;
    }
    uint64_t number_net = htonll(number);
    memcpy(binary_content.data + write_pos, &number_net, sizeof(uint64_t));
    write_pos += sizeof(uint64_t);
    return true;
  }

  /// write a 64bit double number (d)
  bool write(double fp64) { return write(*(int64_t *)&fp64); }

  /// write a string (s)
  bool write(const char *str) {
    int str_len = OSCData::oscSize(str);
    if (write_pos + str_len > binary_content.len) {
      return false;
    }
    memset(binary_content.data + write_pos, 0, str_len);
    strcpy((char *)binary_content.data + write_pos, str);
    write_pos += str_len;
    return true;
  }

  /// write a binary blob (b)
  bool write(OSCBinaryData &data) { return write(data.data, data.len); }

  /// write a binary blob (b)
  bool write(const uint8_t *dataArg, int len) {
    uint32_t size = OSCData::oscSize(len) + sizeof(int32_t);
    if (write_pos + size > binary_content.len) {
      return false;
    }
    memset(binary_content.data + write_pos, 0, size);
    int32_t len_net = htonl(len);
    memcpy((binary_content.data + write_pos), &len_net, sizeof(int32_t));
    memcpy((binary_content.data + write_pos + sizeof(int32_t)), dataArg, len);
    write_pos += size;
    return true;
  }

  /// clears all data
  void clear() {
    write_pos = 0;
    read_format_start = nullptr;
    read_data = nullptr;
    binary_content.data = nullptr;
    binary_content.len = 0;
    callbacks.clear();
  }

  /// provides access to the original binary message (defined in the constructor
  /// or via parse())
  uint8_t *data() { return binary_content.data; }

  /// returns the number of bytes written (or parsed)
  int size() { return write_pos; }

  /// parse the data to start for reading
  bool parse(uint8_t *data, size_t len) {
    // store data
    binary_content.data = data;
    binary_content.len = len;

    // to support size()
    write_pos = len;

    // Print frist 20 digits
    if (is_log_active) logMsg(data, 20);

    if (data[0] != '/') return false;

    // determine address length 4 byte aligned
    int addr_len = OSCData::oscSize((char *)binary_content.data);

    // determine start of format
    read_format_start = (uint8_t *)getAddress() + addr_len;

    // check start of format
    if (*read_format_start != ',') {
      return false;
    }

    int format_len =
        OSCData::oscFormatSize(strlen((char *)read_format_start + 1));

    // determine start of data
    read_data = read_format_start + format_len;

    // move past ,
    read_format_start++;

    /// call callback if there are any
    if (callbacks.size() > 0) {
      for (auto &cb : callbacks) {
        if (compare(cb.compare, cb.address, getAddress())) {
          if (cb.callback != nullptr) {
            if (cb.callback(*this, reference)) {
              return true;
            }
          }
        }
      }
      // return false if no callback was called successfully
      return false;
    }

    return true;
  }

  void logMsg(uint8_t *data, int len) {
    Serial.print("OSCData: ");
    for (int i = 0; i < len; i++) {
      Serial.print((char)data[i]);
    }
#ifndef IS_DESKTOP
    Serial.println();
    Serial.print("Hex Data: ");
    for (int i = 0; i < len; i++) {
      Serial.print(data[i], HEX);
    }
    Serial.println();
#endif
  }

  /// provides the address: after calling parse
  const char *getAddress() { return (const char *)binary_content.data; }

  /// provides the format string: after calling parse
  const char *getFormat() { return (const char *)read_format_start; }

  /// reads the next attributes as float
  float readFloat() {
    int64_t number = readInt32();
    return *((float *)&number);
  }

  /// reads the next attribute as int32
  int32_t readInt32() {
    int32_t number;
    memcpy(&number, read_data, sizeof(int32_t));
    read_data += sizeof(int32_t);
    return ntohl(number);
  }

  /// reads the next attribute as long
  int64_t readInt64() {
    int64_t number;
    memcpy(&number, read_data, sizeof(int64_t));
    read_data += sizeof(int64_t);
    return ntohll(number);
  }

  /// reads the next attribute as uint64_t
  uint64_t readTime() { return (uint64_t)readInt64(); }

  /// reads the next attribute as double
  double readDouble() {
    int64_t number = readInt64();
    return *((double *)&number);
  }

  /// reads the next string
  const char *readString() {
    const char *str = (const char *)read_data;
    int str_len = OSCData::oscSize(str);
    read_data += str_len;
    return str;
  }

  /// reads the next attribute as binary data blob.
  const OSCBinaryData readData() {
    OSCBinaryData result;
    int32_t len = 0;
    memcpy(&len, read_data, sizeof(int32_t));
    result.len = ntohl(len);
    result.data = read_data + sizeof(int32_t);
    read_data += (OSCData::oscSize(result.len) + sizeof(int32_t));
    return result;
  }

  /// Log the beginning of the received messages
  void setLogActive(bool active) { is_log_active = active; }

  /// provides access to the original binary message (defined in the constructor
  /// or via parse())
  OSCBinaryData &messageData() { return binary_content; }

  /// storage size (multiple of 4)
  static int oscSize(int len) {
    if (len % 4 == 0) return len;
    int padding = 4 - (len % 4);
    return len + padding;
  }

  /// storage size (multiple of 4) for binary blob data
  static int oscSize(OSCBinaryData data) {
    return sizeof(uint32_t) + OSCData::oscSize(data.len);
  }

  /// storage size (multiple of 4) for string
  static int oscSize(const char *str) {
    // +1 for trailing 0
    return OSCData::oscSize(strlen(str) + 1);
  }

  /// storage size (multiple of 4) for format string (w/o the leading ,)
  static int oscFormatSize(int len) {
    //+2 for , and trailing 0
    return oscSize(len + 2);
  }

  /// storage size (multiple of 4) for format string (w/o the leading ,)
  static int oscFormatSize(const char *str) {
    return oscFormatSize(strlen(str));
  }

  /// store a reference object (for callback)
  void setReference(void *ref) { reference = ref; }

  /// register a parsing callback for a specific address matching string
  bool addCallback(const char *address,
                   bool (*callback)(OSCData &data, void *ref),
                   OSCCompare compare = OSCCompare::Matches) {
    if (address == nullptr || callback == nullptr) return false;

    /// replace existing callback
    for (auto &cb : callbacks) {
      if (cb.address != nullptr && strcmp(cb.address, address) == 0) {
        cb.callback = callback;
        cb.compare = compare;
        return true;
      }
    }
    // add new entry
    Callback cb;
    cb.address = address;
    cb.callback = callback;
    callbacks.push_back(cb);
    return true;
  }

 protected:
  int write_pos = 0;
  uint8_t *read_format_start = nullptr;
  uint8_t *read_data = nullptr;
  bool is_log_active = false;
  OSCBinaryData binary_content;
  void *reference = nullptr;

  struct Callback {
    const char *address = nullptr;
    bool (*callback)(OSCData &data, void *ref) = nullptr;
    OSCCompare compare;
  };
  Vector<Callback> callbacks;

  bool compare(OSCCompare compare, const char *strRef, const char *strCompare) {
    switch (compare) {
      case OSCCompare::Matches:
        return StrView(strRef).matches(strCompare);
      case OSCCompare::Equals:
        return StrView(strRef).equals(strCompare);
      case OSCCompare::StartsWith:
        return StrView(strRef).startsWith(strCompare);
      case OSCCompare::EndsWith:
        return StrView(strRef).endsWith(strCompare);
      case OSCCompare::Contains:
        return StrView(strRef).contains(strCompare);
    }
    return false;
  }

  uint8_t *getEnd() { return binary_content.data + binary_content.len; }
};

}  // namespace audio_tools