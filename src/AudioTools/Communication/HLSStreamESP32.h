#pragma once
#include "AudioTools/CoreAudio/AudioHttp/URLStreamESP32.h"
#include "HLSStream.h"

namespace audio_tools {

using HLSStreamESP32 = HLSStreamT<URLStreamESP32>;

}  // namespace audio_tools