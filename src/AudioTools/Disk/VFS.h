#pragma once

namespace audio_tools {

/***
 * Base class which represents an ESP32 Virtual File System. After initializing
 * the VFS the regular c file operations are supported.
 */
class VFS {
 public:
  /// mount the file systems
  virtual bool begin() = 0;
  /// unmount the file system
  virtual void end() = 0;
  /// provide the mount point (root directory for the file system)
  virtual void setMountPoint(const char* mp) = 0;
};
}  // namespace audio_tools
