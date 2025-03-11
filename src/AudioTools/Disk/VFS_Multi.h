#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Disk/VFS.h"

namespace audio_tools {

/**
 * @brief Define multipe VFS with their mount point
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class VFS_Multi : public VFS {
 public:
  /// adds a vfs with the corresponding mount point
  void add(VFS& vfs, const char* mountPoint) {
    vfs.setMountPoint(mountPoint);
    vfs_vector.push_back(&vfs);
  }
  bool begin() override {
    bool result = true;
    for (int j = 0; j < vfs_vector.size(); j++) {
      result = result && vfs_vector[j]->begin();
    }
    return result;
  }
  void end() override {
    for (int j = 0; j < vfs_vector.size(); j++) {
      vfs_vector[j]->end();
    }
  }
  virtual void setMountPoint(const char* mp) { LOGE("not supported"); }

 protected:
  Vector<VFS*> vfs_vector;
};

}  // namespace audio_tools