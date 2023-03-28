#pragma once
#include "AudioConfig.h"
#ifdef USE_URL_ARDUINO
#include "AudioBasic/StrExt.h"
#include "AudioCodecs/CodecAACHelix.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioHttp/URLStream.h"

#define MAX_HLS_LINE 200

namespace audio_tools {

/**
 * @brief HTTP Live Streaming using HLS. We use a simplified parsing that
 * supports AAC and MP3.
 * @author Phil Schatzmann
 * @ingroup http *@copyright GPLv3
 */

class HLSStream : public AbstractURLStream {
public:
  // executes the URL request
  bool begin(const char *urlStr, const char *acceptMime = nullptr,
             MethodID action = GET, const char *reqMime = "",
             const char *reqData = "") override {
    // parse the url to the HLS
    bool rc = parser.begin(urlStr);
    if (!rc) {
      LOGW("HLS parsing failed");
    }
    return rc;
  }
  // ends the request
  void end() override { parser.end(); }

  /// provides access to the HttpRequest
  HttpRequest &httpRequest() override {
    return parser.getURLStream().httpRequest();
  }

  // only the ICYStream supports this
  bool setMetadataCallback(void (*fn)(MetaDataType info, const char *str,
                                      int len)) override {
    return false;
  }

  /// Writes are not supported
  int availableForWrite() override {
    return parser.getURLStream().availableForWrite();
  }

  int available() { return parser.decodedStream().available(); }

  size_t readBytes(uint8_t *data, size_t len) override {
    size_t result = parser.decodedStream().readBytes(data, len);
    if (result == 0 && !parser.getSegments().empty()) {
      if (parser.nextStream()) {
        result = parser.decodedStream().readBytes(data, len);
      }
    }
    return result;
  }

  /// (Re-)defines the client
  void setClient(Client &clientPar) override {
    parser.getURLStream().setClient(clientPar);
  }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char *ssid) override {
    parser.getURLStream().setSSID(ssid);
  }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char *password) override {
    parser.getURLStream().setPassword(password);
  }

protected:
  struct CodecEntry {
    const char *name;
    AudioDecoder *(*create)() = nullptr;
    void (*release)() = nullptr;
    CodecEntry() = default;
    CodecEntry(CodecEntry &e) = default;
    CodecEntry(const char *name, AudioDecoder *(*create)(), void (*release)()) {
      this->name = name;
      this->create = create;
      this->release = release;
    }
  };

  /**
  /* Codec Management
   */
  class HLSCodecManagement {
  public:
    void addCodec(const char *name, AudioDecoder *(*create)(),
                  void (*release)()) {
      CodecEntry entry{name, create, release};
      codecs.push_back(entry);
    }

    bool isValid(const char *name) {
      for (auto it = codecs.begin(); it != codecs.end(); ++it) {
        if (Str(name).startsWith(it->name)) {
          return true;
        }
      }
      return false;
    }

    AudioDecoder *create(const char *name) {
      for (auto it = codecs.begin(); it != codecs.end(); ++it) {
        if (Str(name).startsWith(it->name)) {
          LOGI("Using codec: %s", it->name);
          release = it->release;
          return it->create();
        }
      }
      return nullptr;
    }

    void end() {
      if (release) {
        release();
      }
    }

  protected:
    List<CodecEntry> codecs;
    void (*release)() = nullptr;
  };

  /**
   * Simple Parser for HLS data. We select the entry with min bandwidth
   */
  class HLSParser {
  public:
    bool begin(const char *urlStr) {
      url_str = "";
      bool rc = url_stream.begin(urlStr);
      if (rc)
        rc = parse();

      if (rc)
        rc = codecSetup();

      if (rc)
        rc = beginStream();

      if (rc)
        rc = nextStream();

      return rc;
    }

    bool beginStream() {
      dec_stream.setDecoder(decoder);
      dec_stream.setStream(&url_stream);
      return dec_stream.begin();
    }

    // parse the index file and the segments
    bool parse() {
      char tmp[MAX_HLS_LINE];
      bool result = true;
      Str str(tmp);
      url_stream.readBytesUntil('\n', tmp, MAX_HLS_LINE);
      if (!str.startsWith("#EXTM3U")) {
        LOGE("Not a EXTM3U");
        return false;
      }

      // parse lines
      while (url_stream.available()) {
        url_stream.readBytesUntil('\n', tmp, MAX_HLS_LINE);
        LOGD(tmp);
        if (process_index) {
          parseIndex(str);
        } else {
          parseSegments(str);
        }
      }

      // load segments
      if (process_index && url_stream.available()) {
        if (url_stream.begin(url_str.c_str())) {
          process_index = false;
          result = parse();
        }
      }
      return result;
    }

    Queue<StrExt> &getSegments() { return segments; }

    URLStream &getURLStream() { return url_stream; }

    void end() {
      segments.clear();
      codec.clear();
      url_str.clear();
      url_stream.end();
      dec_stream.end();
      codec_mgmt.end();
    }

    EncodedAudioStream &decodedStream() { return dec_stream; }

    /// Get the decoded data from the next segment
    bool nextStream() {
      bool result = false;
      StrExt url1;
      URLStream &url_stream = getURLStream();
      if (getSegments().dequeue(url1)) {
        url_stream.end();
        LOGI("playing %s", url1.c_str());
        StrExt tmp;
        tmp.set(url_str.c_str());
        tmp.add("/");
        tmp.add(url1.c_str());
        result = url_stream.begin(tmp.c_str());
      }
      return result;
    }

    void addCodec(const char *name, AudioDecoder *(*create)(),
                  void (*remove)()) {
      codec_mgmt.addCodec(name, create, remove);
    }
    // Determine the decoder
    bool codecSetup() {
      codec_mgmt.end();
      decoder = codec_mgmt.create(codec.c_str());
      return decoder != nullptr;
    }

    bool codecIsValid(const char *name) { return codec_mgmt.isValid(name); }

    bool codecCreate(const char *name) { return codec_mgmt.create(name); }

    void codecDelete() { codec_mgmt.end(); }

  protected:
    int bandwidth = 0;
    bool process_index = true;
    bool url_active = false;
    StrExt codec;
    StrExt url_str;
    Queue<StrExt> segments;
    URLStream url_stream;
    AudioDecoder *decoder = nullptr;
    EncodedAudioStream dec_stream;
    HLSCodecManagement codec_mgmt;

    void parseSegments(Str &str) {
      if (!str.startsWith("#")) {
        StrExt ts = str;
        LOGI("-> %s", ts.c_str());
        segments.enqueue(ts);
      }
    }

    void parseIndex(Str &str) {
      int tmp_bandwidth;
      if (str.startsWith("#EXT-X-STREAM-INF:")) {
        // determine min bandwidth
        int pos = str.indexOf("BANDWIDTH=");
        if (pos > 0) {
          Str num(str.c_str() + pos + 10);
          tmp_bandwidth = num.toInt();
          url_active = (tmp_bandwidth < bandwidth || bandwidth == 0);
          if (url_active) {
            bandwidth = tmp_bandwidth;
            LOGI("-> %d", bandwidth);
          }
        }
        pos = str.indexOf("CODECS=");
        if (pos > 0) {
          int start = pos + 8;
          int end = str.indexOf('"', pos + 10);
          codec.substring(str, start, end);
          LOGI("-> %s", codec.c_str());
        }
        // update url with min bandwidth
        if (url_active && codecIsValid(codec.c_str()) &&
            str.startsWith("http")) {
          url_str = str;
          LOGI("-> %s", url_str.c_str());
        }
      }
    }

  } parser;
};

} // namespace audio_tools

#endif