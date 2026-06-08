#pragma once

/**
 * This file is setting up the SD object and the File class outside of Arduino
 */

#if defined(IS_ZEPHYR)

#include "ZephyrSD.h"
#include "ZephyrFile.h"

#else

#define VFS_SD SD
#include "AudioTools/Disk/VFSFile.h"
#include "AudioTools/Disk/VFS.h"

// We allow the access to the files via the global SD object

namespace audio_tools {

/// @brief Desktop file system compatibility alias
/// @ingroup io
using File = VFSFile;

/// @brief Desktop file system compatibility alias
/// @ingroup io
using FS = VFS;

static FS SD; // global object for compatibility with Arduino code

}

#endif
