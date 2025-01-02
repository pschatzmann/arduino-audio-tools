#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Str.h"

#define MAX_FILE_LEN 256
#define MAX_FILE_COUNT 1000000
// Special logic for SDTFAT
#ifdef SDT_FAT_VERSION
#  define USE_SDFAT
#endif


namespace audio_tools {

/**
 * @brief We access the files directy with an index. The index is determined by
 * a recurseve tree walk thru the directory. Unfortunatly the SDTFAT library has
 * it's own API which is incompatible with the SD API
 */
template <class SDT, class FileT>
class SDDirect {
 public:
  SDDirect(SDT &sd) { p_sd = &sd; };

  void begin(const char *startDir, const char *extension,
             const char *file_name_pattern) {
    TRACED();
    this->start_dir = startDir;
    this->ext = extension;
    this->file_name_pattern = file_name_pattern;
    this->max_idx = -1;
  }

  /// Access file name by index
  const char *operator[](int idx) {
    if (max_idx != -1 && idx > max_idx) {
      return nullptr;
    }

    requested_idx = idx;
    actual_idx = -1;
    found = false;
    listDir(start_dir);
    if (!found) return nullptr;
    return result.c_str();
  }

  /// Provides the number of files
  long size() {
    if (max_idx == -1) {
      requested_idx = MAX_FILE_COUNT;
      actual_idx = -1;
      found = false;
      listDir(start_dir);
      max_idx = actual_idx;
    }
    return max_idx + 1;
  }

 protected:
  String result;
  const char *start_dir;
  SDT *p_sd = nullptr;
  int32_t actual_idx;
  size_t requested_idx;
  long max_idx = -1;
  bool found = false;
  List<String> file_path_stack;
  String file_path_str;

  const char *ext = nullptr;
  const char *file_name_pattern = nullptr;

  /// Writes the index file
  void listDir(const char *dirname) {
    if (dirname == nullptr) return;
    LOGD("listDir: %s", dirname);
    FileT root = open(dirname);
    if (!root) {
      LOGE("Open failed: %s", dirname);
      popPath();
      return;
    }
    if (!isDirectory(root)) {
      LOGD("Is not directory: %s", dirname);
      popPath();
      return;
    }
    if (StrView(dirname).startsWith(".")) {
      LOGD("Invalid file: %s", dirname);
      popPath();
      return;
    }
    if (isDirectory(root)) {
      rewind(root);
    }
    found = false;
    FileT file = openNext(root);
    while (file && !found) {
      if (isDirectory(file)) {
        String name = String(fileNamePath(file));
        LOGD("name: %s", name.c_str());
        pushPath(fileName(file));
        // recurseve call to get all files of this directory
        listDir(name.c_str());
      } else {
        const char *fn = fileNamePath(file);
        if (isValidAudioFile(file)) {
          actual_idx++;
          LOGD("File %s at index: %d", fn, actual_idx);
          if (actual_idx == requested_idx) {
            result = String(fn);
            found = true;
          }
        } else {
          LOGD("Ignoring %s", fn);
        }
      }
      file = openNext(root);
    }
    // stop processing and record maximum index
    if (!found && file_path_stack.size() == 0) {
      max_idx = actual_idx;
    }
    popPath();
  }

  void rewind(FileT &f) {
    TRACED();
#ifdef USE_SDFAT
    f.rewind();
#else
    f.rewindDirectory();
#endif
  }

  bool isDirectory(FileT &f) {
    bool result;
#ifdef USE_SDFAT
    result = f.isDir();
#else
    result = f.isDirectory();
#endif
    LOGD("isDirectory %s: %d", fileName(f), result);
    return result;
  }

  FileT openNext(FileT &dir) {
    TRACED();
#ifdef USE_SDFAT
    FileT result;
    if (!result.openNext(&dir, O_READ)) {
      LOGD("No next file");
    }
    return result;
#else
    return dir.openNextFile();
#endif
  }

  void pushPath(const char *name) {
    TRACED();
    LOGD("pushPath: %s", name);
    String nameStr(name);
    file_path_stack.push_back(nameStr);
  }

  void popPath() {
    TRACED();
    String str;
    file_path_stack.pop_back(str);
    LOGD("popPath: %s", str.c_str());
  }

  /// checks if the file is a valid audio file
  bool isValidAudioFile(FileT &file) {
    const char *file_name = fileName(file);
    if (file.isDirectory()) {
      LOGD("-> isValidAudioFile: '%s': %d", file_name, false);
      return false;
    }
    StrView strFileTName(file_name);
    bool result = strFileTName.endsWithIgnoreCase(ext) &&
                  strFileTName.matches(file_name_pattern) && !isHidden(file);
    LOGD("-> isValidAudioFile: '%s': %d", file_name, result);
    return result;
  }

  /// Returns the filename w/o path
  const char *fileName(FileT &file) {
#ifdef USE_SDFAT
    // add name
    static char name[MAX_FILE_LEN];
    file.getName(name, MAX_FILE_LEN);
    return name;
#else
    StrView tmp(file.name());
    int pos = 0;
    // remove directories
    if (tmp.contains("/")) {
      pos = tmp.lastIndexOf("/") + 1;
    }
    return file.name() + pos;
#endif
  }

  /// Returns the filename including the path
  const char *fileNamePath(FileT &file) {
#if defined(USE_SDFAT) || ESP_IDF_VERSION_MAJOR >= 4
    LOGD("-> fileNamePath: %s", fileName(file));
    file_path_str = start_dir;
    if (!file_path_str.endsWith("/")) {
      file_path_str += "/";
    }
    for (int j = 0; j < file_path_stack.size(); j++) {
      file_path_str += file_path_stack[j] + "/";
    }
    // add name
    static char name[MAX_FILE_LEN];
    strncpy(name, fileName(file), MAX_FILE_LEN);
    file_path_str += name;
    const char *result = file_path_str.c_str();
    LOGD("<- fileNamePath: %s", result);
    return result;
#else
    return file.name();
#endif
  }

  bool isHidden(FileT &f) {
#ifdef USE_SDFAT
    return f.isHidden();
#else
    return StrView(fileNamePath(f)).contains("/.");
#endif
  }

  FileT open(const char *name) {
    TRACED();
#ifdef USE_SDFAT
    FileT result;
    if (!result.open(name)) {
      if (name != nullptr) {
        LOGE("File open error: %s", name);
      } else {
        LOGE("File open error: name is null");
      }
    }
    return result;
#else
    return p_sd->open(name);
#endif
  }
};

}  // namespace audio_tools