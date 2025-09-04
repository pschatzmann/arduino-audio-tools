#include "FTPClient.h"
#include "AudioSource.h"
#include "vector"

namespace audio_tools {

/**
 * @brief An AudioSource that uses the
 * https://github.com/pschatzmann/TinyFTPClient library to retrieve files from a
 * FTP Server. You need to provide an FTPClient object to the constructor and
 * make sure that you open it before you access the files. The storage of the
 * expanded file names is done on the heap, so in order to limit the requested
 * memory we can limit the number of files.
 *
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class ClientType>
class AudioSourceFTP : public AudioSource {
 public:
  /// Default constructor: Provide the client class as template argument e.g. AudioSourceFTP<WiFiClient> source(client, path, ext);.
  AudioSourceFTP(FTPClient<ClientType>& client, const char* path, const char* ext,
                 int files = 0) {
    p_client = &client;
    timeout_auto_next_value = 5000;
    if (path) p_path = path;
    setMaxFiles(files);
  }

  /// Resets the actual data
  bool begin()override {
    TRACED();
    idx = 0;
    files.clear();
    return addDirectory(p_path);
  }

  /// Resets the actual data
  void end() {
    idx = 0;
    files.clear();
  }

  /// Returns next audio stream
  Stream* nextStream(int offset) override {
    int tmp_idx = idx + offset;
    if (!isValidIdx(tmp_idx)) return nullptr;
    return selectStream(tmp_idx);
  };

  /// Returns previous audio stream
  Stream* previousStream(int offset) override { return nextStream(-offset); };

  /// Returns audio stream at the indicated index (the index is zero based, so
  /// the first value is 0!)
  Stream* selectStream(int index) override {
    if (!isValidIdx(index)) return nullptr;
    idx = index;
    if (file) {
       file.close();  // close the previous file
    }
    file = p_client->open(files[idx].name());
    return &file;
  }

  /// Retrieves all files and returns the actual index of the stream
  int index() override { return idx; }

  /// Returns the FTPFile for the indicated path
  Stream* selectStream(const char* path) override {
    TRACED();
    files.clear();
    idx = 0;
    addDirectory(path);
    return selectStream(0);
  }

  /// provides the actual stream (e.g. file) name or url
  const char* toStr() override {
    return file.name();
  }

  /// Defines the max number of files (if value is >0)
  void setMaxFiles(int maxCount) { max_files = maxCount; }

  /// Adds all the files of a directory
  bool addDirectory(const char* path) {
    TRACED();
    if (p_client == nullptr) return false;
    FTPFile dir = p_client->open(path);
    addFiles(dir, 1);
    return true;
  }

  /// Returns the number of available files
  size_t size() { return files.size(); }

 protected:
  std::vector<FTPFile> files;
  FTPClient<ClientType>* p_client = nullptr;
  FTPFile file;
  int idx = 0;
  size_t max_files = 0;
  const char* p_ext = nullptr;
  const char* p_path = "/";
  bool is_first = true;

  /// Adds all files recursively
  void addFiles(FTPFile& dir, int level) {
    if (!endsWith(dir.name(), p_ext)) {
      LOGI("adding file %s", dir.name());
      files.push_back(std::move(dir));
    } else {
      for (const auto& file : p_client->ls(dir.name())) {
        if (endsWith(file.name(), p_ext)) {
          LOGI("adding file %s", file.name());
          files.push_back(std::move(file));
        }
        if (max_files > 0 && files.size() >= max_files) {
          LOGI("max files reached: %d", max_files);
          return;  // stop if we reached the max number of files
        }
      }
    }
  }

  bool isValidIdx(int index) {
    if (index < 0) return false;
    if (index >= files.size()) {
      LOGE("index %d is out of range (size: %d)", index, files.size());
      return false;
    }
    return true;
  }

  bool endsWith(const char* file, const char* ext) {
    if (file == nullptr) return false;
    if (p_ext == nullptr) return true;
    return (StrView(file).endsWith(ext));
  }
};

}  // namespace audio_tools