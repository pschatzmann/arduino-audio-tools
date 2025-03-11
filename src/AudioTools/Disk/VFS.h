#pragma once

namespace audio_tools {

/***
 * Base class which represents an ESP32 Virtual File System. After initializing
 * the VFS the regular c file operations are supported.
 */
class VFS {
 public:
  virtual bool begin() = 0;
  virtual void end() = 0;
  virtual void setMountPoint(const char* mp) = 0;
};
}  // namespace audio_tools
