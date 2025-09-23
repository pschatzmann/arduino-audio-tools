#pragma once
#include "RTSPClient.h"
#include "WiFi.h"

namespace audio_tools {

/**
 * @brief WiFi RTSP client alias using Arduino WiFi networking.
 *
 * Convenience alias for `RTSPClient<WiFiClient, WiFiUDP>`, which uses
 * `WiFiClient` for RTSP TCP control and `WiFiUDP` for RTP.
 *
 * Example:
 * @code{.cpp}
 * I2SStream i2s;                 // your audio sink
 * RTSPClientWiFi client{i2s};    // decode to i2s
 * IPAddress cam(192,168,1,20);
 * client.begin(cam, 554, "stream1"); // optional path
 * while (true) {
 *     client.copy();              // push next RTP payload to decoder
 * }
 * @endcode
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
using RTSPClientWiFi = RTSPClient<WiFiClient, WiFiUDP>;

}  // namespace audio_tools