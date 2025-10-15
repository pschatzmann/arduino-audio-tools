/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */
#include "Ethernet.h"
#include "EthernetUdp.h"
#include "RTSPClient.h"

namespace audio_tools {

/**
 * @brief Ethernet RTSP client alias using Arduino Ethernet networking.
 *
 * Convenience alias for `RTSPClient<EthernetClient, EthernetUDP>`, which uses
 * `EthernetClient` for RTSP TCP control and `EthernetUDP` for RTP.
 *
 * Example:
 * @code{.cpp}
 * I2SStream i2s;                 // your audio sink
 * RTSPClientEthernet client{i2s}; // decode to i2s
 * IPAddress cam(192,168,1,20);
 * client.begin(cam, 554, "stream1"); // optional path
 * while (true) {
 *     client.copy();              // push next RTP payload to decoder
 * }
 * @endcode
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
using RTSPClientEthernet = RTSPClient<EthernetClient, EthernetUDP>;

}  // namespace audio_tools