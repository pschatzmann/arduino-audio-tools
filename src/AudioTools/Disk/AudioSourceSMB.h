#pragma once

#include "AudioSource.h"
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "SMB.h"

namespace audio_tools {

/**
 * @brief An AudioSource that uses the SMBClient of the
 * https://github.com/pschatzmann/TinySMB library to retrieve the files from a
 * Windows, macOS, Samba or NAS share. You need to provide an SMBClient object
 * to the constructor and make sure that you call begin() on it before you
 * access the files.
 *
 * The directory is scanned recursively and the files that match the
 * extension are played in the order in which the server lists them. The file
 * names are not stored: the share is listed each time a new file is
 * selected, so only the name of the actual file is kept in memory.
 *
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceSMB : public AudioSource {
 public:
  /// Default constructor: provide the (opened) SMBClient, the start
  /// directory relative to the share, the file extension and optionally the
  /// max number of files (0 = unlimited)
  AudioSourceSMB(smb::SMBClient& client, const char* path = "/",
                 const char* ext = nullptr, int files = 0) {
    p_client = &client;
    timeout_auto_next_value = 5000;
    setDirectory(path);
    p_ext = ext;
    setMaxFiles(files);
  }

  /// Counts the audio files in the start directory
  bool begin() override {
    TRACED();
    end();
    if (p_client == nullptr) return false;
    file_count = 0;
    if (!walk(directory, -1, file_count, nullptr)) {
      LOGE("Could not list directory: %s", directory.c_str());
      file_count = -1;
      return false;
    }
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
    int n = 0;
    if (p_client == nullptr || !walk(directory, index, n, &path) ||
        path.isEmpty()) {
      LOGW("No file at index %d", index);
      return nullptr;
    }
    idx = index;
    return openFile(path.c_str());
  }

  /// Returns the actual index of the stream
  int index() override { return idx; }

  /// Opens the indicated file. If the path is a directory, it becomes the new
  /// start directory and the first file is opened.
  Stream* selectStream(const char* path) override {
    TRACED();
    if (p_client == nullptr || path == nullptr) return nullptr;
    smb::FileInfo info;
    if (!p_client->stat(path, info)) {
      LOGE("Path not found: %s", path);
      return nullptr;
    }
    if (info.isDirectory) {
      setDirectory(path);
      if (!begin()) return nullptr;
      return selectStream(0);
    }
    return openFile(path);
  }

  /// provides the actual file name with path
  const char* toStr() override { return file.path(); }

  /// Defines the start directory: call begin() afterwards
  void setDirectory(const char* path) {
    directory = "/";
    if (path != nullptr) directory = path;
    if (!directory.startsWith("/")) {
      Str tmp{"/"};
      tmp += directory.c_str();
      directory = tmp.c_str();
    }
  }

  /// Defines the max number of files (if value is >0)
  void setMaxFiles(int maxCount) { max_files = maxCount; }

  /// Defines the file extension filter (e.g. "mp3"): nullptr for all files
  void setFileExtension(const char* ext) { p_ext = ext; }

  /// Defines the read buffer size of the opened files (default 4096)
  void setBufferSize(size_t size) { buffer_size = size; }

  /// Returns the number of files that were found by begin()
  size_t size() { return file_count < 0 ? 0 : file_count; }

 protected:
  smb::SMBClient* p_client = nullptr;
  smb::SMBFile file;
  Str directory;
  int idx = 0;
  int file_count = -1;  // -1: unknown
  size_t max_files = 0;
  size_t buffer_size = 0;
  const char* p_ext = nullptr;

  Stream* openFile(const char* path) {
    file.close();  // close the previous file
    LOGI("open %s", path);
    file = p_client->open(path, "r");
    if (!file || file.isDirectory()) {
      LOGE("Could not open %s", path);
      return nullptr;
    }
    if (buffer_size > 0) file.setBufferSize(buffer_size);
    return &file;
  }

  /// Lists the directory recursively and counts the matching files in n. If
  /// target >= 0 we stop at the file with this index and provide its path in
  /// result. Returns false if the directory could not be listed.
  bool walk(Str& dir, int target, int& n, Str* result) {
    bool done = false;
    bool ok = p_client->listDir(dir.c_str(), [&](const smb::FileInfo& info) {
      if (info.name.empty() || info.name == "." || info.name == "..")
        return true;
      Str path{dir.c_str()};
      if (!path.endsWith("/")) path += "/";
      path += info.name.c_str();
      if (info.isDirectory) {
        walk(path, target, n, result);
      } else if (endsWith(info.name.c_str(), p_ext)) {
        if (n == target && result != nullptr) {
          *result = path.c_str();
        }
        n++;
      }
      done = (target >= 0 && n > target) || isMaxReached(n);
      return !done;
    });
    return ok;
  }

  bool isMaxReached(int n) { return max_files > 0 && n >= (int)max_files; }

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
