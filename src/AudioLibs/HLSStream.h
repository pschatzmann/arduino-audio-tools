#pragma once
#include "AudioCodecs/AudioEncoded.h"
#include "AudioConfig.h"

#ifdef USE_URL_ARDUINO
#include "AudioBasic/StrExt.h"
#include "AudioHttp/URLStream.h"
#include "AudioLibs/Concurrency.h"

#define MAX_HLS_LINE 512
#define START_URLS_LIMIT 4
#define HLS_BUFFER_COUNT 10

namespace audio_tools {

/// @brief Abstract API for URLLoaderHLS
class URLLoaderHLSBase {
 public:
  virtual bool begin() = 0;
  virtual void end() = 0;
  virtual void addUrl(const char *url) = 0;
  virtual int urlCount() = 0;
  virtual int available() { return 0; }
  virtual size_t readBytes(uint8_t *data, size_t len) { return 0; }
  const char *contentType() { return nullptr; }
  int contentLength() { return 0; }

  virtual void setBuffer(int size, int count) {}
};

/// URLLoader which saves the HLS segments to the indicated output
class URLLoaderHLSOutput {
 public:
  URLLoaderHLSOutput(Print &out, int maxUrls = 20) {
    max = maxUrls;
    p_print = &out;
  }
  virtual bool begin() { return true; };
  virtual void end(){};
  virtual void addUrl(const char *url) {
    LOGI("saving data for %s", url);
    url_stream.begin(url);
    url_stream.waitForData(500);
    copier.begin(*p_print, url_stream);
    int bytes_copied = copier.copyAll();
    LOGI("Copied %d of %d", bytes_copied, url_stream.contentLength());
    assert(bytes_copied == url_stream.contentLength());
    url_stream.end();
  }
  virtual int urlCount() { return 0; }

 protected:
  int count = 0;
  int max = 20;
  Print *p_print;
  URLStream url_stream;
  StreamCopy copier;
};

/***
 * @brief We feed the URLLoaderHLS with some url strings. The data of the
 * related segments are provided via the readBytes() method.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class URLLoaderHLS : public URLLoaderHLSBase {
 public:
  // URLLoaderHLS(URLStream &stream) { p_stream = &stream; };
  URLLoaderHLS() = default;

  ~URLLoaderHLS() { end(); }

  bool begin() override {
    TRACED();
#if USE_TASK
    buffer.resize(buffer_size * buffer_count);
    task.begin(std::bind(&URLLoaderHLS::bufferRefill, this));
#else
    buffer.resize(buffer_size * buffer_count);
#endif

    active = true;
    return true;
  }

  void end() override {
    TRACED();
#if USE_TASK
    task.end();
#endif
    if (p_stream != nullptr) p_stream->end();
    p_stream = nullptr;
    buffer.clear();
    active = false;
  }

  /// Adds the next url to be played in sequence
  void addUrl(const char *url) override {
    LOGI("Adding %s", url);
    Str url_str(url);
    char *str = new char[url_str.length() + 1];
    memcpy(str, url_str.c_str(), url_str.length() + 1);
#if USE_TASK
    LockGuard lock_guard{mutex};
#endif
    urls.push_back((const char*)str);
  }

  /// Provides the number of open urls which can be played. Refills them, when
  /// min limit is reached.
  int urlCount() override { return urls.size(); }

  /// Available bytes of the audio stream
  int available() override {
    if (!active) return 0;
    TRACED();
#if !USE_TASK
    bufferRefill();
#endif
    return buffer.available();
  }

  /// Provides data from the audio stream
  size_t readBytes(uint8_t *data, size_t len) override {
    if (!active) return 0;
    TRACED();
#if !USE_TASK
    bufferRefill();
#endif
    if (buffer.available() < len) LOGW("Buffer underflow");
    return buffer.readArray(data, len);
  }

  const char *contentType() {
    if (p_stream == nullptr) return nullptr;
    return p_stream->httpRequest().reply().get(CONTENT_TYPE);
  }

  int contentLength() {
    if (p_stream == nullptr) return 0;
    return p_stream->contentLength();
  }

  void setBuffer(int size, int count) override {
    buffer_size = size;
    buffer_count = count;
  }

 protected:
  Vector<const char *> urls{10};
#if USE_TASK
  BufferRTOS<uint8_t> buffer{0};
  Task task{"Refill", 1024 * 5, 1, 1};
  Mutex mutex;
#else
  RingBuffer<uint8_t> buffer{0};
#endif
  bool active = false;
  int buffer_size = DEFAULT_BUFFER_SIZE;
  int buffer_count = HLS_BUFFER_COUNT;
  URLStream default_stream;
  URLStream *p_stream = &default_stream;
  const char *url_to_play = nullptr;

  /// try to keep the buffer filled
  void bufferRefill() {
    TRACED();
    // we have nothing to do
    if (urls.empty()) {
      LOGD("urls empty");
      delay(10);
      return;
    }
    if (buffer.availableForWrite() == 0) {
      LOGD("buffer full");
      delay(10);
      return;
    }

    // switch current stream if we have no more data
    if (!*p_stream && !urls.empty()) {
      LOGD("Refilling");
      if (url_to_play != nullptr) {
        delete url_to_play;
      }
      url_to_play = urls[0];
      LOGI("playing %s", url_to_play);
      p_stream->setTimeout(5000);
      p_stream->begin(url_to_play);
      p_stream->waitForData(500);
#if USE_TASK
      LockGuard lock_guard{mutex};
#endif
      urls.pop_front();
      // assert(urls[0]!=url);

#ifdef ESP32
      LOGI("Free heap: %u", (unsigned)ESP.getFreeHeap());
#endif
      LOGI("Playing %s of %d", p_stream->urlStr(), (int)urls.size());
    }

    int total = 0;
    int failed = 0;
    int to_write = min(buffer.availableForWrite(), DEFAULT_BUFFER_SIZE);
    // try to keep the buffer filled
    while (to_write > 0) {
      uint8_t tmp[to_write] = {0};
      int read = p_stream->readBytes(tmp, to_write);
      total += read;
      if (read > 0) {
        failed = 0;
        buffer.writeArray(tmp, read);
        LOGI("buffer add %d -> %d:", read, buffer.available());

        to_write = min(buffer.availableForWrite(), DEFAULT_BUFFER_SIZE);
      } else {
        delay(10);
        // this should not really happen
        failed++;
        LOGW("No data idx %d: available: %d", failed, p_stream->available());
        if (failed >= 5) {
          LOGE("No data idx %d: available: %d", failed, p_stream->available());
          if (p_stream->available() == 0) p_stream->end();
          break;
        }
      }
      // After we processed all data we close the stream to get a new url
      if (p_stream->totalRead() == p_stream->contentLength()) {
        p_stream->end();
        break;
      }
      LOGD("Refilled with %d now %d available to write", total,
           buffer.availableForWrite());
    }
  }
};

/**
 * Prevent that the same url is loaded twice. We limit the history to
 * 20 entries.
 */
class URLHistory {
 public:
  bool add(const char *url) {
    bool found = false;
    Str url_str(url);
    for (int j = 0; j < history.size(); j++) {
      if (url_str.equals(history[j])) {
        found = true;
        break;
      }
    }
    if (!found) {
      char *str = new char[url_str.length() + 1];
      memcpy(str, url, url_str.length() + 1);
      history.push_back((const char*)str);
      if (history.size() > 20) {
        delete (history[0]);
        history.pop_front();
      }
    }
    return !found;
  }

  void clear() { history.clear(); }

  int size() { return history.size(); }

 protected:
  Vector<const char *> history;
};

/**
 * @brief Simple Parser for HLS data. We select the entry with min bandwidth
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class HLSParser {
 public:
  // loads the index url
  bool begin(const char *urlStr) {
    index_url_str = urlStr;
    return begin();
  }

  bool begin() {
    TRACEI();
    custom_log_level.set();
    segments_url_str = "";
    bandwidth = 0;
    if (!parseIndex()) {
      TRACEE();
      return false;
    }
    if (!parseSegments()) {
      TRACEE();
      return false;
    }

    if (!p_url_loader->begin()) {
      TRACEE();
      return false;
    }

#if USE_TASK
    segment_load_task.begin(std::bind(&HLSParser::reloadSegments, this));
#endif

    custom_log_level.reset();
    return true;
  }

  int available() {
    TRACED();
    int result = 0;
    custom_log_level.set();
#if !USE_TASK
    reloadSegments();
#endif
    if (active) result = p_url_loader->available();
    custom_log_level.reset();
    return result;
  }

  size_t readBytes(uint8_t *data, size_t len) {
    TRACED();
    size_t result = 0;
    custom_log_level.set();
#if !USE_TASK
    reloadSegments();
#endif
    if (active) result = p_url_loader->readBytes(data, len);
    custom_log_level.reset();
    return result;
  }

  const char *indexUrl() { return index_url_str; }

  const char *segmentsUrl() {
    if (segments_url_str == nullptr) return nullptr;
    return segments_url_str.c_str();
  }

  /// Provides the codec
  const char *getCodec() { return codec.c_str(); }

  /// Provides the content type of the audio data
  const char *contentType() { return p_url_loader->contentType(); }

  int contentLength() { return p_url_loader->contentLength(); }

  /// Closes the processing
  void end() {
    TRACEI();
#if USE_TASK
    segment_load_task.end();
#endif
    codec.clear();
    segments_url_str.clear();
    url_stream.end();
    p_url_loader->end();
    url_history.clear();
    active = false;
  }

  /// Defines the number of urls that are preloaded in the URLLoaderHLS
  void setUrlCount(int count) { url_count = count; }

  /// Defines the class specific custom log level
  void setLogLevel(AudioLogger::LogLevel level) { custom_log_level.set(level); }

  void setBuffer(int size, int count) { p_url_loader->setBuffer(size, count); }

  void setUrlLoader(URLLoaderHLSBase &loader) { p_url_loader = &loader; }

 protected:
  CustomLogLevel custom_log_level;
  int bandwidth = 0;
  int url_count = 5;
  bool url_active = false;
  bool is_extm3u = false;
  StrExt codec;
  StrExt segments_url_str;
  StrExt url_str;
  const char *index_url_str = nullptr;
  URLStream url_stream;
  URLLoaderHLS default_url_loader;
  URLLoaderHLSBase *p_url_loader = &default_url_loader;
  URLHistory url_history;
#if USE_TASK
  Task segment_load_task{"Refill", 1024 * 5, 1, 1};
#endif
  bool active = false;
  bool parse_segments_active = false;
  int media_sequence = 0;
  int tartget_duration_ms = 5000;
  int segment_count = 0;
  uint64_t next_sement_load_time = 0;

  // trigger the reloading of segments if the limit is underflowing
  void reloadSegments() {
    TRACED();
    // get new urls
    if (!segments_url_str.isEmpty()) {
      parseSegments();
    }
  }

  // parse the index file and the segments
  bool parseIndex() {
    TRACED();
    url_stream.setTimeout(5000);
    // url_stream.setConnectionClose(true);

    // we only update the content length
    url_stream.setAutoCreateLines(false);
    bool rc = url_stream.begin(index_url_str);
    url_active = true;
    rc = parseIndexLines();
    return rc;
  }

  // parse the index file 
  bool parseIndexLines() {
    TRACEI();
    char tmp[MAX_HLS_LINE];
    bool result = true;
    is_extm3u = false;

    // parse lines
    memset(tmp, 0, MAX_HLS_LINE);
    while (true) {
      memset(tmp, 0, MAX_HLS_LINE);
      size_t len =
          url_stream.httpRequest().readBytesUntil('\n', tmp, MAX_HLS_LINE);
      if (len == 0 && url_stream.available() == 0) break;
      Str str(tmp);

      // check header
      if (str.indexOf("#EXTM3U") >= 0) {
        is_extm3u = true;
      }

      if (is_extm3u) {
        if (!parseIndexLine(str)) {
          return false;
        }
      }
    }
    return result;
  }


  // parse the segment url provided by the index
  bool parseSegments() {
    TRACED();
    if (parse_segments_active) {
      return false;
    }

    // make sure that we load at relevant schedule
    if (millis() < next_sement_load_time && p_url_loader->urlCount() > 1) {
      delay(1);
      return false;
    }
    parse_segments_active = true;

    LOGI("Available urls: %d", p_url_loader->urlCount());

    if (url_stream) url_stream.clear();
    LOGI("parsing %s", segments_url_str.c_str());

    if (segments_url_str.isEmpty()) {
      TRACEE();
      parse_segments_active = false;
      return false;
    }

    if (!url_stream.begin(segments_url_str.c_str())) {
      TRACEE();
      parse_segments_active = false;
      return false;
    }

    segment_count = 0;
    if (!parseSegmentLines()) {
      TRACEE();
      parse_segments_active = false;
      // do not display as erro
      return true;
    }

    next_sement_load_time = millis() + (segment_count * tartget_duration_ms);
    // assert(segment_count > 0);

    // we request a minimum of collected urls to play before we start
    if (url_history.size() > START_URLS_LIMIT) active = true;
    parse_segments_active = false;

    return true;
  }

  // parse the segments
  bool parseSegmentLines() {
    TRACEI();
    char tmp[MAX_HLS_LINE];
    bool result = true;
    is_extm3u = false;

    // parse lines
    memset(tmp, 0, MAX_HLS_LINE);
    while (true) {
      memset(tmp, 0, MAX_HLS_LINE);
      size_t len =
          url_stream.httpRequest().readBytesUntil('\n', tmp, MAX_HLS_LINE);
      if (len == 0 && url_stream.available() == 0) break;
      Str str(tmp);

      // check header
      if (str.indexOf("#EXTM3U") >= 0) {
        is_extm3u = true;
      }

      if (is_extm3u) {
        if (!parseSegmentLine(str)) {
          return false;
        }
      }
    }
    return result;
  }

  // Add all segments to queue
  bool parseSegmentLine(Str &str) {
    TRACED();
    LOGI("> %s", str.c_str());

    int pos = str.indexOf("#");
    if (pos >= 0) {
      LOGI("-> Segment:  %s", str.c_str());

      pos = str.indexOf("#EXT-X-MEDIA-SEQUENCE:");
      if (pos >= 0) {
        int new_media_sequence = atoi(str.c_str() + pos + 22);
        LOGI("media_sequence: %d", new_media_sequence);
        if (new_media_sequence == media_sequence) {
          LOGW("MEDIA-SEQUENCE already loaded: %d", media_sequence);
          return false;
        }
        media_sequence = new_media_sequence;
      }

      pos = str.indexOf("#EXT-X-TARGETDURATION:");
      if (pos >= 0) {
        const char *duration_str = str.c_str() + pos + 22;
        tartget_duration_ms = 1000 * atoi(duration_str);
        LOGI("tartget_duration_ms: %d (%s)", tartget_duration_ms, duration_str);
      }
    } else {
      segment_count++;
      if (url_history.add(str.c_str())) {
        // provide audio urls to the url_loader
        if (str.startsWith("http")) {
          url_str = str;
        } else {
          // we create the complete url
          url_str = segments_url_str;
          url_str.add("/");
          url_str.add(str.c_str());
        }
        p_url_loader->addUrl(url_str.c_str());
      } else {
        LOGD("Duplicate ignored: %s", str.c_str());
      }
    }
    return true;
  }

  // Determine codec for min bandwidth
  bool parseIndexLine(Str &str) {
    TRACED();
    LOGI("> %s", str.c_str());
    int tmp_bandwidth;
    if (str.indexOf("EXT-X-STREAM-INF") >= 0) {
      // determine min bandwidth
      int pos = str.indexOf("BANDWIDTH=");
      if (pos > 0) {
        Str num(str.c_str() + pos + 10);
        tmp_bandwidth = num.toInt();
        url_active = (tmp_bandwidth < bandwidth || bandwidth == 0);
        if (url_active) {
          bandwidth = tmp_bandwidth;
          LOGD("-> bandwith: %d", bandwidth);
        }
      }

      pos = str.indexOf("CODECS=");
      if (pos > 0) {
        int start = pos + 8;
        int end = str.indexOf('"', pos + 10);
        codec.substring(str, start, end);
        LOGI("-> codec: %s", codec.c_str());
      }
    }

    if (str.startsWith("http")) {
      // check if we have a valid codec
      segments_url_str.set(str);
      LOGD("segments_url_str = %s", str.c_str());
    }

    return true;
  }
};

/**
 * @brief HTTP Live Streaming using HLS: The result is a MPEG-TS data stream
 * that must be decoded e.g. with a DecoderMTS.
 *
 * @author Phil Schatzmann
 * @ingroup http *@copyright GPLv3
 */

class HLSStream : public AudioStream {
 public:
  HLSStream() = default;

  HLSStream(const char *ssid, const char *password) {
    setSSID(ssid);
    setPassword(password);
  }

  bool begin(const char *urlStr) {
    TRACEI();
    login();
    // parse the url to the HLS
    bool rc = parser.begin(urlStr);
    return rc;
  }

  bool begin() {
    TRACEI();
    login();
    bool rc = parser.begin();
    return rc;
  }

  // ends the request
  void end() { parser.end(); }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char *ssid) { this->ssid = ssid; }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char *password) { this->password = password; }

  /// Returns the string representation of the codec of the audio stream
  const char *codec() { return parser.getCodec(); }

  const char *contentType() { return parser.contentType(); }

  int contentLength() { return parser.contentLength(); }

  int available() override {
    TRACED();
    return parser.available();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    return parser.readBytes(data, len);
  }

  /// Defines the class specific custom log level
  void setLogLevel(AudioLogger::LogLevel level) { parser.setLogLevel(level); }

  /// Defines the buffer size
  void setBuffer(int size, int count) { parser.setBuffer(size, count); }

 protected:
  HLSParser parser;
  const char *ssid = nullptr;
  const char *password = nullptr;

  void login() {
#ifdef USE_WIFI
    if (ssid != nullptr && password != nullptr &&
        WiFi.status() != WL_CONNECTED) {
      TRACED();
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
      }
    }
#else
    LOGW("login not supported");
#endif
  }
};

}  // namespace audio_tools

#endif