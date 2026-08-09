#pragma once
#include <string.h>
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioTools/AudioCodecs/AudioFormat.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/ContainerCommon.h"
#include "AudioTools/Video/Video.h"
#include "AudioTools/CoreAudio/Buffers.h"

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
    // use pointer arithmetic (not the bounds-checked operator[]): when size
    // == available_byte_count (buffer fully drained, common for chunks at
    // least as big as the buffer capacity, e.g. large raw video frames),
    // vector.data() + size legally points one-past-the-end and the memmove
    // length is 0, which operator[] would reject even though it's a no-op
    memmove(vector.data(), vector.data() + size, available_byte_count - size);
    available_byte_count -= size;
  }
  bool resize(size_t size) { 
    vector.resize(size + 4); 
    return vector.data() != nullptr;
  }

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

/// @brief Four-character code identifier for AVI format
/// @ingroup codecs
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

// field widths mirror the on-disk 40-byte BITMAPINFOHEADER exactly (all
// DWORD/uint32_t, except the two WORD/uint16_t fields) - do not widen these,
// the decoder reinterpret_casts raw file bytes directly onto this struct
struct BitmapInfoHeader {
  uint32_t biSize;
  uint32_t biWidth;
  uint32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint32_t biXPelsPerMeter;
  uint32_t biYPelsPerMeter;
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

/**
 * @brief Represents a LIST or a CHUNK: The ParseObject represents the
 * current parsing result. We just keep position information and ids
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ParseObject {
public:
  void set(size_t currentPos, StrView id, size_t size, ParseObjectType type) {
    set(currentPos, id.c_str(), size, type);
  }

  void set(size_t currentPos, const char *id, size_t size,
           ParseObjectType type) {
    object_type = type;
    data_size = size;
    declared_size = size;
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
  /// Original (unpadded) size as declared in the file - for AVIStreamData
  /// this is the real payload length, excluding the RIFF word-alignment pad
  /// byte that size()/open may include when the declared size is odd.
  size_t declaredSize() { return declared_size; }

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
  BitmapInfoHeader *asVideoFormat(void *ptr) {
    return (BitmapInfoHeader *)ptr;
  }

  size_t open;
  size_t end_pos;
  size_t start_pos;
  size_t data_size;
  size_t declared_size;

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

class DemuxerAVI : public Demuxer {
public:
  /// @param bufferSize internal parse buffer size.
  ///
  /// This class only demuxes - it does not decode audio itself. Point
  /// setOutputAudio() at an EncodedAudioStream (wrapping whatever
  /// AudioDecoder matches the AVI's audio format) if you need the audio
  /// track decoded; the raw payload is written through as-is otherwise.
  /// Point setOutputVideo() at a Print (e.g. a VideoOutput) to receive the
  /// demuxed video track; leave unset to ignore video.
  DemuxerAVI(int bufferSize = 1024) {
    parse_buffer.resize(bufferSize);
  }

  const char *mime() override { return "video/avi"; }

  /// Overrides the automatic decision of whether a synthetic WAV header
  /// (matching the parsed 'strf' audio format) is sent to the audio output
  /// before any audio payload. By default this is decided automatically
  /// from the parsed format: on whenever isWavFormat() of the parsed 'strf'
  /// format is true (PCM/ADPCM, i.e. audio that needs to go through a
  /// WAVDecoder), off
  /// otherwise (e.g. compressed formats with their own decoder that don't
  /// expect a WAV header). Call this to force it either way instead.
  void setSendWavHeader(bool flag) override {
    send_wav_header = flag;
    send_wav_header_explicit = true;
  }

  bool begin() override {
    parse_state = ParseHeader;
    header_is_avi = false;
    is_parsing_active = true;
    current_pos = 0;
    header_is_avi = false;
    stream_header_idx = -1;
    is_metadata_ready = false;
    is_wav_header_sent = false;
    riff_file_size = 0;
    return true;
  }

  /// Defines the audio output stream - e.g. an EncodedAudioStream wrapping
  /// an AudioDecoder that matches the AVI's audio format, or any other
  /// Print if you want the raw payload as-is.
  void setOutputAudio(Print &out_stream) override {
    p_output_audio = &out_stream;
  }

  /// Satisfies the AudioWriter/AudioDecoder interface - needed so
  /// EncodedAudioOutput/EncodedAudioStream's polymorphic AudioDecoder*
  /// wiring (which calls setOutput() through that base class pointer) still
  /// reaches the audio output correctly; delegates to setOutputAudio().
  virtual void setOutput(Print &out_stream) override {
    setOutputAudio(out_stream);
  }

  ///
  void setMute(bool mute) { is_mute = mute; }

  virtual void setOutputVideo(Print &out_stream) override {
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
  AVIMainHeader aviMainHeader() { return main_header; }

  /// Provides the information from the stream header chunks
  AVIStreamHeader aviStreamHeader(int idx) { return stream_header[idx]; }

  /// Provides the video information
  BitmapInfoHeader aviVideoInfo() { return video_info; };

  const char *aviVideoFormat() { return video_format; }

  /// Provides the audio information
  WAVFormatX aviAudioInfo() { return audio_info; }

  /// Common video info (width/height/format/frame_size/total_file_size),
  /// analogous to audioInfo() - the same VideoInfo type is also provided
  /// by DemuxerMP4. format is decoded from the 'strf' biCompression field
  /// (the authoritative codec id; more reliable than the 'strh' handler
  /// text for third-party files, available separately via videoFormat());
  /// VideoFormat::UNKNOWN for any codec not covered by the enum.
  VideoInfo getVideoInfo() override {
    VideoInfo result;
    result.width = (uint16_t)video_info.biWidth;
    result.height = (uint16_t)video_info.biHeight;
    result.format =
        toVideoFormat(video_info.biCompression, video_info.biBitCount);
    result.frame_size =
        (uint32_t)videoFrameSizeBytes(result.format, result.width, result.height);
    result.total_file_size = riff_file_size;
    return result;
  }


  /// Common audio info (sample_rate/channels/bits_per_sample), extended
  /// with the parsed 'strf' format tag - the same AudioInfoFormat type is
  /// also provided by DemuxerMP4. Named getAudioInfo() rather than
  /// audioInfo() because AudioDecoder already declares a virtual
  /// AudioInfo audioInfo() (returning the plain, unextended type by
  /// value) - by-value virtual returns can't be covariantly widened in
  /// C++, so reusing that name for a subclass-returning version isn't
  /// possible (it would be a hard "invalid covariant return type" error,
  /// not silent hiding).
  AudioInfoFormat getAudioInfo() override {
    AudioInfoFormat result(info);
    result.format = audio_info.wFormatTag;
    return result;
  }

  /// Returns true if all metadata has been parsed and is available
  bool isMetadataReady() { return is_metadata_ready; }
  /// Register a validation callback which is called after parsing just before
  /// playing the audio
  void setValidationCallback(bool (*cb)(DemuxerAVI &avi)) {
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
  Print *p_output_audio = nullptr;
  Print *p_output_video = nullptr;
  long video_frame_start_ms = 0;
  long open_subchunk_len = 0;
  long current_pos = 0;
  long movi_end_pos = 0;
  Str spaces;
  Str str;
  char video_format[5] = {0};
  bool is_metadata_ready = false;
  bool (*validation_cb)(DemuxerAVI &avi) = nullptr;
  bool is_mute = false;
  bool send_wav_header = false;
  bool send_wav_header_explicit = false;
  bool is_wav_header_sent = false;
  /// Total file size (bytes) declared by the RIFF header, normalized from
  /// its chunk_size (= file_size - 8) field; 0 if not yet parsed or the
  /// declared size looks like an unbounded/streamed placeholder.
  uint32_t riff_file_size = 0;
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
        LOGI("audioFormat: %d (%x)", (int)audio_info.wFormatTag,
             (int)audio_info.wFormatTag);
        content_types.push_back(Audio);
        consume(strf.size());
      } else if (isCurrentStreamVideo()) {
        video_info = *(strf.asVideoFormat(parse_buffer.data()));
        setupVideoInfo();
        LOGI("videoFormat: %s", aviVideoFormat());
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
        if (StrView(tmp.id()).equals("strl")) {
          parse_state = ParseStrl;
        } else if (StrView(tmp.id()).equals("movi")) {
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
      if (StrView(movi.id()).equals("movi")) {
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
      if (StrView(hdrl.id()).equals("rec")) {
        consume(CHUNK_HEADER_SIZE);
        processStack(hdrl);
      }

      current_stream_data = parseAVIStreamData();
      parse_state = SubChunkContinue;
      open_subchunk_len = current_stream_data.open;
      if (current_stream_data.isVideo()) {
        LOGI("video:[%d]->[%d]", (int)current_stream_data.start_pos,
             (int)current_stream_data.end_pos);
        video_frame_start_ms = millis();
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
          p_output_video->flush();
          uint32_t time_used_ms = (uint32_t)(millis() - video_frame_start_ms);
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
    if (!send_wav_header_explicit)
      send_wav_header = isWavFormat(audio_info.wFormatTag);
    if (send_wav_header) sendWavHeader();
    notifyAudioChange(info);
  }

  /// Synthesizes a valid WAV header (via the shared WAVHeader writer - also
  /// correctly handles ADPCM's extra 'fmt ' extension + 'fact' chunk) from
  /// the parsed 'strf' audio_info and sends it to p_output_audio, once,
  /// before any real audio payload - so a WAVDecoder-based output can
  /// bootstrap itself from it. Length is written as streamed/unknown, since
  /// the AVI's audio track length is not known upfront while demuxing.
  void sendWavHeader() {
    if (is_wav_header_sent || p_output_audio == nullptr) return;
    WAVAudioInfo winfo(info);
    winfo.format = audio_info.wFormatTag;
    winfo.is_streamed = true;
    winfo.block_align = audio_info.nBlockAlign;
    winfo.byte_rate = audio_info.nAvgBytesPerSec;
    winfo.ext_adpcm_header = true;
    WAVHeader wav_header;
    wav_header.setAudioInfo(winfo);
    wav_header.writeHeader(p_output_audio);
    is_wav_header_sent = true;
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

  /// Maps a 'strf' biCompression value (+ biBitCount, to disambiguate
  /// BI_BITFIELDS) to the corresponding VideoFormat. Covers what
  /// MuxerAVI writes plus a few common real-world FOURCC aliases seen in
  /// third-party AVI files.
  static VideoFormat toVideoFormat(uint32_t biCompression,
                                         uint16_t biBitCount) {
    if (biCompression == 0) return VideoFormat::RAW;  // BI_RGB
    if (biCompression == 3)  // BI_BITFIELDS
      return biBitCount == 16 ? VideoFormat::RGB565 : VideoFormat::RAW;

    auto is = [biCompression](const char *fourcc) {
      return memcmp(&biCompression, fourcc, 4) == 0;
    };
    if (is("H264") || is("h264") || is("X264") || is("x264") ||
        is("avc1") || is("AVC1"))
      return VideoFormat::H264;
    if (is("MJPG") || is("mjpg"))
      return VideoFormat::MJPEG;
    if (is("FMP4") || is("XVID") || is("DIVX") || is("DX50") ||
        is("mp4v"))
      return VideoFormat::MPEG4;
    if (is("YUY2") || is("YUYV") || is("yuy2") || is("yuyv"))
      return VideoFormat::YUV422;
    if (is("I420") || is("IYUV"))
      return VideoFormat::I420;
    if (is("RGBP"))
      return VideoFormat::RGB565;
    return VideoFormat::UNKNOWN;
  }

  void writeData() {
    long to_write = min((long)parse_buffer.available(), open_subchunk_len);
    // open_subchunk_len spans the word-aligned chunk (current_stream_data
    // .size()), which for an odd declared size includes a trailing RIFF pad
    // byte that must be consumed from the buffer but not delivered as
    // payload to the audio/video sink - clip to the real, unpadded length.
    long already_written = (long)current_stream_data.size() - open_subchunk_len;
    long payload_left = (long)current_stream_data.declaredSize() - already_written;
    if (payload_left < 0) payload_left = 0;
    long payload_to_write = min(to_write, payload_left);

    if (current_stream_data.isAudio()) {
      LOGD("audio %d", (int)to_write);
      if (!is_mute && payload_to_write > 0 && p_output_audio != nullptr){
        p_synch->writeAudio(p_output_audio, parse_buffer.data(), payload_to_write);
      }
      open_subchunk_len -= to_write;
      cleanupStack();
      consume(to_write);
    } else if (current_stream_data.isVideo()) {
      LOGD("video %d", (int)to_write);
      if (p_output_video != nullptr && payload_to_write > 0)
        p_output_video->write(parse_buffer.data(), payload_to_write);
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
      // chunk_size = file_size - 8; treat a near-max value (unbounded/
      // streamed placeholder, mirroring WAVHeader's own convention) as
      // "unknown" rather than wrapping around when normalizing to file_size
      riff_file_size =
          header_file_size >= 0xFFFFFFF0 ? 0 : header_file_size + 8;
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
    StrView &list_id = getStr(8, 4);
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
  StrView &getStr(int offset, int len) {
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

/// @brief Configuration for the (single) video track written by MuxerAVI
/// @ingroup video
/**
 * @brief AVI Container Encoder: muxes an already-encoded video stream (e.g.
 * H.264 access units) and an optional PCM/compressed audio stream into a
 * RIFF/AVI container written to a Print (a local File to record, or e.g. a
 * network Client to publish a live stream to an HTTP/TCP client).
 *
 * This is a *streaming* writer: since the total number of frames/bytes is
 * usually not known upfront, the RIFF/movi sizes and stream dwLength fields
 * are written as "unknown" (0xFFFFFFFF) and no idx1 index is appended - the
 * same technique used by live IP-camera AVI/MJPEG streams. This is
 * sufficient for sequential playback (ffplay/VLC/mpv opening the stream, or
 * a file written for later playback) but the result is not seekable. All
 * structural sizes a parser needs while walking the file sequentially
 * (avih/strh/strf chunk sizes, hdrl LIST size) are written exactly.
 *
 * @note Classic AVI has no official H.264 standardization (unlike MP4/TS):
 * common players (VLC, ffplay, mpv) handle "H264-in-AVI" fine, but it is not
 * as universally supported as fragmented MP4 or MPEG-TS (e.g. it will not
 * play in a browser <video> tag).
 *
 * Usage:
 * @code
 * MuxerAVI avi(client); // any Print: File, WiFiClient, ...
 * MuxerVideoConfig cfg;
 * cfg.width = 640;
 * cfg.height = 480;
 * cfg.fps = 25;
 * cfg.format = VideoFormat::H264;
 * avi.setVideoInfo(cfg);
 * avi.begin();
 * // for each encoded H.264 access unit:
 * avi.addVideoFrame(h264_data, h264_len);
 * @endcode
 *
 * @ingroup codecs
 * @ingroup encoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MuxerAVI : public Muxer {
 public:
  MuxerAVI() { audio_info.format = AudioFormat::PCM; }
  MuxerAVI(Print &out) : MuxerAVI() { setOutput(out); }

  const char *mime() override { return "video/avi"; }

  /// Defines the output: e.g. a local File or a network Client
  void setOutput(Print &out) override { p_out = &out; }

  /// Defines the video track configuration - call before begin()
  void setVideoInfo(MuxerVideoConfig config) override { video_cfg = config; }

  /// Provides the video track configuration, analogous to DemuxerAVI's
  /// getVideoInfo() getter.
  MuxerVideoConfig getVideoInfo() override { return video_cfg; }

  /// Adds an (optional) interleaved audio track. 'info.format' selects the
  /// WAVEFORMATEX tag written for the audio track (default:
  /// AudioFormat::PCM). Only uncompressed PCM has been validated for
  /// playback in common players; other tags are written through as-is.
  /// Call before begin().
  void setAudioInfo(AudioInfoFormat info) override {
    audio_info = info;
    has_audio = true;
  }
  /// Provides read/write access to the audio track's AudioInfoFormat
  AudioInfoFormat &audioInfo() override { return audio_info; }

  /// Writes the RIFF/AVI header. Call after configuring video (and audio, if
  /// any) and before writing any frames.
  bool begin() override {
    if (p_out == nullptr) {
      LOGE("output not defined");
      return false;
    }
    if (video_cfg.width == 0 || video_cfg.height == 0) {
      LOGE("invalid video size: %d x %d", (int)video_cfg.width,
           (int)video_cfg.height);
      return false;
    }
    writeHeader();
    video_frame_count = 0;
    audio_chunk_count = 0;
    frame_open = false;
    is_open = true;
    return true;
  }

  /// Selects whether write() feeds the video or the audio track. Defaults
  /// to StreamContentType::Video; switch to StreamContentType::Audio (and
  /// back) around calls when using MuxerAVI as a plain sink for both.
  void setStreamType(StreamContentType type) override { write_stream_type = type; }
  /// The track write() currently targets (see setStreamType())
  StreamContentType streamType() override { return write_stream_type; }

  /// Closes the encoder: no trailer is written (streaming AVI has no idx1)
  void end() override { is_open = false; }

  operator bool() override { return is_open; }

  /// Starts a new video ('00dc') chunk of the given size. This is MuxerAVI's
  /// own API for incremental frame construction (not part of the VideoOutput
  /// interface - RIFF chunks are length-prefixed, so the size must be known
  /// upfront, unlike the write()/flush() contract VideoOutput consumers use).
  void beginFrame(size_t size) {
    if (!is_open) return;
    writeChunkHeader("00dc", (uint32_t)size);
    frame_open = true;
    frame_remaining = size;
    frame_pad = (size % 2) != 0;
  }

  /// Appends data to the currently open video frame - requires a prior
  /// beginFrame() (and a later endFrame()); use this for incremental/
  /// streaming sources that hand over a frame in pieces. For a single
  /// complete frame per call, use write() (dispatches by
  /// getVideoInfo().format) or one of the addXxxFrame() methods instead.
  size_t writeFrame(const uint8_t *data, size_t len) {
    if (!is_open || !frame_open) return 0;
    size_t to_write = len < frame_remaining ? len : frame_remaining;
    size_t written = p_out->write(data, to_write);
    frame_remaining -= written;
    return written;
  }

  /// Closes the current video frame (word-aligns the chunk)
  uint32_t endFrame() {
    if (frame_open && frame_pad) p_out->write((uint8_t)0);
    frame_open = false;
    video_frame_count++;
    return 0;
  }

  /// VideoOutput API / generic sink: writes one complete frame to whichever
  /// track streamType() currently selects (see setStreamType()). For video,
  /// dispatches to the addXxxFrame() matching getVideoInfo().format; for
  /// audio, calls addAudioFrame(). Lets MuxerAVI be used as a plain
  /// one-call-per-frame Print-like sink (e.g. from a camera driver, or an
  /// audio encoder) without the caller needing to know which addXxxFrame()
  /// applies.
  size_t write(const uint8_t *data, size_t len) override {
    if (write_stream_type == StreamContentType::Audio) {
      return addAudioFrame(data, len);
    }
    switch (video_cfg.format) {
      case VideoFormat::MJPEG:
        return addJpegFrame(data, len);
      case VideoFormat::YUV422:
        return addYUV422Frame(data, len);
      case VideoFormat::RGB565:
        return addRGB565Frame(data, len);
      case VideoFormat::I420:
        return addI420Frame(data, len);
      default:
        return addVideoFrame(data, len);
    }
  }

  /// Convenience: writes one complete, already-encoded video frame (e.g. one
  /// H.264 access unit, or one raw pixel buffer) as a single '00dc' chunk.
  /// Works for any VideoFormat - the format-specific addXxxFrame()
  /// methods below are thin wrappers that additionally validate the format
  /// and (for fixed-size raw formats) the buffer length.
  /// @param isKeyFrame accepted for interface compatibility with
  /// Muxer::addVideoFrame() - AVI has no per-frame sync-sample flag to
  /// write it into, so it is ignored.
  size_t addVideoFrame(const uint8_t *data, size_t len,
                       bool isKeyFrame = true) override {
    beginFrame(len);
    size_t written = writeFrame(data, len);
    endFrame();
    return written;
  }

  /// Writes one complete Motion-JPEG frame: a full, already-encoded JPEG
  /// image (e.g. as produced by an ESP32-CAM or other hardware JPEG
  /// encoder). Length is inherently variable, so only the configured
  /// format is validated, not the size.
  size_t addJpegFrame(const uint8_t *data, size_t len) override {
    checkVideoFormat(VideoFormat::MJPEG);
    return addVideoFrame(data, len);
  }

  /// Writes one packed 4:2:2 YUV frame (YUY2/YUYV byte order). Expects
  /// exactly width*height*2 bytes.
  size_t addYUV422Frame(const uint8_t *data, size_t len) override {
    checkRawFrame(VideoFormat::YUV422, len);
    return addVideoFrame(data, len);
  }

  /// Writes one uncompressed RGB565 (16-bit, 5-6-5) frame. Expects exactly
  /// width*height*2 bytes.
  size_t addRGB565Frame(const uint8_t *data, size_t len) override {
    checkRawFrame(VideoFormat::RGB565, len);
    return addVideoFrame(data, len);
  }

  /// Writes one planar 4:2:0 YUV (I420/IYUV) frame: a full-resolution Y
  /// plane followed by quarter-resolution U and V planes. Expects exactly
  /// width*height*3/2 bytes.
  size_t addI420Frame(const uint8_t *data, size_t len) override {
    checkRawFrame(VideoFormat::I420, len);
    return addVideoFrame(data, len);
  }

  /// Writes one interleaved audio block as a single '01wb' chunk. Ignored
  /// (returns 0) if no audio track was configured via setAudioInfo().
  size_t addAudioFrame(const uint8_t *data, size_t len) override {
    if (!is_open || !has_audio) return 0;
    writeChunkHeader("01wb", (uint32_t)len);
    size_t written = p_out->write(data, len);
    if (len % 2 != 0) p_out->write((uint8_t)0);
    audio_chunk_count++;
    return written;
  }

  /// Number of video frames written so far
  uint32_t videoFrameCount() { return video_frame_count; }
  /// Number of audio chunks written so far
  uint32_t audioChunkCount() { return audio_chunk_count; }

 protected:
  static const uint32_t AVIF_ISINTERLEAVED = 0x00000100;

  Print *p_out = nullptr;
  MuxerVideoConfig video_cfg;
  StreamContentType write_stream_type = StreamContentType::Video;
  AudioInfoFormat audio_info;
  bool has_audio = false;
  bool is_open = false;
  bool frame_open = false;
  bool frame_pad = false;
  size_t frame_remaining = 0;
  uint32_t video_frame_count = 0;
  uint32_t audio_chunk_count = 0;

  const char *fourCC() {
    if (video_cfg.fourcc != nullptr) return video_cfg.fourcc;
    switch (video_cfg.format) {
      case VideoFormat::H264:
        return "H264";
      case VideoFormat::MJPEG:
        return "MJPG";
      case VideoFormat::MPEG4:
        return "FMP4";
      case VideoFormat::RAW:
        return "DIB ";
      case VideoFormat::YUV422:
        return "YUY2";
      case VideoFormat::RGB565:
        return "RGBP";
      case VideoFormat::I420:
        return "I420";
      case VideoFormat::UNKNOWN:
        return "H264";
    }
    return "H264";
  }

  /// True if this format is written as uncompressed RGB565 using
  /// BI_BITFIELDS (which needs 3 extra DWORD color masks in 'strf')
  bool isBitfields() { return video_cfg.format == VideoFormat::RGB565; }

  /// Bits per pixel written into biBitCount; purely informational for
  /// compressed formats
  uint16_t biBitCount() {
    switch (video_cfg.format) {
      case VideoFormat::RAW:
        return 24;
      case VideoFormat::YUV422:
      case VideoFormat::RGB565:
        return 16;
      case VideoFormat::I420:
        return 12;
      default:
        return 24;
    }
  }

  /// Number of raw bytes a single frame must have for the given
  /// (uncompressed, fixed-size) format; 0 for compressed/variable-size
  /// formats where this check does not apply
  size_t expectedRawFrameSize(VideoFormat format) {
    size_t px = (size_t)video_cfg.width * (size_t)video_cfg.height;
    switch (format) {
      case VideoFormat::RAW:
        return px * 3;
      case VideoFormat::YUV422:
      case VideoFormat::RGB565:
        return px * 2;
      case VideoFormat::I420:
        return px + px / 2;
      default:
        return 0;
    }
  }

  /// biSizeImage: the uncompressed buffer size for raw formats, 0 (unknown/
  /// variable) for compressed ones
  uint32_t biSizeImage() { return (uint32_t)expectedRawFrameSize(video_cfg.format); }

  /// Writes the biCompression field: numeric BI_RGB(0)/BI_BITFIELDS(3) for
  /// uncompressed RGB, or the FOURCC codec tag for everything else
  void writeBiCompression() {
    if (video_cfg.format == VideoFormat::RAW) {
      writeU32(0);  // BI_RGB
    } else if (video_cfg.format == VideoFormat::RGB565) {
      writeU32(3);  // BI_BITFIELDS
    } else {
      writeFourCC(fourCC());  // e.g. H264, MJPG, FMP4, YUY2, I420
    }
  }

  /// Logs a warning if getVideoInfo().format was not set to the format that
  /// an addXxxFrame() convenience method was called for
  void checkVideoFormat(VideoFormat expected) {
    if (video_cfg.format != expected) {
      LOGW("getVideoInfo().format does not match the addXxxFrame() called");
    }
  }

  /// Validates both the configured format and (for fixed-size raw formats)
  /// that len matches width*height based expectations
  void checkRawFrame(VideoFormat expected, size_t len) {
    checkVideoFormat(expected);
    size_t expected_size = expectedRawFrameSize(expected);
    if (expected_size > 0 && len != expected_size) {
      LOGW("frame size %d does not match the expected %d bytes for %d x %d",
           (int)len, (int)expected_size, (int)video_cfg.width,
           (int)video_cfg.height);
    }
  }

  void writeU8(uint8_t v) { p_out->write(v); }
  void writeU16(uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF)};
    p_out->write(b, 2);
  }
  void writeI16(int16_t v) { writeU16((uint16_t)v); }
  void writeU32(uint32_t v) {
    uint8_t b[4] = {(uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF),
                    (uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 24) & 0xFF)};
    p_out->write(b, 4);
  }
  void writeFourCC(const char *cc) { p_out->write((const uint8_t *)cc, 4); }
  void writeZeros(int n) {
    for (int j = 0; j < n; j++) writeU8(0);
  }
  void writeChunkHeader(const char *id, uint32_t size) {
    writeFourCC(id);
    writeU32(size);
  }

  /// Size of the 'strf' chunk payload: the base 40-byte BITMAPINFOHEADER,
  /// plus 3 extra DWORD color masks for BI_BITFIELDS formats (RGB565)
  uint32_t videoStrfSize() { return 40 + (isBitfields() ? 12 : 0); }

  /// Bytes following the video strl LIST's own size field: the "strl"
  /// list-type FOURCC plus the nested strh/strf chunks (each incl. header)
  uint32_t videoStrlSize() { return 4 + (8 + 56) + (8 + videoStrfSize()); }
  /// Bytes following the audio strl LIST's own size field (only valid if
  /// has_audio)
  uint32_t audioStrlSize() { return 4 + (8 + 56) + (8 + 18); }

  void writeMainHeader() {
    uint32_t micros_per_frame =
        video_cfg.fps > 0 ? (uint32_t)(1000000.0f / video_cfg.fps) : 0;
    writeChunkHeader("avih", 56);
    writeU32(micros_per_frame);                    // dwMicroSecPerFrame
    writeU32(0);                                   // dwMaxBytesPerSec
    writeU32(0);                                   // dwPaddingGranularity
    writeU32(has_audio ? AVIF_ISINTERLEAVED : 0);  // dwFlags
    writeU32(0);                                   // dwTotalFrames (unknown)
    writeU32(0);                                   // dwInitialFrames
    writeU32(has_audio ? 2 : 1);                   // dwStreams
    writeU32(0);                                   // dwSuggestedBufferSize
    writeU32(video_cfg.width);                     // dwWidth
    writeU32(video_cfg.height);                    // dwHeight
    writeZeros(16);                                // dwReserved[4]
  }

  void writeVideoStrl() {
    writeFourCC("LIST");
    writeU32(videoStrlSize());
    writeFourCC("strl");

    // strh (AVIStreamHeader)
    writeChunkHeader("strh", 56);
    writeFourCC("vids");
    writeFourCC(fourCC());
    writeU32(0);                                  // dwFlags
    writeU16(0);                                  // wPriority
    writeU16(0);                                  // wLanguage
    writeU32(0);                                  // dwInitialFrames
    writeU32(1000);                               // dwScale
    writeU32((uint32_t)(video_cfg.fps * 1000));   // dwRate
    writeU32(0);                                  // dwStart
    writeU32(0);                                  // dwLength (unknown)
    writeU32(0);                                  // dwSuggestedBufferSize
    writeU32(0xFFFFFFFF);                         // dwQuality (use default)
    writeU32(0);                       // dwSampleSize (variable per frame)
    writeI16(0);                                  // rcFrame.left
    writeI16(0);                                  // rcFrame.top
    writeI16((int16_t)video_cfg.width);           // rcFrame.right
    writeI16((int16_t)video_cfg.height);          // rcFrame.bottom

    // strf (BITMAPINFOHEADER [+ 3 DWORD color masks for RGB565])
    writeChunkHeader("strf", videoStrfSize());
    writeU32(40);                // biSize (base header; masks, if any, follow)
    writeU32(video_cfg.width);   // biWidth
    writeU32(video_cfg.height);  // biHeight
    writeU16(1);                 // biPlanes
    writeU16(biBitCount());      // biBitCount
    writeBiCompression();        // biCompression
    writeU32(biSizeImage());     // biSizeImage
    writeU32(0);                 // biXPelsPerMeter
    writeU32(0);                 // biYPelsPerMeter
    writeU32(0);                 // biClrUsed
    writeU32(0);                 // biClrImportant
    if (isBitfields()) {
      writeU32(0x0000F800);  // red mask
      writeU32(0x000007E0);  // green mask
      writeU32(0x0000001F);  // blue mask
    }
  }

  void writeAudioStrl() {
    writeFourCC("LIST");
    writeU32(audioStrlSize());
    writeFourCC("strl");

    uint16_t block_align =
        (uint16_t)(audio_info.channels * (audio_info.bits_per_sample / 8));
    uint32_t byte_rate = (uint32_t)audio_info.sample_rate * block_align;

    // strh (AVIStreamHeader)
    writeChunkHeader("strh", 56);
    writeFourCC("auds");
    writeZeros(4);                      // fccHandler (unspecified)
    writeU32(0);                        // dwFlags
    writeU16(0);                        // wPriority
    writeU16(0);                        // wLanguage
    writeU32(0);                        // dwInitialFrames
    writeU32(1);                        // dwScale
    writeU32((uint32_t)audio_info.sample_rate);  // dwRate
    writeU32(0);                        // dwStart
    writeU32(0);                        // dwLength (unknown)
    writeU32(0);                        // dwSuggestedBufferSize
    writeU32(0xFFFFFFFF);               // dwQuality
    writeU32(block_align);              // dwSampleSize
    writeZeros(8);                      // rcFrame (unused for audio)

    // strf (WAVEFORMATEX)
    writeChunkHeader("strf", 18);
    writeU16((uint16_t)audio_info.format);       // wFormatTag
    writeU16(audio_info.channels);               // nChannels
    writeU32((uint32_t)audio_info.sample_rate);  // nSamplesPerSec
    writeU32(byte_rate);                         // nAvgBytesPerSec
    writeU16(block_align);                       // nBlockAlign
    writeU16(audio_info.bits_per_sample);        // wBitsPerSample
    writeU16(0);                                 // cbSize
  }

  void writeHeader() {
    // hdrl LIST payload: "hdrl" FOURCC + avih chunk + strl LIST(s), each incl
    // their own headers
    uint32_t hdrl_size = 4 + (8 + 56) + (8 + videoStrlSize());
    if (has_audio) hdrl_size += (8 + audioStrlSize());

    writeFourCC("RIFF");
    writeU32(0xFFFFFFFF);  // total file size: unknown while streaming
    writeFourCC("AVI ");

    writeFourCC("LIST");
    writeU32(hdrl_size);
    writeFourCC("hdrl");
    writeMainHeader();
    writeVideoStrl();
    if (has_audio) writeAudioStrl();

    writeFourCC("LIST");
    writeU32(0xFFFFFFFF);  // movi size: unknown while streaming
    writeFourCC("movi");
  }
};

} // namespace audio_tools