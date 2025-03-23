#pragma once
#define VFS_SD SD
#include "AudioTools/Disk/VFSFile.h"
#include "AudioTools/Disk/VFS.h"

// We allow the access to the files via the global SD object

namespace audio_tools {

using File = VFSFile;
using FS = VFS;
    
}
