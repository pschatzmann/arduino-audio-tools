
#include "AudioTools/CoreAudio/AudioPlayer.h"
#include "OSCData.h"

namespace audio_tools {

/**
 * @brief Audio Player OSC Sender: Remote control for AudioPlayer class. Sends
 * OSC messages to the RCAudioPlayerOSCReceiver.
 * @ingroup player
 * @ingroup communication
 * @author Phil Schatzmann
 */
class RCAudioPlayerOSCSender {
  RCAudioPlayerOSCSender() = default;
  RCAudioPlayerOSCSender(Print &out) { setOutput(out); }

  void setOutput(Print &out) { p_out = &out; }

  bool setActive(bool active) { return active ? play() : stop(); }

  bool play() {
    if (p_out == nullptr) return false;

    uint8_t data[20];
    OSCData msg{data, sizeof(data)};

    msg.setAddress("/play");
    msg.setFormat("");
    p_out->write(msg.data(), msg.size());
    return true;
  }

  /// halts the playing: same as setActive(false)
  bool stop() {
    if (p_out == nullptr) return false;
    uint8_t data[20];
    OSCData msg{data, sizeof(data)};

    msg.setAddress("/stop");
    msg.setFormat("");
    p_out->write(msg.data(), msg.size());
    return true;
  }

  /// moves to next file or nth next file when indicating an offset. Negative
  /// values are supported to move back.
  bool next(int offset = 1) {
    if (p_out == nullptr) return false;
    uint8_t data[80];
    OSCData msg{data, sizeof(data)};

    msg.setAddress("/next");
    msg.setFormat("i");
    msg.write((int32_t)offset);
    p_out->write(msg.data(), msg.size());
    return true;
  }

  bool previous(int offset = 1) {
    if (p_out == nullptr) return false;
    uint8_t data[80];
    OSCData msg{data, sizeof(data)};

    msg.setAddress("/previous");
    msg.setFormat("i");
    msg.write((int32_t)offset);
    p_out->write(msg.data(), msg.size());
    return true;
  }

  /// moves to the selected file position
  bool setIndex(int idx) {
    if (p_out == nullptr) return false;
    uint8_t data[80];
    OSCData msg{data, sizeof(data)};

    msg.setAddress("/index");
    msg.setFormat("i");
    msg.write((int32_t)idx);
    p_out->write(msg.data(), msg.size());
    return true;
  }

  /// Moves to the selected file w/o updating the actual file position
  bool setPath(const char *path) {
    if (p_out == nullptr) return false;
    uint8_t data[strlen(path) + 20];
    OSCData msg{data, sizeof(data)};

    msg.setAddress("/path");
    msg.setFormat("s");
    msg.write(path);
    p_out->write(msg.data(), msg.size());
    return true;
  }

  bool setVolume(float volume) {
    if (p_out == nullptr) return false;
    uint8_t data[80];
    OSCData msg{data, sizeof(data)};

    msg.setAddress("/volume");
    msg.setFormat("f");
    msg.write(volume);
    p_out->write(msg.data(), msg.size());
    return true;
  }

 protected:
  Print *p_out = nullptr;
};

/**
 * @brief Audio Player OSC Receiver: Receives OSC messages from the
 * AudioPlayerOSCSender and applies it to the AudioPlayer.
 * @ingroup player
 * @ingroup communication
 * @author Phil Schatzmann
 */
class RCAudioPlayerOSCReceiver {
  RCAudioPlayerOSCReceiver() { registerCallbacks(); }

  RCAudioPlayerOSCReceiver(AudioPlayer &player) : RCAudioPlayerOSCReceiver() {
    setAudioPlayer(player);
  }

  void setAudioPlayer(AudioPlayer &player) { p_player = &player; }

  /// Process the incoming OSC messages
  bool processInputMessage(Stream &in) {
    if (!is_active) return false;

    uint8_t data[80];
    size_t len = in.readBytes(data, sizeof(data));
    if (len > 0) {
      if (osc.parse(data, len)) {
        return true;
      }
    }
    return false;
  }

  /// Starts the processing of the incoming OSC messages
  bool begin() {
    if (p_player == nullptr) {
      LOGE("RCAudioPlayerOSCReceiver: player is null");
      return false;
    }
    osc.setReference(p_player);
    is_active = true;
    return true;
  }

  /// stops the processing of the incoming OSC messages
  void end() {
    is_active = false;
    osc.clear();
  }

 protected:
  AudioPlayer *p_player = nullptr;
  bool is_active = false;
  OSCData osc;

  static bool play(OSCData &data, void *ref) {
    AudioPlayer *p_player = (AudioPlayer *)ref;
    p_player->play();
    return true;
  }

  static bool stop(OSCData &data, void *ref) {
    AudioPlayer *p_player = (AudioPlayer *)ref;
    p_player->stop();
    return true;
  }

  static bool next(OSCData &data, void *ref) {
    AudioPlayer *p_player = (AudioPlayer *)ref;
    int offset = data.readInt32();
    return p_player->next(offset);
  }

  static bool previous(OSCData &data, void *ref) {
    AudioPlayer *p_player = (AudioPlayer *)ref;
    int offset = data.readInt32();
    return p_player->previous(offset);
  }
  static bool setIndex(OSCData &data, void *ref) {
    AudioPlayer *p_player = (AudioPlayer *)ref;
    int idx = data.readInt32();
    return p_player->setIndex(idx);
  }
  static bool setPath(OSCData &data, void *ref) {
    AudioPlayer *p_player = (AudioPlayer *)ref;
    const char *path = data.readString();
    return p_player->setPath(path);
  }
  static bool setVolume(OSCData &data, void *ref) {
    AudioPlayer *p_player = (AudioPlayer *)ref;
    float volume = data.readFloat();
    return p_player->setVolume(volume);
  }

  void registerCallbacks() {
    osc.addCallback("/play", play, OSCCompare::StartsWith);
    osc.addCallback("/stop", stop, OSCCompare::StartsWith);
    osc.addCallback("/next", next, OSCCompare::StartsWith);
    osc.addCallback("/previous", previous, OSCCompare::StartsWith);
    osc.addCallback("/index", setIndex, OSCCompare::StartsWith);
    osc.addCallback("/path", setPath, OSCCompare::StartsWith);
    osc.addCallback("/volume", setVolume, OSCCompare::StartsWith);
  }
};

}  // namespace audio_tools
