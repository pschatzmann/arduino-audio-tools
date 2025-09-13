#pragma once
#include "WiFi.h"
#include "RTSPPlatform.h"

namespace audio_tools {

// Default platform specialization for Arduino WiFi
using RTSPPlatformWiFi = RTSPPlatform<WiFiClient, WiFiUDP>;

}  // namespace audio_tools