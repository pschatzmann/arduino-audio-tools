#pragma once
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioTools/CoreAudio/AudioHttp/URLStream.h"
#include "AudioTools/CoreAudio/StreamCopy.h"
#include "AudioToolsConfig.h"

#define MAX_HLS_LINE 512
#define START_URLS_LIMIT 4
#define HLS_BUFFER_COUNT 2
#define HLS_MAX_NO_READ 2
#define HLS_MAX_URL_LEN 256
#define HLS_TIMEOUT 5000
#define HLS_UNDER_OVERFLOW_WAIT_TIME 10

/// hide hls implementation in it's own namespace

namespace audio_tools_hls {

/***
 * @brief We feed the URLLoaderHLS with some url strings. The data of the
 * related segments are provided via the readBytes() method.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <typename URLStream>
class URLLoaderHLS {
 public:
  URLLoaderHLS() = default;

  ~URLLoaderHLS() { end(); }

  bool begin() {
    TRACED();
    buffer.resize(buffer_size * buffer_count);

    active = true;
    return true;
  }

  void end() {
    TRACED();
    url_stream.end();
    buffer.clear();
    active = false;
  }

  /// Adds the next url to be played in sequence
  void addUrl(const char *url) {
    LOGI("Adding %s", url);
    StrView url_str(url);
    char *str = new char[url_str.length() + 1];
    memcpy(str, url_str.c_str(), url_str.length() + 1);
    urls.push_back((const char *)str);
  }

  /// Provides the number of open urls which can be played. Refills them, when
  /// min limit is reached.
  int urlCount() { return urls.size(); }

  /// Available bytes of the audio stream
  int available() {
    if (!active) return 0;
    TRACED();
    bufferRefill();

    return buffer.available();
  }

  /// Provides data from the audio stream
  size_t readBytes(uint8_t *data, size_t len) {
    if (!active) return 0;
    TRACED();
    bufferRefill();

    if (buffer.available() < len) LOGW("Buffer underflow");
    return buffer.readArray(data, len);
  }

  const char *contentType() {
    return url_stream.httpRequest().reply().get(CONTENT_TYPE);
  }

  int contentLength() { return url_stream.contentLength(); }

  void setBufferSize(int size, int count) {
    buffer_size = size;
    buffer_count = count;
    // support call after begin()!
    if (buffer.size() != 0) {
      buffer.resize(buffer_size * buffer_count);
    }
  }

  void setCACert(const char *cert) { url_stream.setCACert(cert); }

 protected:
  Vector<const char *> urls{10};
  RingBuffer<uint8_t> buffer{0};
  bool active = false;
  int buffer_size = DEFAULT_BUFFER_SIZE;
  int buffer_count = HLS_BUFFER_COUNT;
  URLStream url_stream;
  const char *url_to_play = nullptr;

  /// try to keep the buffer filled
  void bufferRefill() {
    TRACED();
    // we have nothing to do
    if (urls.empty()) {
      LOGD("urls empty");
      delay(HLS_UNDER_OVERFLOW_WAIT_TIME);
      return;
    }
    if (buffer.availableForWrite() == 0) {
      LOGD("buffer full");
      delay(HLS_UNDER_OVERFLOW_WAIT_TIME);
      return;
    }

    // switch current stream if we have no more data
    if (!url_stream && !urls.empty()) {
      LOGD("Refilling");
      if (url_to_play != nullptr) {
        delete url_to_play;
      }
      url_to_play = urls[0];
      LOGI("playing %s", url_to_play);
      url_stream.end();
      url_stream.setConnectionClose(true);
      url_stream.setTimeout(HLS_TIMEOUT);
      url_stream.begin(url_to_play);
      url_stream.waitForData(HLS_TIMEOUT);
      urls.pop_front();
      // assert(urls[0]!=url);

      LOGI("Playing %s of %d", url_stream.urlStr(), (int)urls.size());
    }

    int total = 0;
    int failed = 0;
    int to_write = min(buffer.availableForWrite(), DEFAULT_BUFFER_SIZE);
    // try to keep the buffer filled
    while (to_write > 0) {
      uint8_t tmp[to_write] = {0};
      int read = url_stream.readBytes(tmp, to_write);
      total += read;
      if (read > 0) {
        failed = 0;
        buffer.writeArray(tmp, read);
        LOGD("buffer add %d -> %d:", read, buffer.available());

        to_write = min(buffer.availableForWrite(), DEFAULT_BUFFER_SIZE);
      } else {
        delay(10);
      }
      // After we processed all data we close the stream to get a new url
      if (url_stream.totalRead() == url_stream.contentLength()) {
        LOGI("Closing stream because all bytes were processed: available: %d",
             url_stream.available());
        url_stream.end();
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
    if (url == nullptr) return true;
    bool found = false;
    StrView url_str(url);
    for (int j = 0; j < history.size(); j++) {
      if (url_str.equals(history[j])) {
        found = true;
        break;
      }
    }
    if (!found) {
      char *str = new char[url_str.length() + 1];
      memcpy(str, url, url_str.length() + 1);
      history.push_back((const char *)str);
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
 * @brief Simple Parser for HLS data.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename URLStream>
class HLSParser {
 public:
  // loads the index url
  bool begin(const char *urlStr) {
    index_url_str = urlStr;
    return begin();
  }

  bool begin() {
    TRACEI();
    segments_url_str = "";
    bandwidth = 0;
    total_read = 0;

    if (!parseIndex()) {
      TRACEE();
      return false;
    }

    // in some exceptional cases the index provided segement info
    if (url_loader.urlCount() == 0) {
      if (!parseSegments()) {
        TRACEE();
        return false;
      }
    } else {
      segments_url_str = index_url_str;
      segmentsActivate();
    }

    if (!url_loader.begin()) {
      TRACEE();
      return false;
    }

    return true;
  }

  int available() {
    TRACED();
    int result = 0;
    reloadSegments();

    if (active) result = url_loader.available();
    return result;
  }

  size_t readBytes(uint8_t *data, size_t len) {
    TRACED();
    size_t result = 0;
    reloadSegments();

    if (active) result = url_loader.readBytes(data, len);
    total_read += result;
    return result;
  }

  const char *indexUrl() { return index_url_str; }

  const char *segmentsUrl() { return segments_url_str.c_str(); }

  /// Provides the codec
  const char *getCodec() { return codec.c_str(); }

  /// Provides the content type of the audio data
  const char *contentType() { return url_loader.contentType(); }

  /// Provides the http content lengh
  int contentLength() { return url_loader.contentLength(); }

  /// Closes the processing
  void end() {
    TRACEI();
    codec.clear();
    segments_url_str.clear();
    url_stream.end();
    url_loader.end();
    url_history.clear();
    active = false;
  }

  /// Defines the number of urls that are preloaded in the URLLoaderHLS
  void setUrlCount(int count) { url_count = count; }

  /// Redefines the buffer size
  void setBufferSize(int size, int count) {
    url_loader.setBufferSize(size, count);
  }

  void setCACert(const char *cert) {
    url_stream.setCACert(cert);
    url_loader.setCACert(cert);
  }

  void setPowerSave(bool flag) { url_stream.setPowerSave(flag); }

  void setURLResolver(const char *(*cb)(const char *segment,
                                        const char *reqURL)) {
    resolve_url = cb;
  }
  /// Provides the hls url as string
  const char *urlStr() { return url_str.c_str(); }

  /// Povides the number of bytes read 
  size_t totalRead() { return total_read; };
 
  protected:
  enum class URLType { Undefined, Index, Segment };
  URLType next_url_type = URLType::Undefined;
  int bandwidth = 0;
  int url_count = 5;
  size_t total_read = 0;
  bool url_active = false;
  bool is_extm3u = false;
  Str codec;
  Str segments_url_str;
  Str url_str;
  const char *index_url_str = nullptr;
  URLStream url_stream;
  URLLoaderHLS<URLStream> url_loader;
  URLHistory url_history;
  bool active = false;
  bool parse_segments_active = false;
  int media_sequence = 0;
  int segment_count = 0;
  uint64_t next_sement_load_time_planned = 0;
  float play_time = 0;
  uint64_t next_sement_load_time = 0;
  const char *(*resolve_url)(const char *segment,
                             const char *reqURL) = resolveURL;

  /// Default implementation for url resolver: determine absolue url from
  /// relative url
  static const char *resolveURL(const char *segment, const char *reqURL) {
    // avoid dynamic memory allocation
    static char result[HLS_MAX_URL_LEN] = {0};
    StrView result_str(result, HLS_MAX_URL_LEN);
    StrView index_url(reqURL);
    // Use prefix up to ? or laast /
    int end = index_url.lastIndexOf("?");
    if (end >= 0) {
      result_str.substring(reqURL, 0, end);
    } else {
      end = index_url.lastIndexOf("/");
      if (end >= 0) {
        result_str.substring(reqURL, 0, end);
      }
    }
    // Use the full url
    if (result_str.isEmpty()) {
      result_str = reqURL;
    }
    // add trailing /
    if (!result_str.endsWith("/")) {
      result_str.add("/");
    }
    // add relative segment
    result_str.add(segment);
    LOGI(">> relative addr: %s for %s", segment, reqURL);
    LOGD(">> ->  %s", result);
    return result;
  }

  /// trigger the reloading of segments if the limit is underflowing
  void reloadSegments() {
    TRACED();
    // get new urls
    if (!segments_url_str.isEmpty()) {
      parseSegments();
    }
  }

  /// parse the index file and the segments
  bool parseIndex() {
    TRACED();
    url_stream.end();
    url_stream.setTimeout(HLS_TIMEOUT);
    url_stream.setConnectionClose(true);
    if (!url_stream.begin(index_url_str)) return false;
    url_active = true;
    return parseIndexLines();
  }

  /// parse the index file
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
      // stop when there is no more data
      if (len == 0 && url_stream.available() == 0) break;
      StrView str(tmp);

      // check header
      if (str.startsWith("#EXTM3U")) {
        is_extm3u = true;
        // reset timings
        resetTimings();
      }

      if (is_extm3u) {
        if (!parseIndexLine(str)) {
          return false;
        }
      }
    }
    return result;
  }

  /// Determine codec for min bandwidth
  bool parseIndexLine(StrView &str) {
    TRACED();
    LOGI("> %s", str.c_str());
    parseIndexLineMetaData(str);
    // in some exceptional cases the index provided segement info
    parseSegmentLineMetaData(str);
    parseLineURL(str);
    return true;
  }

  bool parseIndexLineMetaData(StrView &str) {
    int tmp_bandwidth;
    if (str.startsWith("#")) {
      if (str.indexOf("EXT-X-STREAM-INF") >= 0) {
        next_url_type = URLType::Index;
        // determine min bandwidth
        int pos = str.indexOf("BANDWIDTH=");
        if (pos > 0) {
          StrView num(str.c_str() + pos + 10);
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
    }
    return true;
  }

  void resetTimings() {
    next_sement_load_time_planned = millis();
    play_time = 0;
    next_sement_load_time = 0xFFFFFFFFFFFFFFFF;
  }

  /// parse the segment url provided by the index
  bool parseSegments() {
    TRACED();
    if (parse_segments_active) {
      return false;
    }

    // make sure that we load at relevant schedule
    if (millis() < next_sement_load_time && url_loader.urlCount() > 1) {
      delay(1);
      return false;
    }
    parse_segments_active = true;

    LOGI("Available urls: %d", url_loader.urlCount());

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
      // do not display as error
      return true;
    }

    segmentsActivate();
    return true;
  }

  void segmentsActivate() {
    LOGI("Reloading in %f sec", play_time / 1000.0);
    if (play_time > 0) {
      next_sement_load_time = next_sement_load_time_planned + play_time;
    }

    // we request a minimum of collected urls to play before we start
    if (url_history.size() > START_URLS_LIMIT) active = true;
    parse_segments_active = false;
  }

  /// parse the segments
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
      StrView str(tmp);

      // check header
      if (str.startsWith("#EXTM3U")) {
        is_extm3u = true;
        resetTimings();
      }

      if (is_extm3u) {
        if (!parseSegmentLine(str)) {
          return false;
        }
      }
    }
    return result;
  }

  /// Add all segments to queue
  bool parseSegmentLine(StrView &str) {
    TRACED();
    LOGI("> %s", str.c_str());
    if (!parseSegmentLineMetaData(str)) return false;
    parseLineURL(str);
    return true;
  }

  bool parseSegmentLineMetaData(StrView &str) {
    if (str.startsWith("#")) {
      if (str.startsWith("#EXT-X-MEDIA-SEQUENCE:")) {
        int new_media_sequence = atoi(str.c_str() + 22);
        LOGI("media_sequence: %d", new_media_sequence);
        if (new_media_sequence == media_sequence) {
          LOGW("MEDIA-SEQUENCE already loaded: %d", media_sequence);
          return false;
        }
        media_sequence = new_media_sequence;
      }

      // add play time to next_sement_load_time_planned
      if (str.startsWith("#EXTINF")) {
        next_url_type = URLType::Segment;
        StrView sec_str(str.c_str() + 8);
        float sec = sec_str.toFloat();
        LOGI("adding play time: %f sec", sec);
        play_time += (sec * 1000.0);
      }
    }
    return true;
  }

  bool parseLineURL(StrView &str) {
    if (!str.startsWith("#")) {
      switch (next_url_type) {
        case URLType::Undefined:
          // we should not get here
          assert(false);
          break;
        case URLType::Index:
          if (str.startsWith("http")) {
            segments_url_str.set(str);
          } else {
            segments_url_str.set(resolve_url(str.c_str(), index_url_str));
          }
          LOGD("segments_url_str = %s", segments_url_str.c_str());
          break;
        case URLType::Segment:
          segment_count++;
          if (url_history.add(str.c_str())) {
            // provide audio urls to the url_loader
            if (str.startsWith("http")) {
              url_str = str;
            } else {
              // we create the complete url
              url_str = resolve_url(str.c_str(), index_url_str);
            }
            url_loader.addUrl(url_str.c_str());
          } else {
            LOGD("Duplicate ignored: %s", str.c_str());
          }
      }
      // clear url type
      next_url_type = URLType::Undefined;
    }
    return true;
  }
};

}  // namespace audio_tools_hls

namespace audio_tools {
/**
 * @brief HTTP Live Streaming using HLS: The resulting .ts data is provided
 * via readBytes() that dynamically reload new Segments. Please note that
 * this reloading adds a considerable delay: So if you want to play back the
 * audio, you should buffer the content in a seaparate task.
 *
 * @author Phil Schatzmann
 * @ingroup http *@copyright GPLv3
 */

template <typename URLStream>
class HLSStreamT : public AbstractURLStream {
 public:
  /// Empty constructor
  HLSStreamT() = default;

  /// Convenience constructor which logs in to the WiFi
  HLSStreamT(const char *ssid, const char *password) {
    setSSID(ssid);
    setPassword(password);
  }

  /// Open an HLS url
  bool begin(const char *urlStr) {
    TRACEI();
    login();
    // parse the url to the HLS
    bool rc = parser.begin(urlStr);
    return rc;
  }

  /// Reopens the last url
  bool begin() {
    TRACEI();
    login();
    bool rc = parser.begin();
    return rc;
  }

  /// ends the request
  void end() { parser.end(); }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char *ssid) { this->ssid = ssid; }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char *password) { this->password = password; }

  /// Returns the string representation of the codec of the audio stream
  const char *codec() { return parser.getCodec(); }

  /// Provides the content type from the http reply
  const char *contentType() { return parser.contentType(); }

  /// Provides the content length of the actual .ts Segment
  int contentLength() { return parser.contentLength(); }

  /// Provides number of available bytes in the read buffer
  int available() override {
    TRACED();
    return parser.available();
  }

  /// Provides the data fro the next .ts Segment
  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    return parser.readBytes(data, len);
  }

  /// Redefines the read buffer size
  void setBufferSize(int size, int count) { parser.setBufferSize(size, count); }

  /// Defines the certificate
  void setCACert(const char *cert) { parser.setCACert(cert); }

  /// Changes the Wifi to power saving mode
  void setPowerSave(bool flag) { parser.setPowerSave(flag); }

  /// Custom logic to provide the codec as Content-Type to support the
  /// MultiCodec
  const char *getReplyHeader(const char *header) {
    const char *codec = parser.getCodec();
    const char *result = nullptr;
    if (StrView(header).equalsIgnoreCase(CONTENT_TYPE)) {
      result = parser.contentType();
    }
    if (result) LOGI("-> Format: %s", result);
    return result;
  }

  /// The resolving of relative addresses can be quite tricky: you can provide
  /// your custom resolver implementation
  void setURLResolver(const char *(*cb)(const char *segment,
                                        const char *reqURL)) {
    parser.setURLResolver(cb);
  }

  const char *urlStr() override { return parser.urlStr(); }

  size_t totalRead() override { return parser.totalRead(); };
  /// not implemented
  void setConnectionClose(bool flag) override {};
  /// not implemented
  bool waitForData(int timeout) override { return false; }

 protected:
  audio_tools_hls::HLSParser<URLStream> parser;
  const char *ssid = nullptr;
  const char *password = nullptr;

  void login() {
#ifdef USE_WIFI
    if (ssid != nullptr && password != nullptr &&
        WiFi.status() != WL_CONNECTED) {
      TRACED();
      delay(1000);
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

  /// Added to comply with AbstractURLStream
  bool begin(const char *urlStr, const char *acceptMime, MethodID action = GET,
             const char *reqMime = "", const char *reqData = "") {
    return begin(urlStr);
  }

  HttpRequest &httpRequest() {
    static HttpRequest dummy;
    return dummy;
  }

  /// Not implemented: potential future improvement
  void setClient(Client &clientPar) {}

  /// Not implemented
  void addRequestHeader(const char *header, const char *value) {}
};

using HLSStream = HLSStreamT<URLStream>;

}  // namespace audio_tools
