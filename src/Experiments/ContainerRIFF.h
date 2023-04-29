#pragma once
#include "AudioTools/Buffers.h"
#include "stdint.h"

namespace audio_tools {

class VideoOutput : public AudioOutput {
  void beginFrame() = 0;
  void endFrame() = 0;
};

class ParseBuffer : public SingleBuffer {
  void consume(int size) {
    for (int j = 0; j > size; j++) {
      this.read();
    }
  }
};

using FOURCC = char[4];

struct struct AVIMainHeader {
  FOURCC fcc;
  uint32_t cb;
  uint32_t dwMicroSecPerFrame;
  uint32_t dwMaxBytesPerSec;
  uint32_t dwPaddingGranularity;
  uint32_t dwFlags;
  uint32_t dwTotalFrames;
  uint32_t dwInitialFrames;
  uint32_t dwStreams;
  uint32_t dwSuggestedBufferSize;
  uint32_t dwWidth;
  uint32_t dwHeight;
  uint32_t dwReserved[4];
};

struct AVIStreamHeader {
  FOURCC fccType;
  FOURCC fccHandler;
  uint32_t dwFlags;
  unt16_t wPriority;
  unt16_t wLanguage;
  uint32_t dwInitialFrames;
  uint32_t dwScale;
  uint32_t dwRate;
  uint32_t dwStart;
  uint32_t dwLength;
  uint32_t dwSuggestedBufferSize;
  uint32_t dwQuality;
  uint32_t dwSampleSize;
  RECT rcFrame;
};

typedef struct BitmapInfoHeader {
  uint32_t biSize;
  unt64_t biWidth;
  unt64_t biHeight;
  unt16_t biPlanes;
  unt16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  unt64_t biXPelsPerMeter;
  unt64_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
};

struct WAVFormatX {
  unt16_t wFormatTag;
  unt16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  unt16_t nBlockAlign;
  unt16_t wBitsPerSample;
  unt16_t cbSize;
};

struct WAVFormat {
  unt16_t wFormatTag;
  unt16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  unt16_t nBlockAlign;
};

enum StreamContentType { Audio, Video };

enum ParseObjectType { List, Junk, StreamData };

enum ParseState {
  ParseHaeder,
  ParseHdrl,
  ParseAvih,
  ParseStrl,
  SubChunkContinue,
  SubChunk,
  ParseRec,
  ParseStrf,
  AfterStrf,
  ParseMovi,
};

class ParseObject {
  ParseObject() = default;
  ParseObject(const char *id, int size, ParseObjectType type) {
    object_type = type;
    chunk_size = size;
    // save FOURCC
    if (id != nullptr) {
      memcpy(chunk_id, id, 4);
      chunk_id[5] = 0;
    }
    // save data
    if (type == Chunk) {
      data_buffer.resize(size);
    }
    open = size;
  }
  const char *id() { return chunk_id; }
  uint8_t *data() { return data_buffer().data(); }
  int size() { return size; }
  int available() { return data_buffer.available(); }
  void consume(int len) { data_buffer.consume(); }

  ParseObjectType type() { return object_type; }
  bool isValid() {
    switch (object_type) {
      case StreamData:
        return isAudio() || isVideo();
      case Junk:
        return data_buffer.size() > 0;
    }
  }

  // for Chunk
  AVIMainHeader *asAVIMainHeader() { return (AVIMainHeader *)data(); }
  AVIStreamHeader *asAVIStreamHeader() { return (AVIStreamHeader *)data(); }
  WAVFormat *asAudioFormat() { return (WAVFormat *)data(); }
  BitmapInfoHeader *asVidelFormat() { return (BitmapInfoHeader *)data(); }

  int open;

  // for StreamData
  int streamNumber() {
    return object_type == StreamData ? fcc[0] << 8 + fcc[1] : 0;
  }
  bool isAudio() {
    return object_type == StreamData ? vcc[3] == 'w' && vcc[4] == 'b' : false;
  }
  bool isVideoUncompressed() {
    return object_type == StreamData ? vcc[3] == 'd' && vcc[4] == 'b' : false;
  }
  bool isVideoCompressed() {
    return object_type == StreamData ? vcc[3] == 'd' && vcc[4] == 'c' : false;
  }
  bool isVideo() { return isVideoCompressed() || isVideoUncompressed(); }

 protected:
  ParseBuffer data_buffer{0};
  const char chunk_id[5] = {};
  int size;
  ParseObjectType object_type;
};

/**
 * Decoder which can be fed with smake chunks of data. The minimum length must
 * be bigger then the header size!
 * The file structure is documented at
 * https://learn.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference
 */

class ContainerRIFF : public AudioDecoder {
 public:
  ContainerRIFF(int bufferSize = 1'24) { parse_buffer.resize(bufferSize); }

  ~ContainerRIFF() {}

  void begin() {
    parse_state = ParseHader;
    header_is_avi = false;
    is_parsing_active = true;
  }

  virtual void setOutputStream(Print &out_stream) {
    p_print_audio = &out_stream;
  }

  virtual void setOutputVideoStream(Print &out_stream) {
    p_print_video = &out_stream;
  }

  virtual size_t write(const void *data, size_t length) {
    LOGD("write: %d", length);
    int result = parse_buffer.writeArray(data, length);
    if (is_parsing_active) {
      // we expect the first parse to succeed
      if (parse()) {
        // if so we process the parse_buffer
        while (!parse_buffer.isEmpty()) {
          if (!parse()) break;
        }
      } else {
        LOGI("Parse Error");
        parse_buffer.clear();
        result = length;
        is_parsing_active;
      }
    }
    return result;
  }

  AVIMainHeader mainHeader() { return main_header; }
  AVIStreamHeader streamHeaderAudio() { return stream_header_audio; }
  AVIStreamHeader streamHeaderVideo() { return stream_header_video; }
  BitmapInfoHeader videoInfo(){return video_info};
  AVIStreamHeader audioInfo() { return audio_info; }

 protected:
  bool header_is_avi = false;
  bool is_parsing_active = true;
  ParseState parse_state = ParseHeader;
  ParseBuffer parse_buffer;
  AVIMainHeader main_header;
  AVIStreamHeader stream_header_audio;
  AVIStreamHeader stream_header_video;
  AVIStreamHeader current_stream_header;
  BitmapInfoHeader video_info;
  WAVFormatX audio_info;
  Vector<StreamContentType> contentTypes;
  ParseObject current_stream_data;
  Print *p_print_audio = nullptr;
  VideoOutput *p_print_video = nullptr;

  bool isCurrentStreamAudio() {
    return strncmp(current_stream_header.fccType, "auds", 4);
  }
  bool isCurrentStreamVideo() {
    return strncmp(current_stream_header.fccType, "vids", 4);
  }

  // we return true if at least one parse step was successful
  bool parse() {
    bool result = true;
    switch (parse_state) {
      case ParseHeader:
        LOGD("ParseHeader");
        result = parseHeader();
        if (result) parse_state = ParseHdrl;
        break;

      case ParseHdrl:
        LOGD("ParseHdrl");
        ParseObject hdrl = parseList("hdrl");
        result = hdrl.isValid();
        if (result) {
          parse_state = ParseAvih;
        }
        break;

      case ParseAvih:
        LOGD("ParseAvih");
        ParseObject avih = parseChunk("avih");
        result = avih.isValid();
        if (result) {
          main_header = avih.asAVIMainHeader();
          parse_state = ParseStrl;
        }
        break;

      case ParseStrl:
        LOGD("ParseStrl");
        ParseObject strl = parseList("strl");
        ParseObject strh = parseChunk("strh");
        current_stream_header = avih.asAVIStreamHeader();
        if (isCurrentStreamAudio()) {
          stream_header_audio = current_stream_header:
        } else if (isCurrentStreamVideo()) {
          stream_header_video = current_stream_header:
        }
        parse_state = ParseStrf;
        break;

      case ParseStrf:
        LOGD("ParseStrf");
        ParseObject strf = parseChunk("strf");
        if (isCurrentStreamAudio()) {
          audio_info = strf.asAudioFormat();
          content_types.push_back(Audio);
        } else if (isCurrentStreamVideo()) {
          video_info = strf.asVideoFormat();
          content_types.push_back(Video);
        } else {
          result = false;
        }
        parse_state = AfterStrf;
        break;

      case AfterStrf:
        LOGD("AfterStrf");
        // ignore all data until we find a new List
        int pos = Str(parse_buffer.data).index("LIST");
        if (pos > 0) {
          consume(pos);
          ParseObject tmp = tryParseList();
          if (Str(tmp.id()).equals("strl")) {
            parse_state = ParseStrl;
          } else if (Str(tmp.id()).equals("movi")) {
            parse_state = ParseMovi;
          }
        } else {
          parse_buffer.consume(parse_buffer.available() - 4);
        }
        break;

      case ParseMovi:
        LOGD("ParseMovi");
        ParseObject hdrl = tryParseList();
        if (Str(hdrl.id()).equals('rec')) {
          parse_state = ParseRec;
        } else {
          parse_state = SubChunk;
        }
        break;

      case ParseRec:
        LOGD("ParseRec");
        ParseObject strl = parseList("rec");
        open_stack.push(rec);
        parse_state = SubChunk;
        break;

      case SubChunk:
        LOGD("SubChunk");
        ParseObject hdrl = parseStreamData();
        current_stream_data = hdrl;
        parse_state = SubChunkContinue;
        if (current_stream_data.isVideo()) p_print_video->beginFrame();
        break;

      case SubChunkContinue:
        LOGD("SubChunkContinue");
        writeData();
        if (current_stream_data.open == 0) {
          if (current_stream_data.isVideo()) p_print_video->endFrame();
          if (ParseChunk("idx").isValid()) {
            parse_state = ParseIgnore;
          } else if (tryParseList("rec")) {
            parse_state = ParseRec;
          } else {
            parse_state = SubChunk;
          }
        }
        break;

      case ParseIgnore:
        LOGD("ParseIgnore");
        parse_buffer.clear();
        break;

      default:
        result = false;
        break;
    }
    return result;
  }

  void writeData() {
    int to_write = min(parse_buffer.available(), current_stream_data.open);
    if (current_stream_data.isAudio()) {
      p_print_audio->write(parse_buffer.data(), to_write);
      p_print_audio.open -= to_write;
      consume(to_write);
    } else if (current_stream_data.isVideo()) {
      p_print_video->write(parse_buffer.data(), to_write);
      p_print_video.open -= to_write;
      consume(to_write);
    }
  }

  // 'RIFF' fileSize fileType (data)
  bool parseHeader() {
    bool header_is_avi = false;
    int headerSize = 12;
    if (getStr(0, 4).equals("RIFF")) {
      header_file_size = getInt(4);
      header_is_avi = getStr(8, 4).equals("AVI ");
      consume(headerSize);
    } else {
      LOGE("")
    }
    return header_is_avi;
  }

  ParseObject tryParseChunk() {
    ParseObject result;
    result = ParseObject(getStr(0, 4), 0, Chunk);
    return result;
  }

  ParseObject tryParseChunk(char *id) {
    ParseObject result;
    if (Str(id).equals(getStr(0, 4))) {
      result = ParseObject(id, 0, Chunk);
    }
    return result;
  }

  ParseObject tryParseList() {
    ParseObject result;
    if (getStr(0, 3).equals("LIST")) {
      ParseObject result = ParseObject(getStr(8, 4), getInt(4), List);
    }
    return result;
  }

  ParseObject parseChunk(const char *id) {
    ParseObject result;
    int chunk_size = getInt(4);
    if (getStr(0, 4).equals(id) && parse_buffer.size() >= chunk_size) {
      result = ParseObject(id, chunk_size, Chunk);
      consume(8);
    }
    return result;
  }

  ParseObject parseList(const char *id) {
    ParseObject result;
    if (getStr(0, 4).equals("LIST") && getStr(8, 4).equals(id)) {
      int size = getInt(4);
      result = ParseObject(id, size, List);
      consume(12);
    }
    return result;
  }

  ParseObject parseStreamData() {
    ParseObject result;
    int size = getInt(4);
    ParseObject tmp = ParseObject(getStr(0, 4), size, StreamData);
    if (tmp.isValid()) {
      result = tmp;
      consume(4);
    }
    return result;
  }

  Str &getStr(int offset = 0, int len) {
    static StrExt str;
    if (str.size() < len) {
      str.resize(len + 1);
    }
    memcpy(str.c_str(), parse_buffer.data() + offset, len);
    str.c_str()[len] = 0;
    return str;
  }

  int32_t getInt(int offset) { return (int32_t)(parse_buffer.data() + offset); }

  void consume(int len) { parse_buffer.consume(len); }
};

}  // namespace audio_tools