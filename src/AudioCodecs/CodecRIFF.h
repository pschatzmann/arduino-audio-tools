#pragma once
#include "AudioTools/Buffers.h"
#include "stdint.h"

namespace audio_tools {

class ParseBuffer : public SingleBuffer {
  void consume(int size) {}
};

struct AVIStreamHeader {
  FOURCC fccType;
  FOURCC fccHandler;
  DWORD dwFlags;
  WORD wPriority;
  WORD wLanguage;
  DWORD dwInitialFrames;
  DWORD dwScale;
  DWORD dwRate;
  DWORD dwStart;
  DWORD dwLength;
  DWORD dwSuggestedBufferSize;
  DWORD dwQuality;
  DWORD dwSampleSize;
  RECT rcFrame;
};

typedef struct BITMAPINFOHEADER {
  DWORD biSize;
  LONG biWidth;
  LONG biHeight;
  WORD biPlanes;
  WORD biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG biXPelsPerMeter;
  LONG biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
};

struct WAVEFORMATEX {
  WORD wFormatTag;
  WORD nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD nBlockAlign;
  WORD wBitsPerSample;
  WORD cbSize;
};

struct WAVEFORMAT {
  WORD wFormatTag;
  WORD nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD nBlockAlign;
};

struct WAVEFORMATEXTENSIBLE {
  WAVEFORMATEX Format;
  union {
    WORD wValidBitsPerSample;
    WORD wSamplesPerBlock;
    WORD wReserved;
  } Samples;
  DWORD dwChannelMask;
  GUID SubFormat;
};

class List {
  List(const char *id, int size) {
    list_id.set(id, 4);
    list_size = size;
  }
  Str *id() { return list_id; }
  int size() { return list_size; }
  int open() { return open }
  void consume(int len){open -= len};

 protected:
  StrExt list_id(5);
  int list_size;

}

class Chunk {
  Chunk() = default;
  Chunk(const char *id, int size) {
    data_buffer.resize(size);
    chunk_size = size;
    chunk_id.set(id, 4);
  }
  Str *id() { return Strchunk_id; }
  uint8_t *data() { return data_buffer().data(); }
  int size() { return chunk_size; }
  int available() { return data_buffer.available(); }
  int open() { return chunk_size - data_buffer.available(); }
  void consume(int len) { data_buffer.consum(); }

  AVIStreamHeader *asAVIStreamHeader() { return (AVIStreamHeader *)data(); }
  bool isValid() { return data_buffer.size() > 0; }

 protected:
  ParseBuffer data_buffer{0};
  StrExt chunk_id(5);
  int chunk_size;
};

/**
 * Decoder which can be fed with smake chunks of data. The minimum length must
 * be bigger then the header size!
 */

class RIFFDecoder : public AudioDecoder {
 public:
  RIFFDecoder(int bufferSize = 8192) {}

  ~RIFFDecoder() {}

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
      if (!parse()) {
        LOGI("Parse Error");
        parse_buffer.clear();
        result = length;
        is_parsing_active;
      } else {
        result = length;
      }
    }
    return result;
  }

 protected:
  enum ParseState { ParseHader, ParseMovi };
  using FOURCC = uint8_t[4];
  int32_t header_file_size;
  bool header_is_avi = false;
  bool is_parsing_active = true;
  ParseState parse_state = ParseHader;
  ParseBuffer parse_buffer;
  Print *p_print_audio = nullptr;
  Print *p_print_video = nullptr;
  AVIStreamHeader header;
  List hdrl;

  // we return true if at least one parse step was successful
  bool parse() {
    bool result = false;
    switch (parse_state) {
      case ParseHader:
        if (parseHeader()) {
          parse_state = ParseHdrl;
          result = true;
        } else {
          LOGE("Not an AVI!")
        }
        break;
      case ParseHdrl:
        hdrl = parseList("hdrl");
        parse_state = ParseAvih;
        break;

      case ParseAvih:
        header = parseChunk("avih").asAVIStreamHeader();
        parse_state = ParseStrl;
        break;

      case ParseStrl:
        int len_strl = parseListStart("strl");
        Chunk strh = parseChunk("strh");
        parse_state = ParseStrf;

      case ParseStrf:
        Chunk strf = parseChunk("strf");
        endList() break;

      case ParseMovi:
        break;
    }
  }

  Chunk parseChunk(const char *id) {
    Chunk result;
    if (getStr(0, 4).equals("id")) {
      int chunk_size = getInt(4);
      resut = Chunk(id, chunk_size);
      consume(8);
    }
    return result;
  }

  // 'RIFF' fileSize fileType (data)
  bool parseHeader() {
    int headerSize = 12;
    if (getStr(0, 4).equals("RIFF")) {
      header_file_size = getInt(4);
      header_is_avi = getStr(8, 4).equals("AVI ");
      consume(12);
    }
    return header_is_avi;
  }

  // 'LIST' ( listType ( listData ) )
  int parseListStart(const char *type) {
    int result = 0;
    if (getStr(0, 4).equals("LIST")) {
      int list_size = getInt(4);
      result = getStr(8, 4).equals(type);
      consume(8);
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