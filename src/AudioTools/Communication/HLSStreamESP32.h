#pragma once
#include "AudioTools/Communication/HTTP/URLStreamESP32.h"
#include "AudioTools/CoreAudio/AudioHttp/URLStreamESP32.h"
#include "HLSStream.h"

namespace audio_tools {

/// @brief HLS Stream implementation using URLStreamESP32 for ESP32-specific HTTP requests
/// @note Supported only on ESP32 platforms with WiFi support!
/// @ingroup hls
using HLSStreamESP32 = HLSStreamT<URLStreamESP32>;

}  // namespace audio_tools