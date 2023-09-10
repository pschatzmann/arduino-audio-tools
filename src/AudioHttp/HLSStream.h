#pragma once
#include "AudioCodecs/AudioEncoded.h"
#include "AudioConfig.h"

#ifdef USE_URL_ARDUINO
#include "AudioBasic/StrExt.h"
#include "AudioCodecs/CodecAACHelix.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioHttp/URLStream.h"

#define MAX_HLS_LINE 200

namespace audio_tools {

/**
 * Simple Parser for HLS data. We select the entry with min bandwidth
 */
class HLSParser {
 public:
  bool begin(const char *urlStr) {
    index_url_str = urlStr;
    segments_url_str = "";
    bandwidth = 0;
    LOGI("Loading index: %s", index_url_str);
    url_stream.setTimeout(1000);
    url_stream.setConnectionClose(false);
    // we only update the content length
    url_stream.httpRequest().reply().put(CONTENT_LENGTH, 0);
    url_stream.setAutoCreateLines(false);

    bool rc = url_stream.begin(index_url_str);
    if (rc) rc = parse(true);
    return rc;
  }

  bool begin() {
    TRACEI();
    segments_url_str = "";
    bandwidth = 0;
    LOGI("-------------------");
    LOGI("Loading index: %s", index_url_str);
    bool rc = url_stream.begin(index_url_str);
    if (rc) rc = parse(true);
    return rc;
  }

  // parse the index file and the segments
  bool parse(bool process_index) {
    LOGI("parsing %s", process_index ? "Index" : "Segements")
    char tmp[MAX_HLS_LINE];
    bool result = true;
    is_extm3u = false;

    // parse lines
    memset(tmp, 0, MAX_HLS_LINE);
    while (url_stream.available()) {
      memset(tmp, 0, MAX_HLS_LINE);
      url_stream.httpRequest().readBytesUntil('\n', tmp, MAX_HLS_LINE);
      Str str(tmp);

      if (!str.startsWith("#EXTM3U")) {
        is_extm3u = true;
      }

      if (process_index) {
        parseIndex(str);
      } else {
        parseSegments(str);
      }
    }

    // load segments
    if (process_index && !segments_url_str.isEmpty()) {
      endUrlStream();
      LOGI("Load segments from: %s", segments_url_str.c_str());
      if (url_stream.begin(segments_url_str.c_str())) {
        result = parse(false);
      }
    }
    return result;
  }

  Queue<StrExt> &getSegments() { return segments; }

  /// Provide access to the actual data stream
  URLStream &getURLStream() { return url_stream; }

  /// Get the  data from the next segment
  bool nextStream() {
    TRACEI();
    bool result = false;
    StrExt url1;
    URLStream &url_stream = getURLStream();
    if (getSegments().dequeue(url1)) {
      endUrlStream();
      StrExt tmp;
      // if the segment is a complete http url we use it
      if (url1.startsWith("http")) {
        tmp.set(url1.c_str());
      } else {
        // we create the complete url
        tmp.set(segments_url_str.c_str());
        tmp.add("/");
        tmp.add(url1.c_str());
      }
      LOGI("-------------------");
      LOGI("playing %s", tmp.c_str());
      endUrlStream();
      result = url_stream.begin(tmp.c_str(), "audio/mp4a", GET);
    } else {
      LOGW("No more segments");
    }
    return result;
  }

  /// Provides the codec
  const char *getCodec() { return codec.c_str(); }

  const char *contentType() {
    return url_stream.httpRequest().reply().get(CONTENT_TYPE);
  }

  const char *contentLength() {
    return url_stream.httpRequest().reply().get(CONTENT_LENGTH);
  }

  /// Closes the processing
  void end() {
    TRACED();
    segments.clear();
    codec.clear();
    segments_url_str.clear();
    endUrlStream();
  }

 protected:
  int bandwidth = 0;
  bool url_active = false;
  bool is_extm3u = false;
  StrExt codec;
  StrExt segments_url_str;
  const char *index_url_str = nullptr;
  Queue<StrExt> segments;
  URLStream url_stream;

  void endUrlStream() {
    TRACED();
    url_stream.end();
  }

  // Add all segments to queue
  void parseSegments(Str str) {
    TRACED();
    LOGI("> %s", str.c_str());

    if (!str.startsWith("#")) {
      LOGI("-> Segment:  %s", str.c_str());
      StrExt ts = str;
      segments.enqueue(ts);
    }
  }

  // Determine codec for min badnwidth
  void parseIndex(Str str) {
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

    if (url_active && str.startsWith("http")) {
      if (str.startsWith("http")) {
        // check if we have a valid codec
        segments_url_str.set(str);
      }
    }
  }
};

/**
 * @brief HTTP Live Streaming using HLS. The result is a MPEG-TS data stream
 * that must be decuded with a DecoderMTS.
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
    // parse the url to the HLS
    bool rc = parser.begin(urlStr);

    // trigger first access to data
    available();
    return rc;
  }

  bool begin() { return parser.begin(); }

  // ends the request
  void end() { parser.end(); }

  /// provides access to the HttpRequest
  HttpRequest &httpRequest() { return parser.getURLStream().httpRequest(); }

  /// (Re-)defines the client
  void setClient(Client &clientPar) {
    parser.getURLStream().setClient(clientPar);
  }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char *ssid) { parser.getURLStream().setSSID(ssid); }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char *password) {
    parser.getURLStream().setPassword(password);
  }

  /// Returns the string representation of the codec of the audio stream
  const char *codec() { return parser.getCodec(); }

  const char *contentType() {
    return parser.contentType();
  }

  const char *contentLength() {
    return parser.contentLength();
  }


  int available() override {
    Stream &urlStream = getURLStream();
    int result = urlStream.available();
    if (result == 0) {
      if (!parser.nextStream()) {
        // we consumed all segments so we get new ones
        begin();
      }
      result = urlStream.available();
    }
    return result;
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    Stream &urlStream = getURLStream();
    size_t result = 0;
    if (urlStream.available() > 0) {
      result = urlStream.readBytes(data, len);
    }
    return result;
  }

 protected:
  HLSParser parser;
  Print *p_out = nullptr;
  AudioInfoSupport *p_ai = nullptr;

  Stream &getURLStream() { return parser.getURLStream(); }
};

}  // namespace audio_tools

#endif