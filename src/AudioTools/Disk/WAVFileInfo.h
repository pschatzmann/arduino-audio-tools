#pragma once

#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "FS.h"

namespace audio_tools {

/**
 * @brief Utility for reading and rewriting WAV file metadata in place.
 *
 * The class parses the existing WAV header, extracts the current audio
 * information, and can rewrite the header with an updated file size so the
 * RIFF and data chunk lengths stay consistent.
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <typename FileT = File>
class WAVFileInfo {
 public:
  WAVFileInfo() = default;

  /**
   * @brief Reads and parses the WAV header from a file.
   *
   * The method reads up to `MAX_WAV_HEADER_LEN` bytes from the beginning of
   * the provided file, parses the WAV header, and copies the extracted audio
   * metadata into `info`.
   *
   * @param file File-like object that provides `read()`.
   * @param info Target structure that receives the parsed WAV metadata.
   * @return `true` if a complete and valid WAV header was parsed successfully;
   * otherwise `false`.
   */
  bool getInfo(FileT file, WAVAudioInfo& info) {
    WAVHeader header;
    uint8_t buffer[MAX_WAV_HEADER_LEN];
    size_t bytesRead = file.read(buffer, MAX_WAV_HEADER_LEN);
    if (bytesRead > 0) {
      header.write(buffer, bytesRead);
      if (header.isDataComplete() && header.parse()) {
        info = header.audioInfo();
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Rewrites the WAV header with the current file size information.
   *
   * This updates `info.file_size` from the actual file size and recalculates
   * the payload size before writing a refreshed WAV header back to the file.
   *
   * @param file File-like object that provides `size()` and `write()`.
   * @param info WAV metadata to update and write back into the file header.
   * @return `true` if the header was written successfully; otherwise `false`.
   */
  bool updateSize(FileT file, WAVAudioInfo& info) {
    size_t fileSize = file.size();
    info.file_size = fileSize;
    info.data_length = fileSize - 36;  // Assuming standard 44-byte header
    WAVHeader header;
    if (!header.writeHeader(&file, info)) {
      LOGE("Failed to write updated header with new file size");
      return false;  // write failed
    }
    return true;
  }

  /**
   * @brief Reads the current WAV header and rewrites it with the actual file
   * size.
   *
   * This overload parses the existing header metadata from the file, seeks
   * back to the beginning of the stream, and then updates the header fields
   * using the current file size.
   *
   * @param file File-like object that provides `read()`, `seek()`, `size()`,
   * and `write()`. The file must be open in a mode that allows both reading
   * and writing.
   * @return `true` if the header was read, the file was repositioned, and the
   * updated header was written successfully; otherwise `false`.
   */
  bool updateSize(FileT file) {
    WAVAudioInfo info;
    if (!getInfo(file, info)) {
      LOGE("Failed to read existing header for size update");
      return false;  // read failed
    }
    if (!file.seek(0)) {
      LOGE("Failed to seek to beginning of file for header rewrite");
      return false;  // seek failed
    }
    return updateSize(file, info);
  }
};

}  // namespace audio_tools