#pragma once

#include "AudioBasic/Str.h"

namespace audio_tools {

/**
 * @brief Str which keeps the data on the heap. We grow the allocated
 * memory only if the copy source is not fitting.
 *
 * While it should be avoided to use a lot of heap allocatioins in
 * embedded devices it is sometimes more convinent to allocate a string
 * once on the heap and have the insurance that it might grow
 * if we need to process an unexpected size.
 *
 * We also need to use this if we want to manage a vecor of strings.
 *
 * @ingroup string
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class StrExt : public Str {

public:
  StrExt() = default;

  StrExt(int initialAllocatedLength) : Str() {
    maxlen = initialAllocatedLength;
    is_const = false;
  }

  StrExt(Str &source) : Str() { set(source); }

  StrExt(StrExt &source) : Str() { set(source); }

  StrExt(const char *str) : Str() {
    if (str != nullptr) {
      len = strlen(str);
      maxlen = len;
      grow(maxlen);
      if (chars != nullptr) {
        strcpy(chars, str);
      }
    }
  }

  // move constructor
  StrExt(StrExt &&obj) = default;

  // move assignment
  StrExt &operator=(StrExt &&obj) {
    set(obj.c_str());
    return *this;
  }

  // copy assingment
  StrExt &operator=(StrExt &obj) {
    set(obj.c_str());
    return *this;
  };

  ~StrExt() {}

  bool isOnHeap() override { return true; }

  bool isConst() override { return false; }

  void operator=(const char *str) { set(str); }

  void operator=(char *str) { set(str); }

  void operator=(int v) { set(v); }

  void operator=(double v) { set(v); }

  size_t capacity() { return maxlen; }

  void setCapacity(size_t newLen) { grow(newLen); }

  // make sure that the max size is allocated
  void allocate(int len = -1) {
    int new_size = len < 0 ? maxlen : len;
    grow(new_size);
    this->len = new_size;
  }

  /// assigns a memory buffer
  void copyFrom(const char *source, int len, int maxlen = 0) {
    this->maxlen = maxlen == 0 ? len : maxlen;
    grow(this->maxlen);
    if (this->chars != nullptr) {
      this->len = len;
      this->is_const = false;
      memmove(this->chars, source, len);
      this->chars[len] = 0;
    }
  }

  /// Fills the string with len chars
  void setChars(char c, int len) {
    grow(this->maxlen);
    if (this->chars != nullptr) {
      for (int j = 0; j < len; j++) {
        this->chars[j] = c;
      }
      this->len = len;
      this->is_const = false;
      this->chars[len] = 0;
    }
  }

  /// url encode the string
  void urlEncode() {
    char temp[4];
    int new_size = 0;
    // Calculate the new size
    for (size_t i = 0; i < len; i++) {
      urlEncodeChar(chars[i], temp, 4);
      new_size += strlen(temp);
    }
    // build new string
    char result[new_size + 1] = {0};
    for (size_t i = 0; i < len; i++) {
      urlEncodeChar(chars[i], temp, 4);
      strcat(result, temp);
    }
    // save result
    grow(new_size);
    strcpy(chars, result);
    this->len = strlen(temp);
  }

  /// decodes a url encoded string 
  void urlDecode() {
    char szTemp[2];
    size_t i = 0;
    size_t result_idx = 0;
    while (i < len) {
      if (chars[i] == '%') {
        szTemp[0] = chars[i + 1];
        szTemp[1] = chars[i + 2];
        chars[result_idx] = strToBin(szTemp);
        i = i + 3;
      } else if (chars[i] == '+') {
        chars[result_idx] = ' ';
        i++;
      } else {
        chars[result_idx] += chars[i];
        i++;
      }
      result_idx++;
    }
    chars[result_idx] = 0;
    this->len = result_idx;
  }

protected:
  Vector<char> vector;

  bool grow(int newMaxLen) {
    bool grown = false;
    assert(newMaxLen < 1024 * 10);

    if (chars == nullptr || newMaxLen > maxlen) {
      LOGD("grow(%d)", newMaxLen);

      grown = true;
      // we use at minimum the defined maxlen
      int newSize = newMaxLen > maxlen ? newMaxLen : maxlen;
      vector.resize(newSize + 1);
      chars = &vector[0];
      maxlen = newSize;
    }
    return grown;
  }

  void urlEncodeChar(char c, char *result, int maxLen) {
    if (isalnum(c)) {
      snprintf(result, maxLen, "%c", c);
    } else if (isspace(c)) {
      snprintf(result, maxLen, "%s", "+");
    } else {
      snprintf(result, maxLen, "%%%X%X", c >> 4, c % 16);
    }
  }

  char charToInt(char ch) {
    if (ch >= '0' && ch <= '9') {
      return (char)(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
      return (char)(ch - 'a' + 10);
    }
    if (ch >= 'A' && ch <= 'F') {
      return (char)(ch - 'A' + 10);
    }
    return -1;
  }

  char strToBin(char *pString) {
    char szBuffer[2];
    char ch;
    szBuffer[0] = charToInt(pString[0]);   // make the B to 11 -- 00001011
    szBuffer[1] = charToInt(pString[1]);   // make the 0 to 0 -- 00000000
    ch = (szBuffer[0] << 4) | szBuffer[1]; // to change the BO to 10110000
    return ch;
  }
};

} // namespace audio_tools
