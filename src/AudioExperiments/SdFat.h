#pragma once

#include "Stream.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#define SD_SCK_MHZ(maxMhz) (1000000UL * (maxMhz))
#define SPI_FULL_SPEED SD_SCK_MHZ(50)
#define SPI_DIV3_SPEED SD_SCK_MHZ(16)
#define SPI_HALF_SPEED SD_SCK_MHZ(4)
#define SPI_DIV6_SPEED SD_SCK_MHZ(8)
#define SPI_QUARTER_SPEED SD_SCK_MHZ(2)
#define SPI_EIGHTH_SPEED SD_SCK_MHZ(1)
#define SPI_SIXTEENTH_SPEED SD_SCK_HZ(500000)

#define O_RDONLY ios::in          ///< Open for reading only.
#define O_WRONLY ios::out         ///< Open for writing only.
#define O_RDWR ios::in | ios::out ///< Open for reading and writing.
#define O_AT_END ios::ate         ///< Open at EOF.
#define O_APPEND ios::ate         ///< Set append mode.
#define O_CREAT ios::trunc                 ///< Create file if it does not exist.
#define O_TRUNC ios::trunc        ///< Truncate file to zero length.
#define O_EXCL 0                  ///< Fail if the file exists.
#define O_SYNC 0                  ///< Synchronized write I/O operations.
#define O_READ O_RDONLY
#define O_WRITE O_WRONLY

#ifndef SS
#define SS 0
#endif

using namespace std;

/**
 * @brief C++ std based emulatoion of SdSpiConfig
 *
 */

class SdSpiConfig {
  SdSpiConfig(int a = 0, int b = 0, int c = 0) {}
};

/**
 * @brief C++ std based emulatoion ofr SdFat
 *
 */
class SdFat {
public:
  bool begin(int cs = SS, int speed = 0) { return true; }
  bool begin(SdSpiConfig &cfg) { return true; }
  void errorHalt(const char *msg) {
    Serial.println(msg);
    exit(0);
  }
  void initErrorHalt() { exit(0); }

  bool exists(char *name) {
    struct stat info;
    return stat(name, &info) == 0;
  }
};
/**
 * @brief C++ std based emulatoion ofr SdFile
 *
 */
class SdFile : public Stream {
public:
  bool isOpen() {
    bool result = file.is_open();
    return result;
  }

  bool open(const char *name, int flags) {
    this->filename = name;
    struct stat info;
    if ((flags == O_RDONLY) && stat(name, &info) != 0)
      return false;
    else if (info.st_mode & S_IFDIR) {
      is_dir = true;
      size = 0;
      dir_path = std::filesystem::path(filename);
      iterator = std::filesystem::directory_iterator({dir_path});
      return true;
    } else {
      is_dir = false;
      size = info.st_size;
      file.open(filename.c_str(), flags);
      return isOpen();
    }
  }

  bool close() {
    file.close();
    return !isOpen();
  }

  size_t readBytes(uint8_t *buffer, size_t length) override {
    return file.readsome((char *)buffer, length);
  }
  size_t write(const uint8_t *buffer, size_t size) override {
    file.write((const char *)buffer, size);
    return file.good() ? size : 0;
  }
  size_t write(uint8_t ch) override {
    file.put(ch);
    return file.good() ? 1 : 0;
  }
  int available() override { return size - file.tellg(); };
  int availableForWrite() override { return 1024; }
  int read() override { return file.get(); }
  int peek() override { return file.peek(); }
  void flush() override {}
  void getName(char *str, size_t len) { strncpy(str, filename.c_str(), len); }
  bool isDir() { return is_dir; }
  bool isHidden() { return false; }
  bool rewind() {
    pos = 0;
    return is_dir;
  }
  bool openNext(SdFile &dir, int flags = O_RDONLY) {
    if (dir.isDir() && dir.iterator != end(dir.iterator)) {
      std::filesystem::directory_entry entry = *dir.iterator++;
      return open(entry.path().c_str(), flags);
    }
    return false;
  }
  int dirIndex() { return pos; }

protected:
  std::fstream file;
  size_t size = 0;
  bool is_dir;
  std::filesystem::directory_iterator iterator;
  std::filesystem::path dir_path;
  int pos = 0;
  std::string filename;
};