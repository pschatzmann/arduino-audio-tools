#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "VFSFile.h"

namespace audio_tools {

/***
 * @brief Abstract Base class which represents an ESP32 Virtual File System.
 * After initializing the VFS the regular c file operations are supported.
 * @brief Abstract Base class which represents an ESP32 Virtual File System.
 * After initializing the VFS the regular c file operations are supported.
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VFS {
 public:
  /// mount the file systems
  virtual bool begin() {return true;}
  /// unmount the file system
  virtual void end() {} 
  /// provide the mount point (root directory for the file system)
  virtual void setMountPoint(const char* mp) = 0;

  VFSFile open(const char* file, FileMode mode = VFS_FILE_READ) {
    VFSFile vfs_file;
    vfs_file.open(expand(file), mode);
    return vfs_file;
  }
  VFSFile open(const std::string& path, FileMode mode = VFS_FILE_READ) {
    const char* path_str = path.c_str();
    const char* path_str_exanded = expand(path_str);
    LOGI("open: %s", path_str_exanded);
    return this->open(path_str_exanded, mode);
  }
  bool exists(const char* path) {
    struct stat buffer;
    return (stat(expand(path), &buffer) == 0);
  }
  bool exists(const std::string& path) { return exists(path.c_str()); }
  bool remove(const char* path) { return ::remove(expand(path)) == 0; }
  bool remove(const std::string& path) { return remove(path.c_str()); }
  bool rename(const char* pathFrom, const char* pathTo) {
    return ::rename(expand(pathFrom), expand(pathTo)) == 0;
  }
  bool rename(const std::string& pathFrom, const std::string& pathTo) {
    return rename(pathFrom.c_str(), pathTo.c_str());
  }
  bool mkdir(const char* path) { return ::mkdir(expand(path), 0777) == 0; }
  bool mkdir(const std::string& path) { return mkdir(path.c_str()); }
  bool rmdir(const char* path) { return ::rmdir(expand(path)) == 0; }
  bool rmdir(const std::string& path) { return rmdir(path.c_str()); }

  /// provides the actual mount point
  const char* mountPoint() { return mount_point; }

 protected:
  const char* mount_point = "/";

  Str tmp;

  /// expands the file name with the mount point
  const char* expand(const char* file) {
    assert(mount_point != nullptr);
    assert(file != nullptr);
    tmp = mount_point;
    if (!StrView(file).startsWith("/") && !StrView(mount_point).endsWith("/")) {
      tmp.add("/");
    }
    tmp.add(file);
    return tmp.c_str();
  }
};

}  // namespace audio_tools
