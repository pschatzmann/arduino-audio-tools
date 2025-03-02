#pragma once

#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"

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

class Str : public StrView {
 public:
  Str() = default;

  Str(int initialAllocatedLength) : StrView() {
    maxlen = initialAllocatedLength;
    is_const = false;
    grow(maxlen);
  }

  Str(const char *str) : StrView() {
    if (str != nullptr) {
      len = strlen(str);
      maxlen = len;
      grow(maxlen);
      if (chars != nullptr) {
        strcpy(chars, str);
      }
    }
  }

  /// Convert StrView to Str
  Str(StrView &source) : StrView() { set(source); }

  /// Copy constructor
  Str(Str &source) : StrView() { set(source); }

  /// Move constructor
  Str(Str &&obj) { move(obj); }

  /// Destructor
  ~Str() {
    len = maxlen = 0;
    chars = nullptr;
  }

  /// Move assignment
  Str &operator=(Str &&obj) {
    return move(obj);
  }

  /// Copy assingment
  Str &operator=(Str &obj) {
    //assert(&obj!=nullptr);
    set(obj.c_str());
    return *this;
  };

  bool isOnHeap() override { return true; }

  bool isConst() override { return false; }

  void operator=(const char *str) override { set(str); }

  void operator=(char *str) override { set(str); }

  void operator=(int v) override { set(v); }

  void operator=(double v) override { set(v); }

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
    char result[new_size + 1];
    memset(result,0, new_size+1);
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

  void clear() override {
    len = 0;
    maxlen = 0;
    vector.resize(0);
    chars = nullptr;
  }

  void swap(Str &other){
    int tmp_len = len;
    int tmp_maxlen = maxlen;
    len = other.len;
    maxlen = other.maxlen;
    vector.swap(other.vector);
    chars = vector.data();
    other.chars = other.vector.data();
  }


 protected:
  Vector<char> vector;

  Str& move(Str &other) {
    swap(other);
    other.clear();
    return *this;
  }

  bool grow(int newMaxLen) override {
    bool grown = false;
    assert(newMaxLen < 1024 * 10);
    if (newMaxLen < 0) return false;

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
    szBuffer[0] = charToInt(pString[0]);    // make the B to 11 -- 00001011
    szBuffer[1] = charToInt(pString[1]);    // make the 0 to 0 -- 00000000
    ch = (szBuffer[0] << 4) | szBuffer[1];  // to change the BO to 10110000
    return ch;
  }
};

}  // namespace audio_tools