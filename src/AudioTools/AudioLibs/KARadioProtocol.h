#pragma once

#include <optional>

#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioTools/CoreAudio/AudioPlayer.h"
#include "AudioTools/Disk/AudioSource.h"
#include "AudioPlayerProtocol.h"

#define KA_VERSION "Release: 2.4, Revision: R0"

namespace audio_tools {

/**
 * @brief KA-Radio Protocol: We can use the KA-Radio protocol to control the
 * audio player provided by the audiotools.
 * Supported commands: play, instant, volume, volume+, volume-, pause,
 * resume, stop, start, next, prev, mute, infos, version.
 * Example: volume=50&play=128&infos
 * See https://github.com/karawin/Ka-Radio32/blob/master/Interface.md
 *
 * @ingroup player
 * @author Phil Schatzmann
 */

class KARadioProtocol : public AudioPlayerProtocol {
 public:
  /// Empty constructor: call setPlayer to define the player
  KARadioProtocol() {
    addCommand("play", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                          KARadioProtocol* self) {
      if (!par.isEmpty()) {
        int idx = par.toInt();
        player.setIndex(idx);
      }
      return true;
    });
    addCommand("instant", [](AudioPlayer& player, Str& cmd, Str& par,
                             Print& out, KARadioProtocol* self) {
      player.setPath(par.c_str());
      return true;
    });
    addCommand("volume", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                            KARadioProtocol* self) {
      if (!par.isEmpty()) {
        int volume = par.toInt();
        player.setVolume(static_cast<float>(volume) / 254.0f);
      }
      return true;
    });
    addCommand("volume+", [](AudioPlayer& player, Str& cmd, Str& par,
                             Print& out, KARadioProtocol* self) {
      int volume = player.volume() * 254.0f;
      volume += 5;
      if (volume > 245) {
        volume = 254;
      }
      player.setVolume(static_cast<float>(volume) / 254.0f);
      return true;
    });
    addCommand("volume-", [](AudioPlayer& player, Str& cmd, Str& par,
                             Print& out, KARadioProtocol* self) {
      int volume = player.volume() * 254.0f;
      volume -= 5;
      if (volume < 0) {
        volume = 0;
      }
      player.setVolume(static_cast<float>(volume) / 254.0f);
      return true;
    });
    addCommand("pause", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                           KARadioProtocol* self) {
      player.setActive(false);
      return true;
    });
    addCommand("resume", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                            KARadioProtocol* self) {
      player.setActive(true);
      return true;
    });
    addCommand("stop", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                          KARadioProtocol* self) {
      player.setActive(false);
      return true;
    });
    addCommand("start", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                           KARadioProtocol* self) {
      player.setActive(true);
      return true;
    });
    addCommand("next", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                          KARadioProtocol* self) {
      player.next();
      return true;
    });
    addCommand("prev", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                          KARadioProtocol* self) {
      player.previous();
      return true;
    });
    addCommand("mute", [](AudioPlayer& player, Str& cmd, Str& par, Print& out,
                          KARadioProtocol* self) {
      if (!par.isEmpty()) {
        player.setActive(!(par.toInt() == 1));
      }
      return true;
    });
    addCommand("infos", [](AudioPlayer& player, Str& cmd, Str& par,
                           Print& result, KARadioProtocol* self) {
      result.print("vol: ");
      result.println(self->volume);
      result.print("num: ");
      result.println(self->index());
      result.print("stn: ");  // station
      result.println(self->stationName());
      result.print("tit: ");  // title
      result.println(self->title());
      result.print("sts: ");  // status
      result.println(player.isActive());
      return true;
    });
    addCommand("version", [](AudioPlayer& player, Str& cmd, Str& par,
                             Print& result, KARadioProtocol* self) {
      result.print("version: ");
      result.println(KA_VERSION);
      return true;
    });
    addCommand("list",
               [](AudioPlayer& player, Str& cmd, Str& par, Print& result,
                  KARadioProtocol* self) {  // arg: 0 to 254
                 if (!par.isEmpty()) {
                   player.setIndex(par.toInt());
                 }
                 result.println(self->stationName());
                 return true;
               });
  }

  /// Default constructor
  KARadioProtocol(AudioPlayer& player) : KARadioProtocol() {
    setPlayer(player);
  }

  /// Defines the player
  void setPlayer(AudioPlayer& player) override {
    AudioPlayerProtocol::setPlayer(player);
    volume = player.volume() * 254.0f;
  }

  bool processCommand(Stream& input, Print& result) override {
    return AudioPlayerProtocol::processCommand(input, result);
  }

  /// processes the commands and returns the result output via the Print
  /// object
  bool processCommand(const char* input, Print& result) override {
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
    assert(p_player != nullptr);

    // ignore empty commands
    if (name.isEmpty()) return false;

    for (Action& act : actions) {
      if (name.equals(act.cmd)) {
        return act.callback(*p_player, name, arg, result, this);
      } else {
        LOGD("-> %s vs %s: failed", name.c_str(), act.cmd);
      }
    }

    LOGE("Invalid command:", name.c_str());
    return false;
  }

  /// Provides the actual index
  int index() { return p_player->audioSource().index(); }

  /// Provides the actual title
  const char* title() { return title_str.c_str(); }

  /// Add a new command
  void addCommand(const char* cmd,
                  bool (*cb)(AudioPlayer& player, Str& cmd, Str& par,
                             Print& out, KARadioProtocol* self)) {
    Action act;
    act.cmd = cmd;
    act.callback = cb;
    actions.push_back(act);
  }

 protected:
  int volume = 0;
  Str title_str = "n/a";
  struct Action {
    const char* cmd;
    bool (*callback)(AudioPlayer& player, Str& cmd, Str& par, Print& out,
                     KARadioProtocol* self) = nullptr;
  };
  Vector<Action> actions;

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