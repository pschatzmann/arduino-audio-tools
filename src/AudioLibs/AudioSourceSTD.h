#pragma once

#include "AudioBasic/StrExt.h"
#include "AudioLogger.h"
#include "AudioTools/AudioSource.h"
#include "AudioLibs/Desktop/File.h"
#include <filesystem>

namespace audio_tools {

namespace fs = std::filesystem;

/**
 * @brief AudioSource using the standard C++ api
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceSTD : public AudioSource {
public:
  /// Default constructor
  AudioSourceSTD(const char *startFilePath = "/", const char *ext = ".mp3") {
    start_path = startFilePath;
    exension = ext;
    timeout_auto_next_value = 600000;
  }

  virtual void begin() override {
    TRACED();
    idx_pos = 0;
  }

  void end() {
  }

  virtual Stream *nextStream(int offset = 1) override {
    LOGI("nextStream: %d", offset);
    return selectStream(idx_pos + offset);
  }

  virtual Stream *selectStream(int index) override {
    LOGI("selectStream: %d", index);
    idx_pos = index;
    file_name = get(index);
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

  /// Provides the number of files (The max index is size()-1): WARNING this is very slow if you have a lot of files in many subdirectories
  long size() { 
    long count = 0;
    for (auto const& dir_entry : fs::recursive_directory_iterator(start_path)){
        if (isValidAudioFile(dir_entry))
          count++;
    }
    return count;
  }

protected:
  File file;
  size_t idx_pos = 0;
  const char *file_name;
  const char *exension = "";
  const char *start_path = nullptr;
  const char *file_name_pattern = "*";
  fs::directory_entry entry;

  const char* get(int idx){
      int count = 0;
      const char* result = nullptr;
      for (auto const& dir_entry : fs::recursive_directory_iterator(start_path)){
        if (isValidAudioFile(dir_entry)){
          if (count++==idx){
            entry = dir_entry;
            result = entry.path().c_str();
            break;
          }
        }
      }
      return result;
  }

  /// checks if the file is a valid audio file
  bool isValidAudioFile(fs::directory_entry file) {
    const std::filesystem::path& path = file.path();

    const char *file_name = path.filename().c_str();
    if (file.is_directory()) {
      LOGD("-> isValidAudioFile: '%s': %d", file_name, false);
      return false;
    }
    Str strFileTName(file_name);
    bool result = strFileTName.endsWithIgnoreCase(exension) 
                  && strFileTName.matches(file_name_pattern);
    LOGD("-> isValidAudioFile: '%s': %d", file_name, result);
    return result;
  }

};

} // namespace audio_tools