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
 * @brief HTTP Live Streaming using HLS. We use a simplified parsing that
 * supports registed decoders.
 *
 * http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_world_service/bbc_world_service.isml/bbc_world_service-audio%3d96000.norewind.m3u8/bbc_world_service-audio=96000-262505003.ts

 * @author Phil Schatzmann
 * @ingroup http *@copyright GPLv3
 */

class HLSStream {
 public:
  HLSStream(AudioStream &out) {
    setOutput(out);
  };

  HLSStream(AudioStream &out, const char *ssid, const char *password) {
    setOutput(out);
    setSSID(ssid);
    setPassword(password);
  }

  // executes the URL request
  bool begin(const char *urlStr) {
    // parse the url to the HLS
    bool rc = parser.begin(urlStr);

    if (rc) {
      rc = beginEncodedAudioStream();
    if (rc)
      rc = parser.nextStream();

    if(!rc)
      LOGW("HLS failed");
    }
    return rc;
  }
  // ends the request
  void end()  { parser.end(); }

  /// provides access to the HttpRequest
  HttpRequest &httpRequest()  {
    return parser.getURLStream().httpRequest();
  }

  /// (Re-)defines the client
  void setClient(Client &clientPar)  {
    parser.getURLStream().setClient(clientPar);
  }

  /// Sets the ssid that will be used for logging in (when calling begin)
  void setSSID(const char *ssid)  {
    parser.getURLStream().setSSID(ssid);
  }

  /// Sets the password that will be used for logging in (when calling begin)
  void setPassword(const char *password)  {
    parser.getURLStream().setPassword(password);
  }

  /// Defines a codec for the indicated type string
  void setOutput(AudioStream &out) { p_out = &out;}

  /// Registers a decoder
  void addDecoder(const char *name, AudioDecoder &codec) {
    parser.addDecoder(name, codec);
  }

  int copy() {
    uint8_t tmp[512];
    size_t len = readBytes(tmp, 512);
    size_t len_write = dec_stream.write(tmp, len);
    LOGI("copy %d -> %d", len, len_write);
    return len;
  }

 protected:
  /**
   * An individual decoder with it's name
   */
  struct CodecEntry {
    const char *name;
    AudioDecoder *p_decoder;
    CodecEntry() = default;
    CodecEntry(CodecEntry &e) = default;
    CodecEntry(const char *name, AudioDecoder &codec) {
      this->name = name;
      this->p_decoder = &codec;
    }
  };

  /**
   * Codec Management: We collect the decoders in a list and retrieve them by
   * the name that is used in the codec specification of the hls
   */
  class HLSCodecManagement {
   public:
    void addDecoder(const char *name, AudioDecoder &decoder) {
      CodecEntry entry{name, decoder};
      codecs.push_back(entry);
    }

    bool isValid(const char *name) {
      if (name == nullptr) return false;

      for (auto it = codecs.begin(); it != codecs.end(); ++it) {
        if (Str(name).startsWith(it->name)) {
          return true;
        }
      }
      return false;
    }

    AudioDecoder *create(const char *name) {
      if (name == nullptr) return nullptr;
      for (auto it = codecs.begin(); it != codecs.end(); ++it) {
        if (Str(name).startsWith(it->name)) {
          LOGI("Using codec: %s", it->name);
          p_current_codec = it->p_decoder;
          return p_current_codec;
        }
      }
      return nullptr;
    }

    void end() {
      if (p_current_codec) {
        p_current_codec->end();
      }
    }

   protected:
    List<CodecEntry> codecs;
    AudioDecoder *p_current_codec = nullptr;
  };

  /**
   * Simple Parser for HLS data. We select the entry with min bandwidth
   */
  class HLSParser {
   public:
    bool begin(const char *urlStr) {
      url_str = "";
      LOGI("-------------------");
      LOGI("Loading index: %s", urlStr);
      url_stream.setTimeout(1000);
      url_stream.setConnectionClose(false);
      // we only update the content length
      url_stream.httpRequest().reply().put(CONTENT_LENGTH,0);
      url_stream.setAutoCreateLines(false);
      bool rc = url_stream.begin(urlStr);
      if (rc) rc = parse(true);

      if (rc) rc = codecSetup();

      // if (rc) rc = beginStream();

      // release attributes
      url_stream.clear();
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
      while (url_stream.httpRequest().readBytesUntil('\n', tmp, MAX_HLS_LINE)) {
        Str str(tmp);
        if (!str.startsWith("#EXTM3U")) {
          is_extm3u = true;
        }

        if (process_index) {
          parseIndex(str);
        } else {
          parseSegments(str);
        }
        // clear buffer
        memset(tmp, 0, MAX_HLS_LINE);
      }

      // load segments
      if (process_index && !url_str.isEmpty()) {
        LOGI("-------------------");
        endUrlStream();
        LOGI("Load segments from: %s", url_str.c_str());
        if (url_stream.begin(url_str.c_str())) {
          process_index = false;
          result = parse(false);
        }
      }
      return result;
    }

    Queue<StrExt> &getSegments() { return segments; }

    URLStream &getURLStream() { return url_stream; }

    void end() {
      TRACED();
      segments.clear();
      codec.clear();
      url_str.clear();
      endUrlStream();
//      dec_stream.end();
      codec_mgmt.end();
    }

//    EncodedAudioStream &decodedStream() { return dec_stream; }

    /// Get the decoded data from the next segment
    bool nextStream() {
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
          tmp.set(url_str.c_str());
          tmp.add("/");
          tmp.add(url1.c_str());
        }
        LOGI("-------------------");
        LOGI("playing %s", tmp.c_str());
        endUrlStream();
        result = url_stream.begin(tmp.c_str(), "audio/mp4a", GET);
      }
      return result;
    }

    void endUrlStream() {
      TRACED();
      url_stream.end();
      TRACED();
    }

    void addDecoder(const char *name, AudioDecoder &codec) {
      codec_mgmt.addDecoder(name, codec);
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

    AudioDecoder *getDecoder(){
      return decoder;
    }

   protected:
    int bandwidth = 0;
    bool url_active = false;
    bool is_extm3u = false;
    StrExt codec;
    StrExt url_str;
    Queue<StrExt> segments;
    URLStream url_stream;
    AudioDecoder *decoder = nullptr;
    HLSCodecManagement codec_mgmt;

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
          url_str.set(str);
          if (codecIsValid(codec.c_str())) {
            LOGI("-> url: %s", url_str.c_str());
          } else {
            LOGW("Url ignored because there is no codec for %s", codec.c_str());
          }
        }
      }
    }

  } parser;
  EncodedAudioStream dec_stream;
  AudioStream *p_out = nullptr;

  bool beginEncodedAudioStream() {
    if (p_out==nullptr) return false;
    if (parser.getDecoder()==nullptr) return false;
    dec_stream.setStream(p_out);
    dec_stream.setDecoder(parser.getDecoder());
    dec_stream.setNotifyAudioChange(*p_out);
    return dec_stream.begin();
  }

  size_t readBytes(uint8_t *data, size_t len) {
    size_t result = parser.getURLStream().readBytes(data, len);
    if (result == 0 && !parser.getSegments().empty()) {
      if (parser.nextStream()) {
        result = parser.getURLStream().readBytes(data, len);
      }
    }
    return result;
  }
};

}  // namespace audio_tools

#endif