#pragma once

#include <optional>

#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioTools/CoreAudio/AudioPlayer.h"
#include "AudioTools/Disk/AudioSource.h"

#define KA_VERSION "Release: 2.4, Revision: R0"

namespace audio_tools {

/***
 * @brief KA-Radio Protocol: We can use the KA-Radio protocol to control the
 * audio player provided by the audiotools. Example: volume=50&play=128&infos
 * See https://github.com/karawin/Ka-Radio32/blob/master/Interface.md
 * @author Phil Schatzmann
 */

class KARadioProtocol {
 public:
  /// Default constructor
  KARadioProtocol(AudioPlayer& player) { setPlayer(player); }

  /// Empty constructor: call setPlayer to define the player
  KARadioProtocol() = default;

  /// Defines the player
  void setPlayer(AudioPlayer& player) {
    p_player = &player;
    volume = player.volume() * 254.0f;
  }

  /// processes the commands and returns the result output via the Print object
  bool processCommand(const char* input, Print& result) {
    if (p_player == nullptr) {
      LOGE("player not set");
      return false;
    }

    Str name;
    Str arg;
    StrView line = input;

    int start = line.indexOf('?');
    if (start < 0) start = 0;
    int endPos = line.length();
    bool rc = true;
    while (start >= 0 && start < endPos) {
      int eqPos = line.indexOf('=', start);
      int toPos = getEndPos(line, start + 1);
      if (eqPos >= 0) {
        name.substring(line, start + 1, eqPos);
        arg.substring(line, eqPos + 1, toPos);
      } else {
        name.substring(line, start + 1, toPos);
        arg = "";
      }
      LOGD("start=%d, eqPos=%d, toPos=%d", start, eqPos, toPos);
      // remove leading and trailing spaces
      name.trim();
      arg.trim();
      // execute the command
      rc = processCommand(name, arg, result);
      // calculate new positions
      start = toPos;
      toPos = getEndPos(line, start + 1);
    }
    return rc;
  }

  /// Processes a single command
  bool processCommand(Str& name, Str& arg, Print& result) {
    LOGI("command: %s (%s)", name.c_str(), arg.c_str());
    if (p_player == nullptr) {
      LOGE("No player set");
      return false;
    }
    if (name == "play") {
      if (!arg.isEmpty()) {
        int idx = arg.toInt();
        p_player->setIndex(idx);
      }
    } else if (name == "instant") {
      p_player->setPath(arg.c_str());
    } else if (name == "volume") {
      if (!arg.isEmpty()) {
        volume = arg.toInt();
        p_player->setVolume(static_cast<float>(volume) / 254.0f);
      }
    } else if (name == "volume+") {
      volume += 5;
      if (volume > 245) {
        volume = 254;
      }
      p_player->setVolume(static_cast<float>(volume) / 254.0f);
    } else if (name == "volume-") {
      volume -= 5;
      if (volume < 0) {
        volume = 0;
      }
      p_player->setVolume(static_cast<float>(volume) / 254.0f);
    } else if (name == "pause") {
      p_player->setActive(false);
    } else if (name == "resume") {
      p_player->setActive(true);
    } else if (name == "stop") {
      p_player->setActive(false);
    } else if (name == "start") {
      p_player->setActive(true);
    } else if (name == "next") {
      p_player->next();
    } else if (name == "prev") {
      p_player->previous();
    } else if (name == "version") {
      result.print("version: ");
      result.println(KA_VERSION);
    } else if (name == "mute") {
      if (!arg.isEmpty()) {
        p_player->setActive(!(arg.toInt() == 1));
      }
    } else if (name == "infos") {
      result.print("vol: ");
      result.println(volume);
      result.print("num: ");
      result.println(index());
      result.print("stn: ");  // station
      result.println(stationName());
      result.print("tit: ");  // title
      result.println(title());
      result.print("sts: ");  // status
      result.println(p_player->isActive());
    } else if (name == "list") {
      // arg: 0 to 254
      if (!arg.isEmpty()) {
        p_player->setIndex(arg.toInt());
      }
      result.println(stationName());
    } else {
      LOGE("Invalid command:", name.c_str());
      return false;
    }
    result.flush();
    return true;
  }

  /// Provides the actual index
  int index() { return p_player->audioSource().index(); }

  /// Provides the actual title
  const char* title() { return title_str.c_str(); }

 protected:
  AudioPlayer* p_player = nullptr;
  int volume = 0;
  Str title_str = "n/a";

  int getEndPos(StrView& line, int start) {
    int endPos = line.indexOf('&', start);
    if (endPos < 0) {
      endPos = line.length();
    }
    return endPos;
  }

  const char* stationName() {
    if (p_player != nullptr) {
      return p_player->audioSource().toStr();
    }
    return "";
  }
};
}  // namespace audio_tools