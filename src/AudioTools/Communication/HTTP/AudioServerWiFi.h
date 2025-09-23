#pragma once

#include "AudioToolsConfig.h"

#ifdef USE_WIFI
#include "WiFiInclude.h"
#endif

#include "AudioEncodedServerT.h"
#include "AudioServerT.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

namespace audio_tools {

#ifdef USE_WIFI
/// @brief Basic WiFi audio server for streaming audio content
/// @ingroup http
using AudioServer = AudioServerT<WiFiClient, WiFiServer>;

/// @brief WiFi audio server for streaming audio content (explicit WiFi naming)
/// @ingroup http
using AudioServerWiFi = AudioServerT<WiFiClient, WiFiServer>;

/// @brief WiFi audio server with encoder support for streaming encoded audio
/// @ingroup http
using AudioEncoderServerWiFi = AudioEncoderServerT<WiFiClient, WiFiServer>;

/// @brief Basic audio server with encoder support (defaults to WiFi when
/// USE_WIFI is defined)
/// @ingroup http
using AudioEncoderServer = AudioEncoderServerT<WiFiClient, WiFiServer>;

/// @brief WiFi audio server specifically for streaming WAV audio
/// @ingroup http
using AudioWAVServerWiFi = AudioWAVServerT<WiFiClient, WiFiServer>;

/// @brief Basic WAV audio server (defaults to WiFi when USE_WIFI is defined)
/// @ingroup http
using AudioWAVServer = AudioWAVServerT<WiFiClient, WiFiServer>;
#endif

}  // namespace audio_tools
