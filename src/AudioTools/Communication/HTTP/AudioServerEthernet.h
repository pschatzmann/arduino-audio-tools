#pragma once

#include "AudioToolsConfig.h"

#ifdef USE_ETHERNET
#  include <Ethernet.h>
#endif

#include "AudioServerT.h"
#include "AudioEncodedServerT.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

namespace audio_tools {

#ifdef USE_ETHERNET
/// @brief Ethernet audio server for streaming audio content over Ethernet
/// @ingroup http
using AudioServerEthernet = AudioServerT<EthernetClient, EthernetServer>;

/// @brief Ethernet audio server with encoder support for streaming encoded audio
/// @ingroup http
using AudioEncoderServerEthernet = AudioEncoderServerT<EthernetClient, EthernetServer>;

/// @brief Ethernet audio server specifically for streaming WAV audio
/// @ingroup http
using AudioWAVServerEthernet  = AudioWAVServerT<EthernetClient, EthernetServer>;

#ifndef USE_WIFI
/// @brief Basic audio server (defaults to Ethernet when USE_WIFI is not defined)
/// @ingroup http
using AudioServer = AudioServerT<EthernetClient, EthernetServer>;

/// @brief Basic audio server with encoder support (defaults to Ethernet when USE_WIFI is not defined)
/// @ingroup http
using AudioEncoderServer = AudioEncoderServerEthernet;

/// @brief Basic WAV audio server (defaults to Ethernet when USE_WIFI is not defined)
/// @ingroup http
using AudioWAVServer = AudioWAVServerEthernet;
#endif
#endif


}  // namespace audio_tools
