#pragma once

#include "AudioLogger.h"
#include "AudioTools/Disk/AudioSource.h"
#include "AudioTools/AudioLibs/Desktop/File.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
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

  virtual bool begin() override {
    TRACED();
    idx_pos = 0;
    return true;
  }

  virtual void end() {
  }

  virtual Stream *nextStream(int offset = 1) override {
    LOGI("nextStream: %d", offset);
    Stream* s = selectStream(idx_pos + offset);
    if (s == nullptr && offset > 0) {
      LOGI("Wrapping to start of directory");
      idx_pos = 0;
      s = selectStream(idx_pos);
    }
    return s;
  }

  virtual Stream *selectStream(int index) override {
    // Determine total mp3 file count to normalize index
    long count = size();
    LOGI("selectStream: %d of %d", index, (int) count);
    if (count <= 0) {
      LOGW("No audio files found under: %s", start_path ? start_path : "<null>");
      return nullptr;
    }
    int norm = index % count;
    if (norm < 0) norm += count;
    idx_pos = norm;
    file_name = get(norm);
    if (file_name==nullptr) {
      LOGW("Could not resolve index %d (normalized %d)", index, norm);
      return nullptr;
    }
    LOGI("Using file %s", file_name);
    file.close();
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
    if (count == 0){
      for (auto const& dir_entry : fs::recursive_directory_iterator(start_path)){
          if (isValidAudioFile(dir_entry))
            count++;
      }
    }
    return count;
  }

protected:
  File file;
  size_t idx_pos = 0;
  const char *file_name;
  std::string current_path; // holds persistent path string
  const char *exension = "";
  const char *start_path = nullptr;
  const char *file_name_pattern = "*";
  fs::directory_entry entry;
  long count = 0;

  const char* get(int idx){
      int count = 0;
      const char* result = nullptr;
      for (auto const& dir_entry : fs::recursive_directory_iterator(start_path)){
        if (isValidAudioFile(dir_entry)){
          if (count++==idx){
            entry = dir_entry;
            current_path = entry.path().string();
            result = current_path.c_str();
            break;
          }
        }
      }
      return result;
  }

  /// checks if the file is a valid audio file
  bool isValidAudioFile(fs::directory_entry file) {
    const std::filesystem::path path = file.path();
    const std::string filename = path.filename().string();
    const char *file_name_c = filename.c_str();
    if (file.is_directory()) {
      LOGD("-> isValidAudioFile: '%s': %d", file_name_c, false);
      return false;
    }
    StrView strFileTName(file_name_c);
    bool result = strFileTName.endsWithIgnoreCase(exension) 
                  && strFileTName.matches(file_name_pattern);
    LOGD("-> isValidAudioFile: '%s': %d", file_name_c, result);
    return result;
  }

};

} // namespace audio_tools