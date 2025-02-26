#pragma once

namespace audio_tools {

/**
 * @brief  Logic to detemine the mime type from the content.
 * By default the following mime types are supported (audio/aac, audio/mpeg,
 * audio/vnd.wave, audio/ogg). You can register your own custom detection logic
 * to cover additional file types.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MimeDetector {
 public:
  bool begin() {
    is_first = true;
    return true;
  }

  size_t write(uint8_t* data, size_t len) {
    determineMime(data, len);
    return len;
  }

  /// Defines the mime detector
  void setMimeDetector(const char* (*mimeDetectCallback)(uint8_t* data,
                                                         size_t len)) {
    this->mimeDetectCallback = mimeDetectCallback;
  }

  /// Define the callback that will notify about mime changes
  void setMimeCallback(void (*callback)(const char*)) {
    TRACED();
    this->notifyMimeCallback = callback;
  }

  /// Provides the actual mime type, that was determined from the first
  /// available data
  const char* mime() { return actual_mime; }

 protected:
  bool is_first = false;
  const char* actual_mime = nullptr;
  void (*notifyMimeCallback)(const char* mime) = nullptr;
  const char* (*mimeDetectCallback)(uint8_t* data,
                                    size_t len) = defaultMimeDetector;

  /// Update the mime type
  void determineMime(void* data, size_t len) {
    if (is_first) {
      actual_mime = mimeDetectCallback((uint8_t*)data, len);
      if (notifyMimeCallback != nullptr && actual_mime != nullptr) {
        notifyMimeCallback(actual_mime);
      }
      is_first = false;
    }
  }

  /// Default logic which supports aac, mp3, wav and ogg
  static const char* defaultMimeDetector(uint8_t* data, size_t len) {
    const char* mime = nullptr;
    if (len > 4) {
      const uint8_t* start = (const uint8_t*)data;
      if (start[0] == 0xFF && (start[1] == 0xF0 || start[1] == 0xF1 || start[1] == 0xF9)) {
        mime = "audio/aac";
      } else if (memcmp(start, "ID3", 3) == 0 || (start[0] == 0xFF && start[1] == 0xFE)) {
        mime = "audio/mpeg";
      } else if (memcmp(start, "RIFF", 4) == 0) {
        mime = "audio/vnd.wave";
      } else if (memcmp(start, "OggS", 4) == 0) {
        mime = "audio/ogg";
      }
    }
    if (mime != nullptr) {
      LOGI("Determined mime: %s", mime);
    }
    return mime;
  }
};

}  // namespace audio_tools
