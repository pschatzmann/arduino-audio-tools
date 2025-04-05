#pragma once
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#include "AudioToolsConfig.h"

#ifdef IS_NOARDUINO
#define NOARD_OVR override
#else
#define NOARD_OVR
#endif

#ifndef FILE_READ
#define FILE_READ VFS_FILE_READ
#define FILE_WRITE VFS_FILE_WRITE
#define FILE_APPEND VFS_FILE_APPEND
#endif

namespace audio_tools {

enum FileMode { VFS_FILE_READ = 'r', VFS_FILE_WRITE = 'w', VFS_FILE_APPEND = 'a' };
enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };

/**
 * @brief Arduino File support using std::fstream
 * @author Phil Schatzmann
 * @ingroup player
 * @copyright GPLv3
 */
class VFSFile : public Stream {
 public:
  VFSFile() = default;
  VFSFile(const char* fn) { open(fn, VFS_FILE_READ); }
  VFSFile(const VFSFile& file) { open(file.name(), VFS_FILE_READ); }
  ~VFSFile() { end();}

  VFSFile& operator=(VFSFile file) {
    open(file.name(), VFS_FILE_READ);
    return *this;
  }

  void open(const char* name, FileMode mode = VFS_FILE_READ) {
    file_path = name;
    switch (mode) {
      case VFS_FILE_READ:
        stream.open(name, stream.binary | stream.in);
        is_read = true;
        break;
      case VFS_FILE_WRITE:
        stream.open(name, stream.binary | stream.trunc | stream.out);
        is_read = false;
        break;
      case VFS_FILE_APPEND:
        stream.open(name, stream.binary | stream.out);
        is_read = false;
        break;
    }
  }

  virtual bool begin() {
    // move to beginning
    return seek(0);
  }

  virtual void end() { stream.close(); }

  virtual int print(const char* str) NOARD_OVR {
    stream << str;
    return strlen(str);
  }

  virtual int println(const char* str = "") NOARD_OVR {
    stream << str << "\n";
    return strlen(str) + 1;
  }

  virtual int print(int number) NOARD_OVR {
    char buffer[80];
    int len = snprintf(buffer, 80, "%d", number);
    print(buffer);
    return len;
  }

  virtual int println(int number) {
    char buffer[80];
    int len = snprintf(buffer, 80, "%d\n", number);
    print(buffer);
    return len;
  }

  virtual void flush() override { stream.flush(); }

 
  virtual size_t write(uint8_t* str, size_t len) {
     stream.write((const char*)str, len);
     return len;
  }

  virtual size_t write(const uint8_t* str, size_t len) {
     stream.write((const char*)str, len);
     return len;
  }

  virtual size_t write(int32_t value) {
    stream.put(value);
    return 1;
  }

  virtual size_t write(uint8_t value) override {
    stream.put(value);
    return 1;
  }

  virtual int available() override {
    int result = size() - position();
    return result;
  };

  virtual int read() override { return stream.get(); }

  virtual size_t readBytes(char* data, size_t len) override {
    return readBytes((uint8_t*)data, len);
  }

  virtual size_t readBytes(uint8_t* data, size_t len) override {
    stream.read((char*)data, len);
    return stream ? len : stream.gcount();
  }

  virtual int peek() override { return stream.peek(); }

  bool seek(uint32_t pos, SeekMode mode) {
    if (is_read) {
      switch (mode) {
        case SeekSet:
          stream.seekg(pos, std::ios_base::beg);
          break;
        case SeekCur:
          stream.seekg(pos, std::ios_base::cur);
          break;
        case SeekEnd:
          stream.seekg(pos, std::ios_base::end);
          break;
      }
    } else {
      switch (mode) {
        case SeekSet:
          stream.seekp(pos, std::ios_base::beg);
          break;
        case SeekCur:
          stream.seekp(pos, std::ios_base::cur);
          break;
        case SeekEnd:
          stream.seekp(pos, std::ios_base::end);
          break;
      }
    }
    return stream.fail() == false;
  }

  bool seek(uint32_t pos) { return seek(pos, SeekSet); }

  size_t position() { return stream.tellp(); }

  size_t size() const {
    std::streampos fsize = 0;
    std::ifstream file(file_path, std::ios::binary);

    fsize = file.tellg();
    file.seekg(0, std::ios::end);
    fsize = file.tellg() - fsize;
    file.close();

    return fsize;
  }

  void close() { stream.close(); }

  const char* name() const { return file_path; }

  operator bool() { return stream.is_open(); }

 protected:
  std::fstream stream;
  bool is_read = true;
  const char* file_path = nullptr;
};

}  // namespace audio_tools