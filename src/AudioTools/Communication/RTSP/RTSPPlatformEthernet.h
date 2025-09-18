#pragma once
#include "Ethernet.h"
#include "EthernetUdp.h"
#include "RTSPPlatform.h"

namespace audio_tools {

// Default platform specialization for Arduino WiFi
using RTSPPlatformEthernet = RTSPPlatform<EthernetServer, EthernetClient, EthernetUDP>;

}  // namespace audio_tools
