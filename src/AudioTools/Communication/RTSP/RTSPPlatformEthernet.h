#pragma once
#include "Ethernet.h"
#include "EthernetUdp.h"
#include "RTSPPlatform.h"

namespace audio_tools {

/**
 * @brief RTSP platform binding for Arduino Ethernet
 *
 * Convenience type alias that binds the generic `RTSPPlatform` to the
 * Arduino Ethernet networking stack (`EthernetServer`, `EthernetClient`,
 * `EthernetUDP`). Use this when running the RTSP server over Ethernet-
 * capable boards.
 *
 * @ingroup rtsp
 */
using RTSPPlatformEthernet = RTSPPlatform<EthernetServer, EthernetClient, EthernetUDP>;

}  // namespace audio_tools
