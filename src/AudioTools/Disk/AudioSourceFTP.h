#pragma once

#include "AudioSource.h"
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "FTPClient.h"

namespace audio_tools {

/**
 * @brief An AudioSource that uses the
 * https://github.com/pschatzmann/TinyFTPClient library to retrieve files from a
 * FTP Server. You need to provide an FTPClient object to the constructor and
 * make sure that you open it before you access the files.
 *
 * The file names are not stored: the directory is listed on the server each
 * time a new file is selected and the n-th matching file is opened. So only
 * the name of the actual file is kept in memory.
 *
 * @note supported only by Arduino platforms!
 *
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class ClientType>
class AudioSourceFTP : public AudioSource {
 public:
  /// Default constructor: Provide the client class as template argument e.g.
  /// AudioSourceFTP<WiFiClient> source(client, path, ext);.
  AudioSourceFTP(FTPClient<ClientType>& client, const char* path,
                 const char* ext, int files = 0) {
    p_client = &client;
    timeout_auto_next_value = 5000;
    directory = path != nullptr ? path : "/";
    p_ext = ext;
    setMaxFiles(files);
  }

  /// Checks that the directory contains some files
  bool begin() override {
    TRACED();
    idx = 0;
    file.close();
    file_count = count();
    LOGI("Number of files: %d", file_count);
    return file_count > 0;
  }

  /// Closes the actual file
  void end() {
    idx = 0;
    file.close();
  }

  /// Returns next audio stream
  Stream* nextStream(int offset) override {
    return selectStream(idx + offset);
  };

  /// Returns previous audio stream
  Stream* previousStream(int offset) override { return nextStream(-offset); };

  /// Returns audio stream at the indicated index (the index is zero based, so
  /// the first value is 0!)
  Stream* selectStream(int index) override {
    if (!isValidIdx(index)) return nullptr;
    Str path;
    if (!findFile(index, path)) {
      LOGW("No file at index %d", index);
      return nullptr;
    }
    idx = index;
    return openFile(path.c_str());
  }

  /// Returns the actual index of the stream
  int index() override { return idx; }

  /// Opens the indicated file: if the path does not match the file extension
  /// it is considered to be a directory and the first file is opened.
  Stream* selectStream(const char* path) override {
    TRACED();
    if (path == nullptr) return nullptr;
    if (p_ext != nullptr && endsWith(path, p_ext)) return openFile(path);
    directory = path;
    if (!begin()) return nullptr;
    return selectStream(0);
  }

  /// provides the actual file name
  const char* toStr() override { return file.name(); }

  /// Defines the max number of files (if value is >0)
  void setMaxFiles(int maxCount) { max_files = maxCount; }

  /// Defines the file extension filter (e.g. "mp3"): nullptr for all files
  void setFileExtension(const char* ext) { p_ext = ext; }

  /// Returns the number of files that were found by begin()
  size_t size() { return file_count < 0 ? 0 : file_count; }

 protected:
  FTPClient<ClientType>* p_client = nullptr;
  FTPFile file;
  Str directory;
  int idx = 0;
  int file_count = -1;  // -1: unknown
  size_t max_files = 0;
  const char* p_ext = nullptr;

  Stream* openFile(const char* path) {
    file.close();  // close the previous file
    LOGI("open %s", path);
    file = p_client->open(path);
    if (!file) {
      LOGE("Could not open %s", path);
      return nullptr;
    }
    return &file;
  }

  /// Lists the directory and provides the path of the n-th matching file
  bool findFile(int index, Str& path) {
    if (p_client == nullptr) return false;
    int n = 0;
    for (auto it = p_client->ls(directory.c_str()); it != it.end(); ++it) {
      const char* name = it.fileName();
      if (!endsWith(name, p_ext)) continue;
      if (n++ == index) {
        toPath(name, path);
        return true;
      }
    }
    return false;
  }

  /// Lists the directory and counts the matching files
  int count() {
    if (p_client == nullptr) return 0;
    int n = 0;
    for (auto it = p_client->ls(directory.c_str()); it != it.end(); ++it) {
      if (!endsWith(it.fileName(), p_ext)) continue;
      if (++n >= max_files && max_files > 0) break;
    }
    return n;
  }

  /// Some servers list the names with the directory, others without it
  void toPath(const char* name, Str& result) {
    if (strchr(name, '/') != nullptr) {
      result = name;
      return;
    }
    result = directory.c_str();
    if (!result.isEmpty() && !result.endsWith("/")) result += "/";
    result += name;
  }

  bool isValidIdx(int index) {
    if (index < 0) return false;
    if (max_files > 0 && index >= (int)max_files) return false;
    if (file_count >= 0 && index >= file_count) {
      LOGW("index %d is out of range (size: %d)", index, file_count);
      return false;
    }
    return true;
  }

  bool endsWith(const char* file, const char* ext) {
    if (file == nullptr) return false;
    if (ext == nullptr) return true;
    return StrView(file).endsWithIgnoreCase(ext);
  }
};

}  // namespace audio_tools
