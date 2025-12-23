

#pragma once
#include "AudioSource.h"
#include "AudioTools/Communication/HTTP/AbstractURLStream.h"
#include "AudioToolsConfig.h"

namespace audio_tools {

/**
 * @brief Audio Source which provides the data via the network from an URL
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceURL : public AudioSource {
 public:
  template <typename T, size_t N>
  AudioSourceURL(AbstractURLStream& urlStream, T (&urlArray)[N],
                 const char* mime, int startPos = 0) {
    TRACED();
    this->actual_stream = &urlStream;
    this->mime = mime;
    this->urlArray = urlArray;
    this->max = N;
    this->pos = startPos - 1;
    this->timeout_auto_next_value = 20000;
  }

  virtual ~AudioSourceURL() { end(); }

  /// Setup Wifi URL
  virtual bool begin() override {
    TRACED();
    this->pos = 0;
    return true;
  }

  void end() {
    if (started) actual_stream->end();
    started = false;
  }

  /// Opens the selected url from the array
  Stream* selectStream(int idx) override {
    if (size() == 0) return nullptr;
    pos = idx;
    if (pos < 0) {
      pos = 0;
      LOGI("url array out of limits: %d -> %d", idx, pos);
    }
    if (pos >= size()) {
      pos = size() - 1;
      LOGI("url array out of limits: %d -> %d", idx, pos);
    }
    LOGI("selectStream: %d/%d -> %s", pos, size() - 1, value(pos));
    if (started) actual_stream->end();
    const char* url = value(pos);
    if (url == nullptr) return nullptr;
    actual_stream->begin(url, mime);
    started = true;
    return actual_stream;
  }

  /// Opens the next url from the array
  Stream* nextStream(int offset) override {
    pos += offset;
    if (pos < 0 || pos >= size()) {
      pos = 0;
    }
    LOGI("nextStream: %d/%d -> %s", pos, max - 1, value(pos));
    return selectStream(pos);
  }

  /// Opens the Previous url from the array
  Stream* previousStream(int offset) override {
    pos -= offset;
    if (pos < 0 || pos >= size()) {
      pos = size() - 1;
    }
    LOGI("previousStream: %d/%d -> %s", pos, size() - 1, value(pos));
    return selectStream(pos);
  }

  /// Opens the selected url
  Stream* selectStream(const char* path) override {
    LOGI("selectStream: %s", path);
    if (started) actual_stream->end();
    actual_stream->begin(path, mime);
    started = true;
    return actual_stream;
  }

  int index() { return pos; }

  const char* toStr() { return value(pos); }

  /// Sets the timeout of the URL Stream in milliseconds
  void setTimeout(int millisec) { actual_stream->setTimeout(millisec); }

  // provides go not to the next on error
  virtual bool isAutoNext() { return true; };

  // only the ICYStream supports this
  bool setMetadataCallback(void (*fn)(MetaDataType info, const char* str,
                                      int len),
                           ID3TypeSelection sel = SELECT_ICY) {
    TRACEI();
    return actual_stream->setMetadataCallback(fn);
  }

  virtual int size() { return max; }

 protected:
  AbstractURLStream* actual_stream = nullptr;
  const char** urlArray = nullptr;
  int pos = 0;
  int max = 0;
  const char* mime = nullptr;
  bool started = false;

  /// allow use with empty constructor in subclasses
  AudioSourceURL() = default;

  virtual const char* value(int pos) {
    if (urlArray == nullptr || size() == 0) return nullptr;
    return urlArray[pos];
  }
};

/**
 * @brief Audio Source which provides the data via the network from an URL.
 * The URLs are stored in an Vector of dynamically allocated strings.
 *
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioSourceDynamicURL : public AudioSourceURL {
 public:
  template <typename T, size_t N>
  AudioSourceDynamicURL(AbstractURLStream& urlStream, T (&urlArray)[N],
                        const char* mime, int startPos = 0) {
    this->actual_stream = &urlStream;
    this->mime = mime;
    this->pos = startPos - 1;
    this->timeout_auto_next_value = 20000;
    for (int j = 0; j < N; j++) {
      addURL(urlArray[j]);
    }
  }

  AudioSourceDynamicURL(AbstractURLStream& urlStream,
                        const char* mime = nullptr, int startPos = 0) {
    this->actual_stream = &urlStream;
    this->mime = mime;
    this->pos = startPos - 1;
    this->timeout_auto_next_value = 20000;
  }

  /// add a new url: a copy of the string will be stored on the heap
  void addURL(const char* url) { url_vector.push_back(url); }

  void clear() { url_vector.clear(); }

  int size() override { return url_vector.size(); }

  /// Opens the selected url and adds it to the list
  Stream* selectStream(const char* path) override {
    LOGI("selectStream: %s", path);
    addURL(path);
    if (started) actual_stream->end();
    actual_stream->begin(path, mime);
    started = true;
    return actual_stream;
  }

 protected:
  Vector<Str> url_vector;

  const char* value(int pos) override { return url_vector[pos].c_str(); }
};

}  // namespace audio_tools
