#pragma once
#include "AudioTools/CoreAudio/AudioPlayer.h"

namespace audio_tools {

/***
 * @brief  Abstract class for protocol to control the audio player.
 * @ingroup player
 * @author Phil Schatzmann
 */
class AudioPlayerProtocol {
 public:
  /// processes the commands and returns the result output via the Print
  /// object
  virtual bool processCommand(const char* input, Print& result) = 0;

  /// Proceess commands passed by Stream (e.g. Serial)
  virtual bool processCommand(Stream& input, Print& result) {
    char buffer[max_input_buffer_size];
    int len = readLine(input, buffer, max_input_buffer_size);
    if (len == 0) return false;
    return processCommand(buffer, result);
  }
  /// Defines the player
  virtual void setPlayer(AudioPlayer& player) { p_player = &player; }

  /// Defines the input buffer size used by the readLine function (default 256)
  void setMaxInputBufferSize(int size) { max_input_buffer_size = size; }

 protected:
  AudioPlayer* p_player = nullptr;
  int max_input_buffer_size = 256;

  /// Reads a line delimited by '\n' from the stream
  int readLine(Stream& in, char* str, int max) {
    int index = 0;
    if (in.available() > 0) {
      index = in.readBytesUntil('\n', str, max);
      str[index] = '\0';  // null termination character
    }
    return index;
  }
};

}  // namespace audio_tools