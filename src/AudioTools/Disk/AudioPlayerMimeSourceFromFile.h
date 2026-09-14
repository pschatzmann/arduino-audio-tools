#pragma once

#include "AudioTools/CoreAudio/AudioMetaData/MimeResolver.h"
#include "AudioTools/CoreAudio/AudioPlayer.h"

namespace audio_tools {

/**
 * @brief MimeSource that determines the mime type from the file name
 * extension of the file that an AudioPlayer is currently playing.
 *
 * Intended to be used together with a MultiDecoder (via
 * MultiDecoder::setMimeSource()) whenever the AudioPlayer's AudioSource is
 * disk based (e.g. AudioSourceSDMMC, AudioSourceSD, AudioSourceSDFAT,
 * AudioSourceSPIFFS, AudioSourceLittleFS, AudioSourceVFS, ...). The
 * decoder is then selected from the file name extension instead of
 * relying on (potentially ambiguous or slower) content sniffing.
 *
 * @code
 * AudioSourceSDMMC source(...);
 * MultiDecoder multiDecoder;
 * AudioPlayer player(source, out, multiDecoder);
 * AudioPlayerMimeSourceFromFile<fs::File> mimeFromFile(player);
 * multiDecoder.setMimeSource(mimeFromFile);
 * @endcode
 *
 * @tparam FileT the File type returned by the AudioSource in use (e.g.
 * fs::File for AudioSourceSD/AudioSourceSDMMC). The type must provide a
 * name() method that returns the (base) file name.
 *
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename FileT>
class AudioPlayerMimeSourceFromFile : public MimeSource {
 public:
  AudioPlayerMimeSourceFromFile(AudioPlayer& player) { p_player = &player; }

  /// Determines the mime type from the extension of the currently playing
  /// file. Returns nullptr if no file is active or the extension is unknown.
  const char* mime() override {
    if (p_player == nullptr) return nullptr;
    Stream* p_stream = p_player->getStream();
    if (p_stream == nullptr) return nullptr;
    FileT* p_file = static_cast<FileT*>(p_stream);
    return resolver.fromFilename(p_file->name());
  }

  /// Provides access to the mime resolver, e.g. to register additional
  /// extension -> mime mappings via addMimeEntry().
  MimeResolver& mimeResolver() { return resolver; }

 protected:
  AudioPlayer* p_player = nullptr;
  MimeResolver resolver;
};

}  // namespace audio_tools
