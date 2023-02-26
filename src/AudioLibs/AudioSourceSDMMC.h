#pragma once

#include "AudioBasic/StrExt.h"
#include "AudioLogger.h"
#include "AudioTools/AudioSource.h"
#include "FS.h"
#include "SD_MMC.h"
#include "AudioLibs/SDDirect.h"

namespace audio_tools {

/**
 * @brief ESP32 AudioSource for AudioPlayer using an SD card as data source.
 * This class is based on the Arduino SD_MMC implementation
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       12
 *    D3       13
 *    CMD      15
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      14
 *    VSS      GND
 *    D0       2  (add 1K pull up after flashing)
 *    D1       4
 *
 *  On the AI Thinker boards the pin settings should be On, On, On, On, On,
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceSDMMC : public AudioSource {
public:
  /// Default constructor
  AudioSourceSDMMC(const char *startFilePath = "/", const char *ext = ".mp3", bool setupIndex=true) {
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
  }

  virtual void begin() override {
    TRACED();
    if (!is_sd_setup) {
      if (!SD_MMC.begin("/sdcard", true)) {
        LOGE("SD_MMC.begin failed");
        return;
      }
      is_sd_setup = true;
    }
    idx.begin(start_path, exension, file_name_pattern);
    idx_pos = 0;
  }

  void end() {
    SD_MMC.end();
    is_sd_setup = false;
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
    file = SD_MMC.open(file_name);
    return file ? &file : nullptr;
  }

  virtual Stream *selectStream(const char *path) override {
    file.close();
    file = SD_MMC.open(path);
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

  /// Provides the number of files (The max index is size()-1): WARNING this is very slow if you have a lot of files in many subdirectories
  long size() { return idx.size();}

protected:
  SDDirect<fs::SDMMCFS,fs::File> idx{SD_MMC};
  File file;
  size_t idx_pos = 0;
  const char *file_name;
  const char *exension = nullptr;
  const char *start_path = nullptr;
  const char *file_name_pattern = "*";
  bool setup_index = true;
  bool is_sd_setup = false;
};

} // namespace audio_tools