#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Str.h"

#define MAX_FILE_LEN 256

// Special logic for SDTFAT
#ifdef SDT_FAT_VERSION
#  define USE_SDFAT
#endif

namespace audio_tools {

/**
 * @brief We store all the relevant file names in an sequential index
 * file. Form there we can access them via an index
 */
template <class SDT, class FileT>
class SDIndex {
 public:
  SDIndex(SDT &sd) { p_sd = &sd; };
  void begin(const char *startDir, const char *extension,
             const char *file_name_pattern, bool setupIndex = true) {
    TRACED();
    this->start_dir = startDir;
    this->ext = extension;
    this->file_name_pattern = file_name_pattern;
    idx_path = filePathString(startDir, "idx.txt");
    idx_defpath = filePathString(startDir, "idx-def.txt");
    int idx_file_size = indexFileTSize();
    LOGI("Index file size: %d", idx_file_size);
    String keyNew =
        String(startDir) + "|" + extension + "|" + file_name_pattern;
    String keyOld = getIndexDef();
    if (setupIndex && (keyNew != keyOld || idx_file_size == 0)) {
      FileT idxfile = p_sd->open(idx_path.c_str(), FILE_WRITE);
      LOGW("Creating index file");
      listDir(idxfile, startDir);
      LOGI("Indexing completed");
      idxfile.close();
      // update index definition file
      saveIndexDef(keyNew);
    }
  }

  void ls(Print &p, const char *startDir, const char *extension,
          const char *file_name_pattern = "*") {
    TRACED();
    this->ext = extension;
    this->file_name_pattern = file_name_pattern;
    listDir(p, startDir);
    file_path_stack.clear();
    file_path_str.clear();
  }

  /// Access file name by index
  const char *operator[](int idx) {
    // return null when inx too big
    if (max_idx >= 0 && idx > max_idx) {
      LOGE("idx %d > size %d", idx, max_idx);
      return nullptr;
    }
    // find record
    FileT idxfile = p_sd->open(idx_path.c_str());
    int count = 0;

    if (idxfile.available() == 0) {
      LOGE("Index file is empty");
    }

    bool found = false;
    while (idxfile.available() > 0 && !found) {
      result = idxfile.readStringUntil('\n');

      // result c-string
      char *c_str = (char *)result.c_str();
      // remove potential cr character - hax to avoid any allocations
      int lastPos = result.length() - 1;
      if (c_str[lastPos] == 13) {
        c_str[lastPos] = 0;
      }

      LOGD("%d -> %s", count, c_str);
      if (count == idx) {
        found = true;
      }
      count++;
    }
    if (!found) {
      max_idx = count;
    }
    idxfile.close();

    return found ? result.c_str() : nullptr;
  }

  long size() {
    if (max_idx == -1) {
      FileT idxfile = p_sd->open(idx_path.c_str());
      int count = 0;

      while (idxfile.available() > 0) {
        result = idxfile.readStringUntil('\n');
        // result c-string
        char *c_str = (char *)result.c_str();
        count++;
      }
      idxfile.close();
      max_idx = count;
    }
    return max_idx;
  }

 protected:
  String result;
  String idx_path;
  String idx_defpath;
  SDT *p_sd = nullptr;
  List<String> file_path_stack;
  String file_path_str;
  const char *start_dir;

  const char *ext = nullptr;
  const char *file_name_pattern = nullptr;
  long max_idx = -1;

  String filePathString(const char *name, const char *suffix) {
    String result = name;
    return result.endsWith("/") ? result + suffix : result + "/" + suffix;
  }

  /// Writes the index file
  void listDir(Print &idxfile, const char *dirname) {
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

    rewind(root);
    FileT file = openNext(root);
    while (file) {
      if (isDirectory(file)) {
        String name = String(fileNamePath(file));
        LOGD("name: %s", name.c_str());
        pushPath(fileName(file));
        listDir(idxfile, name.c_str());
      } else {
        const char *fn = fileNamePath(file);
        if (isValidAudioFile(file)) {
          LOGD("Adding file to index: %s", fn);
          idxfile.println(fn);
        } else {
          LOGD("Ignoring %s", fn);
        }
      }
      file = openNext(root);
    }
    popPath();
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
    LOGD("pushPath: %s", name);
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
    TRACED();
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

  String getIndexDef() {
    FileT idxdef = p_sd->open(idx_defpath.c_str());
    String key1 = idxdef.readString();
    idxdef.close();
    return key1;
  }
  void saveIndexDef(String keyNew) {
    FileT idxdef = p_sd->open(idx_defpath.c_str(), FILE_WRITE);
    idxdef.write((const uint8_t *)keyNew.c_str(), keyNew.length());
    idxdef.close();
  }

  size_t indexFileTSize() {
    FileT idxfile = p_sd->open(idx_path.c_str());
    size_t result = idxfile.size();
    idxfile.close();
    return result;
  }

  void rewind(FileT &f) {
    TRACED();
#ifdef USE_SDFAT
    f.rewind();
#else
    f.rewindDirectory();
#endif
  }

  /// Returns the filename w/o the path
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
      LOGE("FileT open error: %s", name);
    }
    return result;
#else
    return p_sd->open(name);
#endif
  }
};

}  // namespace audio_tools