#pragma once
#include "AudioPlayerProtocolServer.h"
#include "KARadioProtocol.h"

namespace audio_tools {

/**
 * @brief KA-Radio Protocol Server which provides the KARadioProtocol over
 * http to control the audio player provided by the audiotools.
 * @ingroup player
 * @author Phil Schatzmann
 */

class KARadioProtocolServer : public AudioPlayerProtocolServer {
 public:
  /// Default constructor
  KARadioProtocolServer(AudioPlayer& player, int port = 80,
                        const char* ssid = nullptr, const char* pwd = nullptr) {
    setProtocol(protocol);
    setPlayer(player);
    setPort(port);
    setSSID(ssid);
    setPassword(pwd);
  }

  /// Empty constructor: call setPlayer to define the player
  KARadioProtocolServer() = default;

 protected:
  KARadioProtocol protocol;
};
}  // namespace audio_tools