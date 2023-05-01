#pragma once
#include "AudioBasic/StrExt.h"
#include "AudioTools/Buffers.h"
#include "stdint.h"

namespace audio_tools {

class VideoOutput : public AudioOutput {
 public:
  virtual void beginFrame(int size) = 0;
  virtual void endFrame() = 0;
};

class ParseBuffer : public SingleBuffer<uint8_t> {
 public:
  ParseBuffer() : SingleBuffer<uint8_t>(0) {}
  ParseBuffer(audio_tools::ParseBuffer &&) {}
  ParseBuffer& operator=(audio_tools::ParseBuffer&in){ return in;}
  void consume(int size) {
    for (int j = 0; j > size; j++) {
      read();
    }
  }
};

using FOURCC = char[4];

struct AVIMainHeader {
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

struct RECT {
  uint32_t dwWidth;
  uint32_t dwHeight;
};

struct AVIStreamHeader {
  FOURCC fccType;
  FOURCC fccHandler;
  uint32_t dwFlags;
  uint16_t wPriority;
  uint16_t wLanguage;
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

struct BitmapInfoHeader {
  uint32_t biSize;
  uint64_t biWidth;
  uint64_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint64_t biXPelsPerMeter;
  uint64_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
};

struct WAVFormatX {
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
  uint16_t cbSize;
};

struct WAVFormat {
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
};

enum StreamContentType { Audio, Video };

enum ParseObjectType { AVIList, AVIChunk, AVIStreamData };

enum ParseState {
  ParseHeader,
  ParseHdrl,
  ParseAvih,
  ParseStrl,
  SubChunkContinue,
  SubChunk,
  ParseRec,
  ParseStrf,
  AfterStrf,
  ParseMovi,
  ParseIgnore,
};

/// @brief Represents a LIST or a CHUNK
class ParseObject {
 public:
  void set(Str id, int size, ParseObjectType type) {
    set(id.c_str(), size, type);
  }

  void set(const char *id, int size, ParseObjectType type) {
    object_type = type;
    data_size = size;
    // save FOURCC
    if (id != nullptr) {
      memcpy(chunk_id, id, 4);
      chunk_id[5] = 0;
    }
    // save data
    if (type == AVIChunk && size > 0) {
      data_buffer.resize(size);
    }
    open = size;
  }
  const char *id() { return chunk_id; }
  uint8_t *data() { return data_buffer.data(); }
  int size() { return data_size; }
  int available() { return data_buffer.available(); }
  void consume(int len) { data_buffer.consume(len); }

  ParseObjectType type() { return object_type; }
  bool isValid() {
    switch (object_type) {
      case AVIStreamData:
        return isAudio() || isVideo();
      case AVIChunk:
        return data_buffer.size() > 0;
      case AVIList:
        return true;
    }
    return false;
  }

  // for Chunk
  AVIMainHeader *asAVIMainHeader() { return (AVIMainHeader *)data(); }
  AVIStreamHeader *asAVIStreamHeader() { return (AVIStreamHeader *)data(); }
  WAVFormatX *asAudioFormat() { return (WAVFormatX *)data(); }
  BitmapInfoHeader *asVideoFormat() { return (BitmapInfoHeader *)data(); }

  int open;

  // for AVIStreamData
  int streamNumber() {
    return object_type == AVIStreamData ? chunk_id[1] << 8 + chunk_id[0] : 0;
  }
  bool isAudio() {
    return object_type == AVIStreamData
               ? chunk_id[3] == 'w' && chunk_id[4] == 'b'
               : false;
  }
  bool isVideoUncompressed() {
    return object_type == AVIStreamData
               ? chunk_id[3] == 'd' && chunk_id[4] == 'b'
               : false;
  }
  bool isVideoCompressed() {
    return object_type == AVIStreamData
               ? chunk_id[3] == 'd' && chunk_id[4] == 'c'
               : false;
  }
  bool isVideo() { return isVideoCompressed() || isVideoUncompressed(); }

 protected:
  ParseBuffer data_buffer;
  char chunk_id[5] = {};
  int data_size;
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
  ContainerRIFF(int bufferSize = 1024) { parse_buffer.resize(bufferSize); }

  void begin() {
    parse_state = ParseHeader;
    header_is_avi = false;
    is_parsing_active = true;
  }

  virtual void setOutputStream(Print &out_stream) {
    p_print_audio = &out_stream;
  }

  virtual void setOutputVideoStream(VideoOutput &out_stream) {
    p_print_video = &out_stream;
  }

  virtual size_t write(const void *data, size_t length) {
    LOGD("write: %d", length);
    int result = parse_buffer.writeArray((uint8_t *)data, length);
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
  BitmapInfoHeader videoInfo() { return video_info; };
  WAVFormatX audioInfoExt() { return audio_info; }

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
  Vector<StreamContentType> content_types;
  ParseObject current_stream_data;
  Print *p_print_audio = nullptr;
  VideoOutput *p_print_video = nullptr;
  long open_subchunk_len=0;
  long header_file_size=0;

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
      case ParseHeader: {
        LOGD("ParseHeader");
        result = parseHeader();
        if (result) parse_state = ParseHdrl;
      } break;

      case ParseHdrl: {
        LOGD("ParseHdrl");
        ParseObject hdrl = parseList("hdrl");
        result = hdrl.isValid();
        if (result) {
          parse_state = ParseAvih;
        }
      } break;

      case ParseAvih: {
        LOGD("ParseAvih");
        ParseObject avih = parseChunk("avih");
        result = avih.isValid();
        if (result) {
          main_header = *(avih.asAVIMainHeader());
          parse_state = ParseStrl;
        }
      } break;

      case ParseStrl: {
        LOGD("ParseStrl");
        ParseObject strl = parseList("strl");
        ParseObject strh = parseChunk("strh");
        current_stream_header = *(strh.asAVIStreamHeader());
        if (isCurrentStreamAudio()) {
          stream_header_audio = current_stream_header;
        } else if (isCurrentStreamVideo()) {
          stream_header_video = current_stream_header;
        }
        parse_state = ParseStrf;
      } break;

      case ParseStrf: {
        LOGD("ParseStrf");
        ParseObject strf = parseChunk("strf");
        if (isCurrentStreamAudio()) {
          audio_info = *(strf.asAudioFormat());
          content_types.push_back(Audio);
        } else if (isCurrentStreamVideo()) {
          video_info = *(strf.asVideoFormat());
          content_types.push_back(Video);
        } else {
          result = false;
        }
        parse_state = AfterStrf;
      } break;

      case AfterStrf: {
        LOGD("AfterStrf");
        // ignore all data until we find a new List
        int pos = Str((const char*)parse_buffer.data()).indexOf("LIST");
        if (pos > 0) {
          consume(pos);
          ParseObject tmp = tryParseList();
          if (Str(tmp.id()).equals("strl")) {
            parse_state = ParseStrl;
          } else if (Str(tmp.id()).equals("movi")) {
            parse_state = ParseMovi;
          }
        } else {
          // no valid data, so throw it away, we keep the last 4 digits in case
          // if it contains the beginning of a LIST
          parse_buffer.consume(parse_buffer.available() - 4);
        }
      } break;

      case ParseMovi: {
        LOGD("ParseMovi");
        ParseObject hdrl = tryParseList();
        if (Str(hdrl.id()).equals("rec")) {
          parse_state = ParseRec;
        } else {
          parse_state = SubChunk;
        }
      } break;

      case ParseRec: {
        LOGD("ParseRec");
        ParseObject strl = parseList("rec");
        parse_state = SubChunk;
      } break;

      case SubChunk: {
        LOGD("SubChunk");
        ParseObject hdrl = parseAVIStreamData();
        current_stream_data = hdrl;
        parse_state = SubChunkContinue;
        open_subchunk_len = current_stream_data.size();
        if (current_stream_data.isVideo()){
          p_print_video->beginFrame(hdrl.size());
        }

      } break;

      case SubChunkContinue: {
        LOGD("SubChunkContinue");
        writeData();
        if (open_subchunk_len == 0) {
          if (current_stream_data.isVideo()) p_print_video->endFrame();
          if (tryParseChunk("idx").isValid()) {
            parse_state = ParseIgnore;
          } else if (tryParseList("rec").isValid()) {
            parse_state = ParseRec;
          } else {
            parse_state = SubChunk;
          }
        }
      } break;

      case ParseIgnore: {
        LOGD("ParseIgnore");
        parse_buffer.clear();
      } break;

      default:
        result = false;
        break;
    }
    return result;
  }

  void writeData() {
    long to_write = min((long)parse_buffer.available(), open_subchunk_len);
    if (current_stream_data.isAudio()) {
      p_print_audio->write(parse_buffer.data(), to_write);
      open_subchunk_len -= to_write;
      consume(to_write);
    } else if (current_stream_data.isVideo()) {
      p_print_video->write(parse_buffer.data(), to_write);
      open_subchunk_len -= to_write;
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

  /// We parse a chunk and  provide the FOURCC id and size: No content data is
  /// stored
  ParseObject tryParseChunk() {
    ParseObject result;
    result.set(getStr(0, 4), 0, AVIChunk);
    return result;
  }

  /// We try to parse the indicated chunk and determine the size: No content
  /// data is stored
  ParseObject tryParseChunk(const char *id) {
    ParseObject result;
    if (getStr(0, 4).equals(id)) {
      result.set(id, 0, AVIChunk);
    }
    return result;
  }

  ParseObject tryParseList(const char* id) {
    ParseObject result;
    Str& list_id = getStr(8, 4);
    if (list_id.equals(id) && getStr(0, 3).equals("LIST")) {
      result.set(getStr(8, 4), getInt(4), AVIList);
    }
    return result;
  }


  /// We try to parse the actual state for any list
  ParseObject tryParseList() {
    ParseObject result;
    if (getStr(0, 3).equals("LIST")) {
      result.set(getStr(8, 4), getInt(4), AVIList);
    }
    return result;
  }

  /// We load the indicated chunk from the current data
  ParseObject parseChunk(const char *id) {
    ParseObject result;
    int chunk_size = getInt(4);
    if (getStr(0, 4).equals(id) && parse_buffer.size() >= chunk_size) {
      result.set(id, chunk_size, AVIChunk);
      consume(8);
    }
    return result;
  }

  /// We load the indicated list from the current data
  ParseObject parseList(const char *id) {
    ParseObject result;
    if (getStr(0, 4).equals("LIST") && getStr(8, 4).equals(id)) {
      int size = getInt(4);
      result.set(id, size, AVIList);
      consume(12);
    }
    return result;
  }

  ParseObject parseAVIStreamData() {
    ParseObject result;
    int size = getInt(4);
    result.set(getStr(0, 4), size, AVIStreamData);
    if (result.isValid()) {
      consume(4);
    }
    return result;
  }

  /// Provides the string at the indicated byte offset with the indicated length
  Str &getStr(int offset, int len) {
    static StrExt str;
    const char* copyFrom = (const char*)parse_buffer.data();
    str.substring(copyFrom, offset, len);
    return str;
  }

  /// Provides the int32 at the indicated byte offset
  int32_t getInt(int offset) { return (int32_t)(parse_buffer.data() + offset); }

  /// We remove the indicated bytes from the beginning of the buffer
  void consume(int len) { parse_buffer.consume(len); }
};

}  // namespace audio_tools