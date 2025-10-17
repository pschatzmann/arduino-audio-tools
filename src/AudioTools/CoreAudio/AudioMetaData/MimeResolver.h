#pragma once

#include <ctype.h>
#include <stddef.h>

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {

/// extension to mime table (sorted alphabetically)
struct MimeEntry {
  const char* ext;
  const char* mime;
};
/// Default mime table (sorted alphabetically) stored in program memory
static const MimeEntry mime_table[] = {
    {"aac", "audio/aac"},
    {"ac3", "audio/ac3"},
    {"aiff", "audio/aiff"},
    {"aif", "audio/aiff"},
    {"aifc", "audio/aiff"},
    {"alac", "audio/alac"},
    {"amr", "audio/amr"},
    {"au", "audio/basic"},
    {"caf", "audio/x-caf"},
    {"dts", "audio/vnd.dts"},
    {"flac", "audio/flac"},
    {"m3u", "audio/x-mpegurl"},
    {"m3u8", "application/vnd.apple.mpegurl"},
    {"m4a", "audio/m4a"},
    {"mid", "audio/midi"},
    {"midi", "audio/midi"},
    {"mka", "audio/x-matroska"},
    {"mkv", "video/x-matroska"},
    {"mp2", "audio/mpeg"},
    {"mp2t", "video/MP2T"},
    {"mp3", "audio/mpeg"},
    {"mp4", "video/mp4"},
    {"mpeg", "audio/mpeg"},
    {"oga", "audio/ogg"},
    {"ogg", "audio/ogg"},
    {"ogv", "video/ogg"},
    {"opus", "audio/ogg; codecs=opus"},
    {"sid", "audio/prs.sid"},
    {"spx", "audio/ogg; codecs=spx"},
    {"ts", "video/MP2T"},
    {"vorbis", "audio/ogg; codec=vorbis"},
    {"wave", "audio/vnd.wave"},
    {"wav", "audio/vnd.wave"},
    {"webm", "video/webm"},
    {"wma", "audio/x-ms-wma"},
    {nullptr, nullptr}};

/**
 * @class MimeResolver
 * @brief Lookup MIME types by file extension.
 *
 * Small, header-only helper that maps file name extensions (for example
 * "mp3" or ".wav") to standardized MIME type strings (for example
 * "audio/mpeg" or "audio/vnd.wave"). The resolver performs a
 * case-insensitive match and accepts extensions with or without a
 * leading '.'.
 *
 * Behavior:
 * - The lookup table is a fixed, const array (`mime_table`) declared above
 *   the class. Modify that table to add or change mappings.
 * - Lookup returns a pointer to a constant string from the table. The
 *   returned pointer is valid for the lifetime of the program. If no
 *   mapping is found, the methods return nullptr.
 * - Thread-safety: read-only operations are safe for concurrent use.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MimeResolver {
 public:
  /// Return MIME for a filename (looks up extension). Returns nullptr if
  /// unknown.
  const char* fromFilename(const char* filename) {
    if (!filename) return nullptr;
    const char* dot = strrchr(filename, '.');
    if (!dot || *(dot + 1) == '\0') return nullptr;
    return fromExtension(dot + 1);
  }

  /// Return MIME for an extension (case-insensitive). Returns nullptr if
  /// unknown.
  const char* fromExtension(const char* extension) {
    if (!extension) return nullptr;
    // skip leading dot if provided
    if (extension[0] == '.') extension++;
    StrView extView(extension);

    // search in custom table first
    for (size_t i = 0; i < custom_mime_table.size(); i++) {
      if (extView.equalsIgnoreCase(custom_mime_table[i].ext))
        return custom_mime_table[i].mime;
    }

    // search in default table
    for (size_t i = 0; mime_table[i].ext != nullptr; i++) {
      if (extView.equalsIgnoreCase(mime_table[i].ext))
        return mime_table[i].mime;
    }
    return nullptr;
  }

  /// Add custom mime entry (overrides default entries)
  void addMimeEntry(const char* ext, const char* mime) {
    MimeEntry entry;
    entry.ext = ext;
    entry.mime = mime;
    custom_mime_table.push_back(entry);
  }

 protected:
  Vector<MimeEntry> custom_mime_table;
};

}  // namespace audio_tools
