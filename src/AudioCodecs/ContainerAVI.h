#pragma once
#include <string.h>
#include "AudioCodecs/AudioCodecsBase.h"
#include "AudioBasic/StrExt.h"
#include "AudioCodecs/AudioFormat.h"
#include "Video/Video.h"
#include "AudioTools/Buffers.h"

#define LIST_HEADER_SIZE 12
#define CHUNK_HEADER_SIZE 8

namespace audio_tools {

/**
 * @brief We try to keep the necessary buffer for parsing as small as possible,
 * The data() method provides the start of the actual data and with consume
 * we remove the processed data from the buffer to make space again.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ParseBuffer {
public:
  size_t writeArray(uint8_t *data, size_t len) {
    int to_write = min(availableToWrite(), (size_t)len);
    memmove(vector.data() + available_byte_count, data, to_write);
    available_byte_count += to_write;
    return to_write;
  }
  void consume(int size) {
    memmove(vector.data(), &vector[size], available_byte_count - size);
    available_byte_count -= size;
  }
  void resize(int size) { vector.resize(size + 4); }

  uint8_t *data() { return vector.data(); }

  size_t availableToWrite() { return size() - available_byte_count; }

  size_t available() { return available_byte_count; }

  void clear() {
    available_byte_count = 0;
    memset(vector.data(), 0, vector.size());
  }

  bool isEmpty() { return available_byte_count == 0; }

  size_t size() { return vector.size(); }

  long indexOf(const char *str) {
    uint8_t *ptr = (uint8_t *)memmem(vector.data(), available_byte_count, str,
                                     strlen(str));
    return ptr == nullptr ? -1l : ptr - vector.data();
  }

protected:
  Vector<uint8_t> vector{0};
  size_t available_byte_count = 0;
};

using FOURCC = char[4];

struct AVIMainHeader {
  //  FOURCC fcc;
  //  uint32_t cb;
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
  AudioFormat wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
  uint16_t cbSize;
};

// struct WAVFormat {
//   uint16_t wFormatTag;
//   uint16_t nChannels;
//   uint32_t nSamplesPerSec;
//   uint32_t nAvgBytesPerSec;
//   uint16_t nBlockAlign;
// };

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
/***
 * @brief Represents a LIST or a CHUNK: The ParseObject represents the
 * current parsing result. We just keep position information and ids
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ParseObject {
public:
  void set(size_t currentPos, Str id, size_t size, ParseObjectType type) {
    set(currentPos, id.c_str(), size, type);
  }

  void set(size_t currentPos, const char *id, size_t size,
           ParseObjectType type) {
    object_type = type;
    data_size = size;
    start_pos = currentPos;
    // allign on word
    if (size % 2 != 0) {
      data_size++;
    }
    end_pos = currentPos + data_size + 4;
    // save FOURCC
    if (id != nullptr) {
      memcpy(chunk_id, id, 4);
      chunk_id[4] = 0;
    }
    open = data_size;
  }
  const char *id() { return chunk_id; }
  size_t size() { return data_size; }

  ParseObjectType type() { return object_type; }
  bool isValid() {
    switch (object_type) {
    case AVIStreamData:
      return isAudio() || isVideo();
    case AVIChunk:
      return open > 0;
    case AVIList:
      return true;
    }
    return false;
  }

  // for Chunk
  AVIMainHeader *asAVIMainHeader(void *ptr) { return (AVIMainHeader *)ptr; }
  AVIStreamHeader *asAVIStreamHeader(void *ptr) {
    return (AVIStreamHeader *)ptr;
  }
  WAVFormatX *asAVIAudioFormat(void *ptr) { return (WAVFormatX *)ptr; }
  BitmapInfoHeader *asAVIVideoFormat(void *ptr) {
    return (BitmapInfoHeader *)ptr;
  }

  size_t open;
  size_t end_pos;
  size_t start_pos;
  size_t data_size;

  // for AVIStreamData
  int streamNumber() {
    return object_type == AVIStreamData ? (chunk_id[1] << 8) | chunk_id[0] : 0;
  }
  bool isAudio() {
    return object_type == AVIStreamData
               ? chunk_id[2] == 'w' && chunk_id[3] == 'b'
               : false;
  }
  bool isVideoUncompressed() {
    return object_type == AVIStreamData
               ? chunk_id[2] == 'd' && chunk_id[3] == 'b'
               : false;
  }
  bool isVideoCompressed() {
    return object_type == AVIStreamData
               ? chunk_id[2] == 'd' && chunk_id[3] == 'c'
               : false;
  }
  bool isVideo() { return isVideoCompressed() || isVideoUncompressed(); }

protected:
  // ParseBuffer data_buffer;
  char chunk_id[5] = {};
  ParseObjectType object_type;
};

/**
 * @brief AVI Container Decoder which can be fed with small chunks of data. The
 * minimum length must be bigger then the header size! The file structure is
 * documented at
 * https://learn.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AVIDecoder : public ContainerDecoder {
public:
  AVIDecoder(int bufferSize = 1024) {
    parse_buffer.resize(bufferSize);
    p_decoder = &copy_decoder;
    p_output_audio = new EncodedAudioOutput(&copy_decoder);
  }

  AVIDecoder(AudioDecoder *audioDecoder, VideoOutput *videoOut = nullptr,
             int bufferSize = 1024) {
    parse_buffer.resize(bufferSize);
    p_decoder = audioDecoder;
    p_output_audio = new EncodedAudioOutput(audioDecoder);
    if (videoOut != nullptr) {
      setOutputVideoStream(*videoOut);
    }
  }

  ~AVIDecoder() {
    if (p_output_audio != nullptr)
      delete p_output_audio;
  }

  bool begin() override {
    parse_state = ParseHeader;
    header_is_avi = false;
    is_parsing_active = true;
    current_pos = 0;
    header_is_avi = false;
    stream_header_idx = -1;
    is_metadata_ready = false;
    return true;
  }

  /// Defines the audio output stream - usually called by EncodedAudioStream
  virtual void setOutput(Print &out_stream) override {
    // p_output_audio = &out_stream;
    p_output_audio->setOutput(&out_stream);
  }

  ///
  void setMute(bool mute) { is_mute = mute; }

  virtual void setOutputVideoStream(VideoOutput &out_stream) {
    p_output_video = &out_stream;
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("write: %d", (int)len);
    int result = parse_buffer.writeArray((uint8_t *)data, len);
    if (is_parsing_active) {
      // we expect the first parse to succeed
      if (parse()) {
        // if so we process the parse_buffer
        while (parse_buffer.available() > 4) {
          if (!parse())
            break;
        }
      } else {
        LOGD("Parse Error");
        parse_buffer.clear();
        result = len;
        is_parsing_active = false;
      }
    }
    return result;
  }

  operator bool() override { return is_parsing_active; }

  void end() override { is_parsing_active = false; };

  /// Provides the information from the main header chunk
  AVIMainHeader mainHeader() { return main_header; }

  /// Provides the information from the stream header chunks
  AVIStreamHeader streamHeader(int idx) { return stream_header[idx]; }

  /// Provides the video information
  BitmapInfoHeader aviVideoInfo() { return video_info; };

  const char *videoFormat() { return video_format; }

  /// Provides the audio information
  WAVFormatX aviAudioInfo() { return audio_info; }

  /// Provides the  audio_info.wFormatTag
  AudioFormat audioFormat() { return audio_info.wFormatTag; }

  /// Returns true if all metadata has been parsed and is available
  bool isMetadataReady() { return is_metadata_ready; }
  /// Register a validation callback which is called after parsing just before
  /// playing the audio
  void setValidationCallback(bool (*cb)(AVIDecoder &avi)) {
    validation_cb = cb;
  }

  /// Provide the length of the video in seconds
  int videoSeconds() { return video_seconds; }

  /// Replace the synchronization logic with your implementation
  void setVideoAudioSync(VideoAudioSync *yourSync) { p_synch = yourSync; }

protected:
  bool header_is_avi = false;
  bool is_parsing_active = true;
  ParseState parse_state = ParseHeader;
  ParseBuffer parse_buffer;
  AVIMainHeader main_header;
  int stream_header_idx = -1;
  Vector<AVIStreamHeader> stream_header;
  BitmapInfoHeader video_info;
  WAVFormatX audio_info;
  Vector<StreamContentType> content_types;
  Stack<ParseObject> object_stack;
  ParseObject current_stream_data;
  EncodedAudioOutput *p_output_audio = nullptr;
  VideoOutput *p_output_video = nullptr;
  long open_subchunk_len = 0;
  long current_pos = 0;
  long movi_end_pos = 0;
  StrExt spaces;
  StrExt str;
  char video_format[5] = {0};
  bool is_metadata_ready = false;
  bool (*validation_cb)(AVIDecoder &avi) = nullptr;
  bool is_mute = false;
  CopyDecoder copy_decoder;
  AudioDecoder *p_decoder = nullptr;
  int video_seconds = 0;
  VideoAudioSync defaultSynch;
  VideoAudioSync *p_synch = &defaultSynch;

  bool isCurrentStreamAudio() {
    return strncmp(stream_header[stream_header_idx].fccType, "auds", 4) == 0;
  }

  bool isCurrentStreamVideo() {
    return strncmp(stream_header[stream_header_idx].fccType, "vids", 4) == 0;
  }

  // we return true if at least one parse step was successful
  bool parse() {
    bool result = true;
    switch (parse_state) {
    case ParseHeader: {
      result = parseHeader();
      if (result)
        parse_state = ParseHdrl;
    } break;

    case ParseHdrl: {
      ParseObject hdrl = parseList("hdrl");
      result = hdrl.isValid();
      if (result) {
        parse_state = ParseAvih;
      }
    } break;

    case ParseAvih: {
      ParseObject avih = parseChunk("avih");
      result = avih.isValid();
      if (result) {
        main_header = *(avih.asAVIMainHeader(parse_buffer.data()));
        stream_header.resize(main_header.dwStreams);
        consume(avih.size());
        parse_state = ParseStrl;
      }
    } break;

    case ParseStrl: {
      ParseObject strl = parseList("strl");
      ParseObject strh = parseChunk("strh");
      stream_header[++stream_header_idx] =
          *(strh.asAVIStreamHeader(parse_buffer.data()));
      consume(strh.size());
      parse_state = ParseStrf;
    } break;

    case ParseStrf: {
      ParseObject strf = parseChunk("strf");
      if (isCurrentStreamAudio()) {
        audio_info = *(strf.asAVIAudioFormat(parse_buffer.data()));
        setupAudioInfo();
        LOGI("audioFormat: %d (%x)", (int)audioFormat(),(int)audioFormat());
        content_types.push_back(Audio);
        consume(strf.size());
      } else if (isCurrentStreamVideo()) {
        video_info = *(strf.asAVIVideoFormat(parse_buffer.data()));
        setupVideoInfo();
        LOGI("videoFormat: %s", videoFormat());
        content_types.push_back(Video);
        video_format[4] = 0;
        consume(strf.size());
      } else {
        result = false;
      }
      parse_state = AfterStrf;
    } break;

    case AfterStrf: {
      // ignore all data until we find a new List
      int pos = parse_buffer.indexOf("LIST");
      if (pos >= 0) {
        consume(pos);
        ParseObject tmp = tryParseList();
        if (Str(tmp.id()).equals("strl")) {
          parse_state = ParseStrl;
        } else if (Str(tmp.id()).equals("movi")) {
          parse_state = ParseMovi;
        } else {
          // e.g. ignore info
          consume(tmp.size() + LIST_HEADER_SIZE);
        }
      } else {
        // no valid data, so throw it away, we keep the last 4 digits in case
        // if it contains the beginning of a LIST
        cleanupStack();
        consume(parse_buffer.available() - 4);
      }
    } break;

    case ParseMovi: {
      ParseObject movi = tryParseList();
      if (Str(movi.id()).equals("movi")) {
        consume(LIST_HEADER_SIZE);
        is_metadata_ready = true;
        if (validation_cb)
          is_parsing_active = (validation_cb(*this));
        processStack(movi);
        movi_end_pos = movi.end_pos;
        parse_state = SubChunk;
        // trigger new write
        result = false;
      }
    } break;

    case SubChunk: {
      // rec is optinal
      ParseObject hdrl = tryParseList();
      if (Str(hdrl.id()).equals("rec")) {
        consume(CHUNK_HEADER_SIZE);
        processStack(hdrl);
      }

      current_stream_data = parseAVIStreamData();
      parse_state = SubChunkContinue;
      open_subchunk_len = current_stream_data.open;
      if (current_stream_data.isVideo()) {
        LOGI("video:[%d]->[%d]", (int)current_stream_data.start_pos,
             (int)current_stream_data.end_pos);
        if (p_output_video != nullptr)
          p_output_video->beginFrame(current_stream_data.open);
      } else if (current_stream_data.isAudio()) {
        LOGI("audio:[%d]->[%d]", (int)current_stream_data.start_pos,
             (int)current_stream_data.end_pos);
      } else {
        LOGW("unknown subchunk at %d", (int)current_pos);
      }

    } break;

    case SubChunkContinue: {
      writeData();
      if (open_subchunk_len == 0) {
        if (current_stream_data.isVideo() && p_output_video != nullptr) {
          uint32_t time_used_ms = p_output_video->endFrame();
          p_synch->delayVideoFrame(main_header.dwMicroSecPerFrame, time_used_ms);
        }
        if (tryParseChunk("idx").isValid()) {
          parse_state = ParseIgnore;
        } else if (tryParseList("rec").isValid()) {
          parse_state = ParseRec;
        } else {
          if (current_pos >= movi_end_pos) {
            parse_state = ParseIgnore;
          } else {
            parse_state = SubChunk;
          }
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

  void setupAudioInfo() {
    info.channels = audio_info.nChannels;
    info.bits_per_sample = audio_info.wBitsPerSample;
    info.sample_rate = audio_info.nSamplesPerSec;
    info.logInfo();
    // adjust the audio info if necessary
    if (p_decoder != nullptr) {
      p_decoder->setAudioInfo(info);
      info = p_decoder->audioInfo();
    }
    notifyAudioChange(info);
  }

  void setupVideoInfo() {
    memcpy(video_format, stream_header[stream_header_idx].fccHandler, 4);
    AVIStreamHeader *vh = &stream_header[stream_header_idx];
    if (vh->dwScale <= 0) {
      vh->dwScale = 1;
    }
    int rate = vh->dwRate / vh->dwScale;
    video_seconds = rate <= 0 ? 0 : vh->dwLength / rate;
    LOGI("videoSeconds: %d seconds", video_seconds);
  }

  void writeData() {
    long to_write = min((long)parse_buffer.available(), open_subchunk_len);
    if (current_stream_data.isAudio()) {
      LOGD("audio %d", (int)to_write);
      if (!is_mute){
        p_synch->writeAudio(p_output_audio, parse_buffer.data(), to_write);
      }
      open_subchunk_len -= to_write;
      cleanupStack();
      consume(to_write);
    } else if (current_stream_data.isVideo()) {
      LOGD("video %d", (int)to_write);
      if (p_output_video != nullptr)
        p_output_video->write(parse_buffer.data(), to_write);
      open_subchunk_len -= to_write;
      cleanupStack();
      consume(to_write);
    }
  }

  // 'RIFF' fileSize fileType (data)
  bool parseHeader() {
    bool header_is_avi = false;
    int headerSize = 12;
    if (getStr(0, 4).equals("RIFF")) {
      ParseObject result;
      uint32_t header_file_size = getInt(4);
      header_is_avi = getStr(8, 4).equals("AVI ");
      result.set(current_pos, "AVI ", header_file_size, AVIChunk);
      processStack(result);
      consume(headerSize);

    } else {
      LOGE("parseHeader");
    }
    return header_is_avi;
  }

  /// We parse a chunk and  provide the FOURCC id and size: No content data is
  /// stored
  ParseObject tryParseChunk() {
    ParseObject result;
    result.set(current_pos, getStr(0, 4), 0, AVIChunk);
    return result;
  }

  /// We try to parse the indicated chunk and determine the size: No content
  /// data is stored
  ParseObject tryParseChunk(const char *id) {
    ParseObject result;
    if (getStr(0, 4).equals(id)) {
      result.set(current_pos, id, 0, AVIChunk);
    }
    return result;
  }

  ParseObject tryParseList(const char *id) {
    ParseObject result;
    Str &list_id = getStr(8, 4);
    if (list_id.equals(id) && getStr(0, 3).equals("LIST")) {
      result.set(current_pos, getStr(8, 4), getInt(4), AVIList);
    }
    return result;
  }

  /// We try to parse the actual state for any list
  ParseObject tryParseList() {
    ParseObject result;
    if (getStr(0, 4).equals("LIST")) {
      result.set(current_pos, getStr(8, 4), getInt(4), AVIList);
    }
    return result;
  }

  /// We load the indicated chunk from the current data
  ParseObject parseChunk(const char *id) {
    ParseObject result;
    int chunk_size = getInt(4);
    if (getStr(0, 4).equals(id) && parse_buffer.size() >= chunk_size) {
      result.set(current_pos, id, chunk_size, AVIChunk);
      processStack(result);
      consume(CHUNK_HEADER_SIZE);
    }
    return result;
  }

  /// We load the indicated list from the current data
  ParseObject parseList(const char *id) {
    ParseObject result;
    if (getStr(0, 4).equals("LIST") && getStr(8, 4).equals(id)) {
      int size = getInt(4);
      result.set(current_pos, id, size, AVIList);
      processStack(result);
      consume(LIST_HEADER_SIZE);
    }
    return result;
  }

  ParseObject parseAVIStreamData() {
    ParseObject result;
    int size = getInt(4);
    result.set(current_pos, getStr(0, 4), size, AVIStreamData);
    if (result.isValid()) {
      processStack(result);
      consume(8);
    }
    return result;
  }

  void processStack(ParseObject &result) {
    cleanupStack();
    object_stack.push(result);
    spaces.setChars(' ', object_stack.size());
    LOGD("%s - %s (%d-%d) size:%d", spaces.c_str(), result.id(),
         (int)result.start_pos, (int)result.end_pos, (int)result.data_size);
  }

  void cleanupStack() {
    ParseObject current;
    // make sure that we remove the object from the stack of we past the end
    object_stack.peek(current);
    while (current.end_pos <= current_pos) {
      object_stack.pop(current);
      object_stack.peek(current);
    }
  }

  /// Provides the string at the indicated byte offset with the indicated length
  Str &getStr(int offset, int len) {
    str.setCapacity(len + 1);
    const char *data = (const char *)parse_buffer.data();
    str.copyFrom((data + offset), len, 5);

    return str;
  }

  /// Provides the int32 at the indicated byte offset
  uint32_t getInt(int offset) {
    uint32_t *result = (uint32_t *)(parse_buffer.data() + offset);
    return *result;
  }

  /// We remove the processed bytes from the beginning of the buffer
  void consume(int len) {
    current_pos += len;
    parse_buffer.consume(len);
  }
};

} // namespace audio_tools