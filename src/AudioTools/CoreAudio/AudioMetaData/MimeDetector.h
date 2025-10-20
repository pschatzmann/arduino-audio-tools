#pragma once

#include "AudioTools/AudioCodecs/HeaderParserAAC.h"
#include "AudioTools/AudioCodecs/HeaderParserMP3.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

namespace audio_tools {

/**
 * @brief Abstract interface for classes that can provide MIME type information.
 *
 * This class defines a simple interface for objects that can determine and
 * provide MIME type strings. It serves as a base class for various MIME
 * detection and source identification implementations within the audio tools
 * framework.
 *
 * Classes implementing this interface should provide logic to determine the
 * appropriate MIME type based on their specific context (e.g., file content,
 * stream headers, file extensions, etc.).
 *
 * @note This is a pure virtual interface class and cannot be instantiated
 * directly.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MimeSource {
 public:
  /**
   * @brief Get the MIME type string.
   *
   * Pure virtual method that must be implemented by derived classes to return
   * the appropriate MIME type string for the current context.
   *
   * @return const char* Pointer to a null-terminated string containing the MIME
   * type. The string should follow standard MIME type format (e.g.,
   * "audio/mpeg"). Returns nullptr if MIME type cannot be determined.
   *
   * @note The returned pointer should remain valid for the lifetime of the
   * object or until the next call to this method.
   */
  virtual const char* mime() = 0;
};

/**
 * @brief  Logic to detemine the mime type from the content.
 * By default the following mime types are supported (audio/aac, audio/mpeg,
 * audio/vnd.wave, audio/ogg, audio/flac). You can register your own custom
 * detection logic to cover additional file types.
 *
 * Please note that the distinction between mp3 and aac is difficult and might
 * fail in some cases. FLAC detection supports both native FLAC and OGG FLAC
 * formats.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MimeDetector : public MimeSource {
 public:
  MimeDetector(bool setupDefault = true) {
    if (setupDefault) {
      setCheck("audio/vnd.wave; codecs=ms-adpcm", checkWAV_ADPCM);
      setCheck("audio/vnd.wave", checkWAV);
      setCheck("audio/flac", checkFLAC);
      setCheck("audio/ogg; codecs=flac", checkOggFLAC);
      setCheck("audio/ogg; codecs=opus", checkOggOpus);
      setCheck("audio/ogg; codec=vorbis", checkOggVorbis);
      setCheck("audio/ogg", checkOGG);
      setCheck("video/MP2T", checkMP2T);
      setCheck("audio/prs.sid", checkSID);
      setCheck("audio/m4a", checkM4A, false);
      setCheck("audio/mpeg", checkMP3Ext);
      setCheck("audio/aac", checkAACExt);
    }
  }

  /// Sets is_first to true
  bool begin() {
    is_first = true;
    return true;
  }

  /// Clears the actual mime and resets the state
  void end() {
    actual_mime = nullptr;
    is_first = true;
  }

  /// write the header to determine the mime
  size_t write(uint8_t* data, size_t len) {
    actual_mime = default_mime;
    determineMime(data, len);
    return len;
  }

  /// adds/updates the checking logic for the indicated mime
  void setCheck(const char* mime, bool (*check)(uint8_t* start, size_t len),
                bool isActvie = true) {
    StrView mime_str{mime};
    for (int j = 0; j < checks.size(); j++) {
      Check l_check = checks[j];
      if (mime_str.equals(l_check.mime)) {
        l_check.check = check;
        l_check.is_active = isActvie;
        return;
      }
    }
    Check check_to_add{mime, check};
    check_to_add.is_active = isActvie;
    checks.push_back(check_to_add);
    LOGI("MimeDetector for %s: %s", mime, isActvie ? "active" : "inactive");
  }

  // /// Define the callback that will notify about mime changes
  void setMimeCallback(void (*callback)(const char*)) {
    TRACED();
    this->notifyMimeCallback = callback;
  }

  /// Provides the actual mime type, that was determined from the first
  /// available data
  const char* mime() { return actual_mime; }

  static bool checkAAC(uint8_t* start, size_t len) {
    return start[0] == 0xFF &&
           (start[1] == 0xF0 || start[1] == 0xF1 || start[1] == 0xF9);
  }

  static bool checkAACExt(uint8_t* start, size_t len) {
    // checking logic for files
    if (memcmp(start + 4, "ftypM4A", 7) == 0) {
      return true;
    }
    // check for streaming
    HeaderParserAAC aac;
    // it should start with a synch word
    int pos = aac.findSyncWord((const uint8_t*)start, len);
    if (pos == -1) {
      return false;
    }
    // make sure that it is not an mp3
    if (aac.isValid(start + pos, len - pos)) {
      return false;
    }
    return true;
  }

  static bool checkMP3(uint8_t* start, size_t len) {
    return memcmp(start, "ID3", 3) == 0 ||
           (start[0] == 0xFF && ((start[1] & 0xE0) == 0xE0));
  }

  static bool checkMP3Ext(uint8_t* start, size_t len) {
    HeaderParserMP3 mp3;
    return mp3.isValid(start, len);
  }

  static bool checkWAV_ADPCM(uint8_t* start, size_t len) {
    if (memcmp(start, "RIFF", 4) != 0) return false;
    WAVHeader header;
    header.write(start, len);
    if (!header.parse()) return false;
    if (header.audioInfo().format == AudioFormat::ADPCM) {
      return true;
    }
    return false;
  }

  static bool checkWAV(uint8_t* start, size_t len) {
    return memcmp(start, "RIFF", 4) == 0;
  }

  static bool checkOGG(uint8_t* start, size_t len) {
    return memcmp(start, "OggS", 4) == 0;
  }

  static bool checkFLAC(uint8_t* start, size_t len) {
    if (len < 4) return false;

    // Native FLAC streams start with "fLaC" (0x664C6143)
    if (memcmp(start, "fLaC", 4) == 0) {
      return true;
    }
    return false;
  }

  static bool checkOggFLAC(uint8_t* start, size_t len) {
    // Check for OGG FLAC - OGG container with FLAC content
    // OGG starts with "OggS" and may contain FLAC codec
    if (len >= 32 && memcmp(start, "OggS", 4) == 0) {
      // Look for FLAC signature within the first OGG page
      // FLAC in OGG typically has "\x7FFLAC" or "FLAC" as codec identifier
      for (size_t i = 4; i < len - 4 && i < 64; i++) {
        if (memcmp(start + i, "FLAC", 4) == 0) {
          return true;
        }
        // Also check for the more specific OGG FLAC header
        if (i < len - 5 && start[i] == 0x7F &&
            memcmp(start + i + 1, "FLAC", 4) == 0) {
          return true;
        }
      }
    }

    return false;
  }

  /**
   * @brief Checks for OGG Opus format
   *
   * Detects OGG containers that contain Opus-encoded audio data.
   * Opus in OGG streams typically have "OpusHead" as the codec identifier
   * in the first page of the stream.
   *
   * @param start Pointer to the data buffer
   * @param len Length of the data buffer
   * @return true if OGG Opus format is detected, false otherwise
   */
  static bool checkOggOpus(uint8_t* start, size_t len) {
    // Check for OGG Opus - OGG container with Opus content
    // OGG starts with "OggS" and contains Opus codec identifier
    if (len >= 32 && memcmp(start, "OggS", 4) == 0) {
      // Look for Opus signature within the first OGG page
      // Opus in OGG typically has "OpusHead" as the codec identifier
      for (size_t i = 4; i < len - 8 && i < 80; i++) {
        if (memcmp(start + i, "OpusHead", 8) == 0) {
          return true;
        }
      }
    }

    return false;
  }

  /**
   * @brief Checks for OGG Vorbis format
   *
   * Detects OGG containers that contain Vorbis-encoded audio data.
   * Vorbis in OGG streams have "\x01vorbis" as the codec identifier
   * in the first page of the stream.
   *
   * @param start Pointer to the data buffer
   * @param len Length of the data buffer
   * @return true if OGG Vorbis format is detected, false otherwise
   */
  static bool checkOggVorbis(uint8_t* start, size_t len) {
    // Check for OGG Vorbis - OGG container with Vorbis content
    // OGG starts with "OggS" and contains Vorbis codec identifier
    if (len >= 32 && memcmp(start, "OggS", 4) == 0) {
      // Look for Vorbis signature within the first OGG page
      // Vorbis in OGG has "\x01vorbis" as the codec identifier
      for (size_t i = 4; i < len - 7 && i < 80; i++) {
        if (start[i] == 0x01 && memcmp(start + i + 1, "vorbis", 6) == 0) {
          return true;
        }
      }
    }

    return false;
  }

  /// MPEG-2 TS Byte Stream Format
  static bool checkMP2T(uint8_t* start, size_t len) {
    if (len < 189) return start[0] == 0x47;

    return start[0] == 0x47 && start[188] == 0x47;
  }

  /// Commodore 64 SID File
  static bool checkSID(uint8_t* start, size_t len) {
    return memcmp(start, "PSID", 4) == 0 || memcmp(start, "RSID", 4) == 0;
  }

  static bool checkM4A(uint8_t* header, size_t len) {
    if (len < 12) return false;

    // prevent false detecton by mp3 files
    if (memcmp(header, "ID3", 3) == 0) return false;

    // Special hack when we position to start of mdat box
    if (memcmp(header + 4, "mdat", 4) != 0) return true;

    // Check for "ftyp" at offset 4
    if (memcmp(header + 4, "ftyp", 4) != 0) return false;

    // Check for "M4A " or similar major brand
    if (memcmp(header + 8, "M4A ", 4) == 0 ||
        memcmp(header + 8, "mp42", 4) == 0 ||
        memcmp(header + 8, "isom", 4) == 0)
      return true;

    return false;
  }

  /// Provides the default mime type if no mime could be determined
  void setDefaultMime(const char* mime) { default_mime = mime; }

  /// sets the mime rules active or inactive
  int setMimeActive(const char* mimePrefix, bool active) {
    int result = 0;
    for (auto& check : checks) {
      if (StrView(check.mime).startsWith(mimePrefix)) {
        check.is_active = active;
        LOGI("MimeDetector for %s: %s", check.mime,
             check.is_active ? "active" : "inactive");
        result++;
      }
    }
    return result;
  }

  /// Clears all mime rules and resets the actual selection
  void clear() {
    checks.clear();
    actual_mime = nullptr;
    is_first = true;
  }

 protected:
  struct Check {
    const char* mime = nullptr;
    bool (*check)(uint8_t* data, size_t len) = nullptr;
    bool is_active = true;
    Check(const char* mime, bool (*check)(uint8_t* data, size_t len)) {
      this->mime = mime;
      this->check = check;
    }
    Check() = default;
  };
  Vector<Check> checks{0};
  bool is_first = false;
  const char* actual_mime = nullptr;
  const char* default_mime = nullptr;
  void (*notifyMimeCallback)(const char* mime) = nullptr;

  /// Update the mime type
  void determineMime(void* data, size_t len) {
    if (is_first) {
      actual_mime = lookupMime((uint8_t*)data, len);
      if (notifyMimeCallback != nullptr && actual_mime != nullptr) {
        notifyMimeCallback(actual_mime);
      }
      is_first = false;
    }
  }

  /// Default logic which supports aac, mp3, wav and ogg
  const char* lookupMime(uint8_t* data, size_t len) {
    for (int j = 0; j < checks.size(); j++) {
      Check l_check = checks[j];
      if (l_check.is_active && l_check.check(data, len)) {
        return l_check.mime;
      }
    }
    return default_mime;
  }
};

}  // namespace audio_tools
