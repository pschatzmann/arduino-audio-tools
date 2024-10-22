#pragma once

#include "AudioLogger.h"
#include "AudioTools/CoreAudio/AudioSource.h"
#include "AudioTools/AudioLibs/SDDirect.h"
#include <LittleFS.h>

namespace audio_tools {

/**
 * @brief ESP32 AudioSource for AudioPlayer using an the LittleFS file system
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceLittleFS : public AudioSource {
public:
  /// Default constructor
  AudioSourceLittleFS(const char *startFilePath = "/", const char *ext = ".mp3") {
    start_path = startFilePath;
    exension = ext;
  }

  virtual void begin() override {
    TRACED();
    if (!is_sd_setup) {
      while (!LittleFS.begin()) {
        LOGE("LittleFS.begin failed");
        delay(1000);
      }
      is_sd_setup = true;
    }
    idx.begin(start_path, exension, file_name_pattern);
    idx_pos = 0;
  }

  void end() {
    LittleFS.end();
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
    file = LittleFS.open(file_name,"r");
    return file ? &file : nullptr;
  }

  virtual Stream *selectStream(const char *path) override {
    file.close();
    file = LittleFS.open(path,"r");
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
#ifdef RP2040_HOWER
  SDDirect<FS,File> idx{LittleFS};
#else
  SDDirect<fs::LittleFSFS,fs::File> idx{LittleFS};
#endif
  File file;
  size_t idx_pos = 0;
  const char *file_name;
  const char *exension = nullptr;
  const char *start_path = nullptr;
  const char *file_name_pattern = "*";
  bool is_sd_setup = false;
};

} // namespace audio_tools