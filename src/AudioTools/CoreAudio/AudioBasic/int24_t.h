#pragma once
#include "AudioToolsConfig.h"

// select int24 implementation
#include "AudioTools/CoreAudio/AudioBasic/Int24_3bytes_t.h"
#include "AudioTools/CoreAudio/AudioBasic/Int24_4bytes_t.h"

namespace audio_tools {
#ifdef USE_3BYTE_INT24
    using int24_t = int24_3bytes_t;
#else
    using int24_t = int24_4bytes_t;
#endif
}

