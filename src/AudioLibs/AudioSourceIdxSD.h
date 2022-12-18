#pragma once
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include "AudioLogger.h"
#include "AudioBasic/StrExt.h"
#include "AudioTools/AudioSource.h"
#include "AudioLibs/SDIndex.h"

namespace audio_tools {

/**
 * @brief ESP32 AudioSource for AudioPlayer using an SD card as data source.
 * This class is based on the Arduino SD implementation
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       -
 *    D3       SS
 *    CMD      MOSI
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      SCK
 *    VSS      GND
 *    D0       MISO
 *    D1       -
 *
 *  On the AI Thinker boards the pin settings should be On, On, On, On, On,
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceIdxSD : public AudioSource {
public:
  /// Default constructor
  AudioSourceIdxSD(const char *startFilePath = "/", const char *ext = ".mp3", int chipSelect = PIN_CS, bool setupIndex=true) {
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
    cs = chipSelect;
  }

  virtual void begin() override {
    TRACED();
    static bool is_sd_setup = false;
    if (!is_sd_setup) {
      if (!SD.begin(cs)) {
        LOGE("SD.begin cs=%d failed", cs);
        return;
      }
      is_sd_setup = true;
    }
    idx.begin(start_path, exension, file_name_pattern, setup_index);
    idx_pos = 0;
  }

  virtual Stream *nextStream(int offset = 1) override {
    LOGI("nextStream: %d", offset);
    return selectStream(idx_pos + offset);
  }

  virtual Stream *selectStream(int index) override {
    LOGI("selectStream: %d", index);
    idx_pos = index;
    file_name = idx[index];
    if (file_name==nullptr) return nullptr;
    LOGI("Using file %s", file_name);
    file = SD.open(file_name);
    return file ? &file : nullptr;
  }

  virtual Stream *selectStream(const char *path) override {
    file.close();
    file = SD.open(path);
    file_name = file.name();
    LOGI("-> selectStream: %s", path);
    return file ? &file : nullptr;
  }

  /// Defines the regex filter criteria for selecting files. E.g. ".*Bob
  /// Dylan.*"
  void setFileFilter(const char *filter) { file_name_pattern = filter; }

  /// Provides the current index position
  int index() { return idx_pos; }

  /// provides the actual file name
  const char *toStr() { return file_name; }

  // provides default setting go to the next
  virtual bool isAutoNext() { return true; }

  /// Allows to "correct" the start path if not defined in the constructor
  virtual void setPath(const char *p) { start_path = p; }

protected:
  SDIndex<fs::SDFS,fs::File> idx{SD};
  File file;
  size_t idx_pos = 0;
  const char *file_name;
  const char *exension = nullptr;
  const char *start_path = nullptr;
  const char *file_name_pattern = "*";
  bool setup_index = true;
  int cs;


};

} // namespace audio_tools