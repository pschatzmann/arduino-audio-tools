#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/AudioCodecs/AudioFormat.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

#define READ_BUFFER_SIZE 512
#define MAX_WAV_HEADER_LEN 200
/// Hard upper bound the header buffer may grow to while searching for the
/// 'data' chunk tag (e.g. because of LIST/bext/other metadata chunks before
/// 'data'). Protects against an unbounded/never-ending search on malformed
/// input.
#define MAX_WAV_HEADER_LEN_LIMIT 2048
/// Granularity (in bytes, rounded down to a whole number of frames) in
/// which WAVEncoder feeds PCM data to an external (compressing) encoder
/// when a fixed data_length was declared via setDataLength(). Smaller
/// values make the point at which data_length is reached more precise (see
/// WAVEncoder::write()) at the cost of more, smaller writes to the encoder.
#define WAV_ENCODER_COMPRESSED_CHUNK_SIZE 1024

namespace audio_tools {

/**
 * @brief Sound information which is available in the WAV header
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
struct WAVAudioInfo : AudioInfo {
  WAVAudioInfo() = default;
  WAVAudioInfo(const AudioInfo &from) {
    sample_rate = from.sample_rate;
    channels = from.channels;
    bits_per_sample = from.bits_per_sample;
  }

  AudioFormat format = AudioFormat::PCM;
  int byte_rate = 0;
  int block_align = 0;
  bool is_streamed = true;
  bool is_valid = false;
  uint32_t data_length = 0;
  uint32_t file_size = 0;
  int offset = 0;
  /// write the extended 'fmt ' chunk + 'fact' chunk for ADPCM formats
  bool ext_adpcm_header = false;
};

static const char *wav_mime = "audio/wav";

/**
 * @brief Parser for Wav header data
 * for details see https://de.wikipedia.org/wiki/RIFF_WAVE
 *
 * Logic notes for `WAVAudioInfo` size fields:
 * - `file_size` stores the full RIFF file size in bytes.
 *   In the RIFF header this is written/read as `chunk_size = file_size - 8`.
 * - `data_length` stores the WAV payload size (`data` chunk length).
 * - `is_streamed` indicates unknown/unbounded payload length.
 *
 * Parsing behavior:
 * - RIFF `chunk_size` is normalized to full `file_size` by adding 8.
 * - If parsed `data_length` is `0` or very large (`>= 0x7fff0000`), the
 *   stream is treated as streamed (`is_streamed=true`) and `data_length` is
 *   normalized to `~0`.
 *
 * Writing behavior:
 * - RIFF writes `file_size - 8` into the chunk-size field.
 * - If `data_length==0` in non-streamed mode, it is derived from
 *   `file_size - 36` (standard WAV relationship).
 * - In streamed mode, `data_length` is written as max (`~0`) when not set.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class WAVHeader {
 public:
  WAVHeader() = default;

  /// Adds data to the wav header data buffer and make it available for
  /// parsing. The buffer grows automatically (up to MAX_WAV_HEADER_LEN_LIMIT)
  /// if the header does not fit into the initial MAX_WAV_HEADER_LEN bytes -
  /// e.g. because of LIST/bext/other metadata chunks before 'data'.
  int write(uint8_t *data, size_t data_len) {
    size_t needed = (size_t)buffer.available() + data_len;
    if (needed > buffer.size() && buffer.size() < MAX_WAV_HEADER_LEN_LIMIT) {
      size_t new_size = needed < (size_t)MAX_WAV_HEADER_LEN_LIMIT
                             ? needed
                             : (size_t)MAX_WAV_HEADER_LEN_LIMIT;
      buffer.resize(new_size);
    }
    return buffer.writeArray(data, data_len);
  }

  /// Call when header data write is complete to parse the data
  bool parse() {
    LOGI("WAVHeader::begin: %u", (unsigned)buffer.available());
    this->data_pos = 0l;
    memset((void *)&headerInfo, 0, sizeof(WAVAudioInfo));

    if (!setPos("RIFF")) return false;
    // RIFF stores chunk_size (= file_size - 8): normalize to full file size
    headerInfo.file_size = read_int32() + 8;
    if (!setPos("WAVE")) return false;
    if (!setPos("fmt ")) return false;
    int fmt_length = read_int32();
    headerInfo.format = (AudioFormat)read_int16();
    headerInfo.channels = read_int16();
    headerInfo.sample_rate = read_int32();
    headerInfo.byte_rate = read_int32();
    headerInfo.block_align = read_int16();
    headerInfo.bits_per_sample = read_int16();
    if (!setPos("data")) return false;
    headerInfo.data_length = read_int32();
    if (headerInfo.data_length == 0 || headerInfo.data_length >= 0x7fff0000) {
      headerInfo.is_streamed = true;
      headerInfo.data_length = ~0;
    }

    logInfo();
    buffer.clear();
    return true;
  }

  /// Returns true if the header is complete (containd data tag)
  bool isDataComplete() {
    int pos = getDataPos();
    return pos > 0 && buffer.available() >= pos;
  }

  /// True if the header buffer has grown to its maximum allowed size
  /// (MAX_WAV_HEADER_LEN_LIMIT) without finding the 'data' chunk - indicates
  /// a malformed or unusually large header that will never complete.
  bool isOverflow() {
    return buffer.size() >= MAX_WAV_HEADER_LEN_LIMIT && !isDataComplete();
  }

  /// number of bytes available in the header buffer
  size_t available() { return buffer.available(); }

  /// Determines the data start position using the data tag
  int getDataPos() {
    int pos =
        StrView((char *)buffer.data(), MAX_WAV_HEADER_LEN, buffer.available())
            .indexOf("data");
    return pos > 0 ? pos + 8 : 0;
  }

  /// provides the info from the header
  WAVAudioInfo &audioInfo() { return headerInfo; }

  /// Sets the info in the header
  void setAudioInfo(WAVAudioInfo info) { headerInfo = info; }

  /// Just write a wav header to the indicated outputbu
  bool writeHeader(Print *out) {
    return writeHeader(out, headerInfo);
  }

  /// Just write a wav header with explicit info to the indicated output
  bool writeHeader(Print *out, const WAVAudioInfo &info) {
    // reset first: buffer otherwise keeps accumulating bytes from earlier calls
    buffer.reset();
    writeRiffHeader(buffer, info);
    writeFMT(buffer, info);
    if (isADPCM(info.format) && info.ext_adpcm_header) {
      writeFactChunk(buffer, info);
    }
    writeDataHeader(buffer, info);
    int len = buffer.available();
    int written = out->write(buffer.data(), len);
    if (written != len) {
      LOGE("Failed to write WAV header to output: written %d of %d bytes", written, len);
    }
    return written == len;
  }

  /// Number of header bytes preceeding the 'data' chunk's payload, beyond the
  /// 36 bytes of a standard PCM header (12 byte RIFF + 24 byte fmt + 8 byte
  /// data tag/size = 36 + sizeof('fmt ' extension) + sizeof('fact' chunk)).
  /// Only non-zero for ADPCM formats when ext_adpcm_header is enabled.
  static int extraHeaderBytes(const WAVAudioInfo &info) {
    if (!info.ext_adpcm_header) return 0;
    switch (info.format) {
      case AudioFormat::ADPCM:      // MS ADPCM: 34 byte fmt extension + 12 byte fact chunk
        return 34 + 12;
      case AudioFormat::DVI_ADPCM:  // IMA/DVI ADPCM: 4 byte fmt extension + 12 byte fact chunk
        return 4 + 12;
      default:
        return 0;
    }
  }

  /// Reset internal stored header information and buffer
  void clear() {
    data_pos = 0;
    WAVAudioInfo empty;
    empty.sample_rate = 0;
    empty.channels = 0;
    empty.bits_per_sample = 0;
    headerInfo = empty;
    buffer.setClearWithZero(true);
    buffer.reset();
  }

  /// Debug helper: dumps header bytes as printable characters
  void dumpHeader() {
    char msg[buffer.available() + 1];
    memset(msg, 0, buffer.available() + 1);
    for (int j = 0; j < buffer.available(); j++) {
      char c = (char)buffer.data()[j];
      if (!isalpha(c)) {
        c = '.';
      }
      msg[j] = c;
    }
    LOGI("Header: %s", msg);
  }

 protected:
  WAVAudioInfo headerInfo;
  SingleBuffer<uint8_t> buffer{MAX_WAV_HEADER_LEN};
  size_t data_pos = 0;

  bool setPos(const char *id) {
    int id_len = strlen(id);
    int pos = indexOf(id);
    if (pos < 0) return false;
    data_pos = pos + id_len;
    return true;
  }

  int indexOf(const char *str) {
    return StrView((char *)buffer.data(), MAX_WAV_HEADER_LEN,
                   buffer.available())
        .indexOf(str);
  }

  uint32_t read_tag() {
    uint32_t tag = 0;
    tag = (tag << 8) | getChar();
    tag = (tag << 8) | getChar();
    tag = (tag << 8) | getChar();
    tag = (tag << 8) | getChar();
    return tag;
  }

  uint32_t getChar32() { return getChar(); }

  uint32_t read_int32() {
    uint32_t value = 0;
    value |= getChar32() << 0;
    value |= getChar32() << 8;
    value |= getChar32() << 16;
    value |= getChar32() << 24;
    return value;
  }

  uint16_t read_int16() {
    uint16_t value = 0;
    value |= getChar() << 0;
    value |= getChar() << 8;
    return value;
  }

  void skip(int n) {
    int i;
    for (i = 0; i < n; i++) getChar();
  }

  int getChar() {
    if (data_pos < buffer.size())
      return buffer.data()[data_pos++];
    else
      return -1;
  }

  void seek(long int offset, int origin) {
    if (origin == SEEK_SET) {
      data_pos = offset;
    } else if (origin == SEEK_CUR) {
      data_pos += offset;
    }
  }

  size_t tell() { return data_pos; }

  bool eof() { return data_pos >= buffer.size() - 1; }

  void logInfo() {
    LOGI("WAVHeader sound_pos: %d", getDataPos());
    LOGI("WAVHeader channels: %d ", headerInfo.channels);
    LOGI("WAVHeader bits_per_sample: %d", headerInfo.bits_per_sample);
    LOGI("WAVHeader sample_rate: %d ", (int)headerInfo.sample_rate);
    LOGI("WAVHeader format: %d", (int)headerInfo.format);
  }

  void writeRiffHeader(BaseBuffer<uint8_t> &buffer,
                       const WAVAudioInfo &info) {
    buffer.writeArray((uint8_t *)"RIFF", 4);
    // chunk_size = file_size - 8 (RIFF header size)
    uint32_t chunk_size = info.file_size > 8 ? info.file_size - 8 : 0;
    LOGI("writeRiffHeader: file_size=%u riff_size=%u", info.file_size, chunk_size);
    write32(buffer, chunk_size);
    buffer.writeArray((uint8_t *)"WAVE", 4);
  }

  void writeFMT(BaseBuffer<uint8_t> &buffer, const WAVAudioInfo &info) {
    bool is_ms_adpcm = info.ext_adpcm_header && info.format == AudioFormat::ADPCM;
    bool is_ima_adpcm = info.ext_adpcm_header && info.format == AudioFormat::DVI_ADPCM;
    uint16_t fmt_len = 16;
    if (is_ima_adpcm) fmt_len = 20;
    else if (is_ms_adpcm) fmt_len = 50;

    uint16_t spb = samplesPerBlock(info);
    uint32_t byte_rate = info.byte_rate;
    // use the real average byte rate for ADPCM formats whenever the block
    // layout is known, regardless of whether the extended 'fmt '/'fact'
    // chunk is written: byte_rate is informational (e.g. used by some
    // players for scrubbing/duration estimates) and the linear-PCM-based
    // default in info.byte_rate is misleading for compressed formats
    if (isADPCM(info.format) && spb > 0 && info.block_align > 0) {
      // average bytes/sec = (sample_rate * block_align) / samples_per_block
      byte_rate = ((uint64_t)info.sample_rate * info.block_align) / spb;
    }

    buffer.writeArray((uint8_t *)"fmt ", 4);
    write32(buffer, fmt_len);
    write16(buffer, (uint16_t)info.format);
    write16(buffer, info.channels);
    write32(buffer, info.sample_rate);
    write32(buffer, byte_rate);
    write16(buffer, info.block_align);  // frame size
    write16(buffer, info.bits_per_sample);

    if (is_ima_adpcm) {
      write16(buffer, 2);    // cbSize: size of extra format bytes
      write16(buffer, spb);  // wSamplesPerBlock
    } else if (is_ms_adpcm) {
      // standard MS ADPCM coefficient table (7 predictor pairs)
      static const int16_t ms_adpcm_coef[7][2] = {
          {256, 0}, {512, -256}, {0, 0}, {192, 64},
          {240, 0}, {460, -208}, {392, -232}};
      write16(buffer, 32);   // cbSize: size of extra format bytes
      write16(buffer, spb);  // wSamplesPerBlock
      write16(buffer, 7);    // wNumCoef
      for (auto &c : ms_adpcm_coef) {
        write16(buffer, (uint16_t)c[0]);
        write16(buffer, (uint16_t)c[1]);
      }
    }
  }

  /// 'fact' chunk: required for compressed (e.g. ADPCM) formats, holds the
  /// total number of samples (per channel) in the data chunk.
  void writeFactChunk(BaseBuffer<uint8_t> &buffer, const WAVAudioInfo &info) {
    buffer.writeArray((uint8_t *)"fact", 4);
    write32(buffer, 4);  // chunk size
    uint32_t sample_length = 0;
    uint16_t spb = samplesPerBlock(info);
    if (!info.is_streamed && spb > 0 && info.block_align > 0) {
      sample_length = (info.data_length / info.block_align) * spb;
    }
    write32(buffer, sample_length);
  }

  void writeDataHeader(BaseBuffer<uint8_t> &buffer, const WAVAudioInfo &info) {
    buffer.writeArray((uint8_t *)"data", 4);
    uint32_t data_length = info.data_length;
    uint32_t header_bytes = 36 + extraHeaderBytes(info);
    if (headerInfo.is_streamed && data_length == 0) {
      data_length = ~0;  // use max value for streamed data if not set
    }
    if (!headerInfo.is_streamed && info.file_size >= header_bytes && (data_length == 0 || data_length == ~0)) {
      data_length = info.file_size - header_bytes;  // data length = file size - header size
    }
    LOGI("writeDataHeader: data_length=%u", data_length);
    write32(buffer, data_length);
    int offset = info.offset;
    if (offset > 0) {
      uint8_t empty[offset];
      memset(empty, 0, offset);
      buffer.writeArray(empty, offset);  // resolve issue with wrong aligment
    }
  }

  static bool isADPCM(AudioFormat format) {
    return format == AudioFormat::ADPCM || format == AudioFormat::DVI_ADPCM;
  }

  /// Number of samples encoded in a single block, for ADPCM formats
  /// (assumes 4 bits per sample as used by MS/IMA ADPCM).
  static uint16_t samplesPerBlock(const WAVAudioInfo &info) {
    if (info.channels <= 0 || info.block_align <= 0) return 0;
    switch (info.format) {
      case AudioFormat::ADPCM:  // MS ADPCM
        return ((info.block_align / info.channels) - 7) * 2 + 2;
      case AudioFormat::DVI_ADPCM:  // IMA/DVI ADPCM
        return ((info.block_align / info.channels) - 4) * 2 + 1;
      default:
        return 0;
    }
  }

  void write32(BaseBuffer<uint8_t> &buffer, uint64_t value) {
    buffer.writeArray((uint8_t *)&value, 4);
  }

  void write16(BaseBuffer<uint8_t> &buffer, uint16_t value) {
    buffer.writeArray((uint8_t *)&value, 2);
  }

};

/**
 * @brief A simple WAVDecoder: We parse the header data on the first record to
 * determine the format. If no AudioDecoderExt is specified we just write the
 * PCM data to the output that is defined by calling setOutput(). You can define
 * a ADPCM decoder to decode WAV files that contain ADPCM data. Since WAV files
 * can use different ADPCM (or other) encodings, you can register more than one
 * decoder with addDecoder(): the decoder matching the format tag found in the
 * WAV header is selected automatically when a file is parsed.
 *
 * Besides plain PCM, the following formats are natively supported without
 * the need to provide an external decoder:
 * - IEEE_FLOAT (0x0003): 32-bit float PCM samples are converted to 16-bit
 *   linear PCM by default (clamped to [-1.0, 1.0] and scaled). This can be
 *   disabled with setConvertFloatToInt16(false) to pass through the raw
 *   32-bit float bytes instead.
 * - ALAW (0x0006) and MULAW (0x0007): the 8-bit logarithmic G.711 samples are
 *   expanded to 16-bit linear PCM. This can be disabled with
 *   setConvertALawMuLaw(false) to pass through the raw encoded bytes instead.
 *
 * Optionally, if the input WAV file contains 8-bit PCM data, you can enable automatic
 * conversion to 16-bit PCM output by calling setConvert8to16(true). This will convert
 * unsigned 8-bit samples to signed 16-bit samples before writing to the output stream,
 * and the reported bits_per_sample in audioInfo() will be 16 when conversion is active.
 * The same is valid for the 24 bit conversion which converts 24 bit (3 byte) to 32 bit
 * (4 byte).
 *
 * Please note that you need to call begin() everytime you process a new file to let the decoder
 * know that we start with a new header.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class WAVDecoder : public AudioDecoder {

 public:
  /**
   * @brief Construct a new WAVDecoder object for PCM data
   */
  WAVDecoder() = default;

  /**
   * @brief Construct a new WAVDecoder object for ADPCM data. If fmt is not
   * provided, it is derived from dec.wavFormat() (see setDecoder()).
   */
  WAVDecoder(AudioDecoderExt &dec, AudioFormat fmt = AudioFormat::UNKNOWN) {
    setDecoder(dec, fmt);
  }

  /// Defines an optional decoder if the format is not PCM. This replaces
  /// any previously registered decoders. To support WAV files that may use
  /// different ADPCM (or other) formats, register multiple decoders with
  /// addDecoder() instead.
  ///
  /// fmt is the WAV format tag (see AudioFormat) that dec decodes. It can
  /// be omitted if dec can identify its own format via
  /// AudioDecoderExt::wavFormat() - e.g. ADPCMDecoder does this for the
  /// codec ids that correspond to a well defined WAV format tag (currently
  /// MS ADPCM, IMA/DVI ADPCM and Yamaha ADPCM). For codecs that don't map
  /// to a single WAV format tag, fmt must be provided explicitly.
  void setDecoder(AudioDecoderExt &dec, AudioFormat fmt = AudioFormat::UNKNOWN) {
    TRACED();
    decoders.clear();
    addDecoder(dec, fmt);
  }

  /// Registers an additional decoder for a specific WAV format tag, on top
  /// of any decoder(s) already defined. The decoder used to decode the
  /// stream is selected automatically based on the format tag found in the
  /// WAV header, so a single WAVDecoder instance can transparently handle
  /// files using any of the registered formats (e.g. multiple ADPCM
  /// variants). If a decoder is already registered for the same format tag,
  /// it is replaced. See setDecoder() for details on the optional fmt
  /// parameter.
  void addDecoder(AudioDecoderExt &dec, AudioFormat fmt = AudioFormat::UNKNOWN) {
    TRACED();
    if (fmt == AudioFormat::UNKNOWN) fmt = dec.wavFormat();
    if (fmt == AudioFormat::UNKNOWN) {
      LOGE(
          "addDecoder: could not determine the WAV format tag for this "
          "decoder - please provide it explicitly");
      return;
    }
    for (int i = 0; i < decoders.size(); i++) {
      if (decoders[i].format == fmt) {
        decoders[i].decoder = &dec;
        return;
      }
    }
    decoders.push_back({fmt, &dec});
  }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override { this->p_print = &out_stream; }

  /// Prepare decoder for a new WAV stream
  bool begin() override {
    TRACED();
    header.clear();
    // close the decoder used for a previous stream (if any) before
    // selecting a new one for this stream: the match is only known once
    // the header has been parsed, so setupEncodedAudio() is triggered from
    // decodeHeader() instead of here
    endCurrentDecoder();
    byte_buffer.reset();
    buffer24.reset();
    buffer_float16.reset();
    isFirst = true;
    active = true;
    return true;
  }

  /// Finish decoding and release temporary buffers
  void end() override {
    TRACED();
    endCurrentDecoder();
    byte_buffer.reset();
    buffer24.reset();
    buffer_float16.reset();
    active = false;
  }

  /// Provides MIME type "audio/wav"
  const char *mime() { return wav_mime; }

  /// Extended WAV specific info (original header values)
  WAVAudioInfo &audioInfoEx() { return header.audioInfo(); }

  /// Exposed AudioInfo (may reflect conversion flags)
  AudioInfo audioInfo() override {
    WAVAudioInfo info = header.audioInfo();
    if (convert8to16 && info.format == AudioFormat::PCM &&
        info.bits_per_sample == 8) {
      info.bits_per_sample = 16;
    }
    // 32 bits gives better result
    if (convert24 && info.format == AudioFormat::PCM &&
        info.bits_per_sample == 24) {
      info.bits_per_sample = 32;
    }
    // ALAW/MULAW are expanded from 8-bit logarithmic to 16-bit linear PCM
    if (convertALawMuLaw && (info.format == AudioFormat::ALAW ||
                              info.format == AudioFormat::MULAW)) {
      info.bits_per_sample = 16;
    }
    // IEEE_FLOAT can optionally be converted to 16-bit linear PCM
    if (convertFloatToInt16 && info.format == AudioFormat::IEEE_FLOAT &&
        info.bits_per_sample == 32) {
      info.bits_per_sample = 16;
    }
    // any other non-PCM/non-IEEE_FLOAT format (e.g. ADPCM) is decoded by
    // p_decoder to 16-bit PCM; IEEE_FLOAT is passed through as-is
    if (info.format != AudioFormat::PCM &&
        info.format != AudioFormat::IEEE_FLOAT &&
        info.format != AudioFormat::ALAW &&
        info.format != AudioFormat::MULAW) {
      info.bits_per_sample = 16;
    }
    return info;
  }

  /// Write incoming WAV data (header + PCM) into output
  virtual size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    size_t result = 0;
    if (active) {
      if (isFirst) {
        int data_start = decodeHeader((uint8_t *)data, len);
        // we do not have the complete header yet: need more data
        if (data_start == 0) return len;
        // process the outstanding data - only if the format is supported;
        // otherwise report 0 bytes written, consistent with subsequent
        // write() calls once the format has been found to be invalid
        if (isValid) {
          result = data_start +
                   write_out((uint8_t *)data + data_start, len - data_start);
        }

      } else if (isValid) {
        result = write_out((uint8_t *)data, len);
      }
    }
    return result;
  }

  /// Check if the decoder is active
  virtual operator bool() override { return active; }

  /// Convert 8 bit to 16 bit PCM data (default: enabled)
  void setConvert8Bit(bool enable) {
    convert8to16 = enable;
  }

  /// Convert 24 bit (3 byte) to 32 bit (4 byte) PCM data (default: enabled)
  void setConvert24Bit(bool enable) {
    convert24 = enable;
  }

  /// Expand ALAW/MULAW 8-bit logarithmic samples to 16-bit linear PCM
  /// (default: enabled). If disabled, the raw encoded bytes are passed
  /// through unchanged.
  void setConvertALawMuLaw(bool enable) {
    convertALawMuLaw = enable;
  }

  /// Convert 32-bit IEEE_FLOAT samples to 16-bit linear PCM (default:
  /// enabled). Samples are clamped to [-1.0, 1.0] before scaling to the
  /// int16 range. If disabled, the raw 32-bit float bytes are passed
  /// through unchanged.
  void setConvertFloatToInt16(bool enable) {
    convertFloatToInt16 = enable;
  }

  /// Access to the internal header parser and info
  WAVHeader &getHeader() { return header; }

 protected:
  /// Associates a WAV format tag with the decoder responsible for it
  struct DecoderEntry {
    AudioFormat format = AudioFormat::PCM;
    AudioDecoderExt *decoder = nullptr;
    DecoderEntry() = default;
    DecoderEntry(AudioFormat fmt, AudioDecoderExt *dec)
        : format(fmt), decoder(dec) {}
  };

  WAVHeader header;
  bool isFirst = true;
  bool isValid = true;
  bool active = false;
  /// decoder matching the format of the WAV file currently being processed
  /// (selected in decodeHeader() from the registered decoders)
  AudioDecoderExt *p_decoder = nullptr;
  /// decoders registered via setDecoder()/addDecoder(), keyed by format tag
  Vector<DecoderEntry> decoders;
  EncodedAudioOutput dec_out;
  SingleBuffer<uint8_t> byte_buffer{0};
  SingleBuffer<int32_t> buffer24{0};
  SingleBuffer<int16_t> buffer_float16{0};
  bool convert8to16 = true;  // Optional conversion flag
  bool convert24 = true;  // Optional conversion flag
  bool convertALawMuLaw = true;  // Optional conversion flag
  bool convertFloatToInt16 = true;  // Optional conversion flag
  const size_t batch_size = 256;

  Print &out() { return p_decoder == nullptr ? *p_print : dec_out; }

  virtual size_t write_out(const uint8_t *in_ptr, size_t in_size) {
    // check if we need to convert int24 data from 3 bytes to 4 bytes
    size_t result = 0;
    AudioFormat format = header.audioInfo().format;
    if (convert24 && format == AudioFormat::PCM &&
        header.audioInfo().bits_per_sample == 24 && sizeof(int24_t) == 4) {
      write_out_24(in_ptr, in_size);
      result = in_size;
    } else if (convert8to16 && format == AudioFormat::PCM &&
               header.audioInfo().bits_per_sample == 8) {
      result = write_out_8to16(in_ptr, in_size);
    } else if (convertALawMuLaw && format == AudioFormat::ALAW) {
      result = write_out_alaw(in_ptr, in_size);
    } else if (convertALawMuLaw && format == AudioFormat::MULAW) {
      result = write_out_mulaw(in_ptr, in_size);
    } else if (convertFloatToInt16 && format == AudioFormat::IEEE_FLOAT &&
               header.audioInfo().bits_per_sample == 32) {
      result = write_out_float_to_int16(in_ptr, in_size);
    } else {
      result = out().write(in_ptr, in_size);
    }
    return result;
  }

  /// Convert 8-bit PCM to 16-bit PCM and write out
  size_t write_out_8to16(const uint8_t *in_ptr, size_t in_size) {
    size_t samples_remaining = in_size;
    size_t offset = 0;
    int16_t out_buf[batch_size];
    while (samples_remaining > 0) {
      size_t current_batch =
          samples_remaining > batch_size ? batch_size : samples_remaining;
      for (size_t i = 0; i < current_batch; ++i) {
        out_buf[i] = ((int16_t)in_ptr[offset + i] - 128) << 8;
      }
      writeDataT<int16_t>(&out(), out_buf, current_batch);
      offset += current_batch;
      samples_remaining -= current_batch;
    }
    return in_size;
  }

  /// convert 3 byte int24 to 4 byte int32
  size_t write_out_24(const uint8_t *in_ptr, size_t in_size) {
    // store 1 sample
    buffer24.resize(batch_size);
    byte_buffer.resize(3);

    for (size_t i = 0; i < in_size; i++) {
      // Add byte to buffer
      byte_buffer.write(in_ptr[i]);
      
      // Process complete sample when buffer is full
      if (byte_buffer.isFull()) {
        int24_3bytes_t sample24{byte_buffer.data()};
        int32_t converted_sample = sample24.scale32();
        buffer24.write(converted_sample);
        if (buffer24.isFull()) {
          writeDataT<int32_t>(&out(), buffer24.data(), buffer24.available());
          buffer24.reset();
        }
        byte_buffer.reset();
      }
    }

    return in_size;
  }

  /// Convert 32-bit IEEE_FLOAT samples to 16-bit linear PCM and write out.
  /// Samples are clamped to [-1.0, 1.0] before scaling to the int16 range.
  size_t write_out_float_to_int16(const uint8_t *in_ptr, size_t in_size) {
    buffer_float16.resize(batch_size);
    byte_buffer.resize(4);

    for (size_t i = 0; i < in_size; i++) {
      byte_buffer.write(in_ptr[i]);

      if (byte_buffer.isFull()) {
        float sample_f;
        memcpy(&sample_f, byte_buffer.data(), sizeof(sample_f));
        if (sample_f > 1.0f) sample_f = 1.0f;
        if (sample_f < -1.0f) sample_f = -1.0f;
        buffer_float16.write((int16_t)(sample_f * 32767.0f));
        if (buffer_float16.isFull()) {
          writeDataT<int16_t>(&out(), buffer_float16.data(),
                               buffer_float16.available());
          buffer_float16.reset();
        }
        byte_buffer.reset();
      }
    }

    return in_size;
  }

  /// Expand G.711 A-law 8-bit samples to 16-bit linear PCM and write out
  size_t write_out_alaw(const uint8_t *in_ptr, size_t in_size) {
    size_t remaining = in_size;
    size_t offset = 0;
    int16_t out_buf[batch_size];
    while (remaining > 0) {
      size_t current_batch =
          remaining > batch_size ? batch_size : remaining;
      for (size_t i = 0; i < current_batch; ++i) {
        out_buf[i] = alaw2linear(in_ptr[offset + i]);
      }
      writeDataT<int16_t>(&out(), out_buf, current_batch);
      offset += current_batch;
      remaining -= current_batch;
    }
    return in_size;
  }

  /// Expand G.711 mu-law 8-bit samples to 16-bit linear PCM and write out
  size_t write_out_mulaw(const uint8_t *in_ptr, size_t in_size) {
    size_t remaining = in_size;
    size_t offset = 0;
    int16_t out_buf[batch_size];
    while (remaining > 0) {
      size_t current_batch =
          remaining > batch_size ? batch_size : remaining;
      for (size_t i = 0; i < current_batch; ++i) {
        out_buf[i] = ulaw2linear(in_ptr[offset + i]);
      }
      writeDataT<int16_t>(&out(), out_buf, current_batch);
      offset += current_batch;
      remaining -= current_batch;
    }
    return in_size;
  }

  /// Converts a G.711 A-law encoded byte to a 16-bit linear PCM sample
  static int16_t alaw2linear(uint8_t a_val) {
    a_val ^= 0x55;
    int t = (a_val & 0x0f) << 4;
    int seg = ((unsigned)a_val & 0x70) >> 4;
    switch (seg) {
      case 0:
        t += 8;
        break;
      case 1:
        t += 0x108;
        break;
      default:
        t += 0x108;
        t <<= (seg - 1);
        break;
    }
    return (int16_t)((a_val & 0x80) ? t : -t);
  }

  /// Converts a G.711 mu-law encoded byte to a 16-bit linear PCM sample
  static int16_t ulaw2linear(uint8_t u_val) {
    u_val = ~u_val;
    int t = ((u_val & 0x0f) << 3) + 0x84;
    t <<= ((unsigned)u_val & 0x70) >> 4;
    return (int16_t)((u_val & 0x80) ? (0x84 - t) : (t - 0x84));
  }

  /// Decodes the header data: Returns the start pos of the data
  int decodeHeader(uint8_t *in_ptr, size_t in_size) {
    // we expect at least the full header
    header.write(in_ptr, in_size);
    if (!header.isDataComplete()) {
      if (header.isOverflow()) {
        // the header (up to and including the 'data' chunk tag) exceeds
        // MAX_WAV_HEADER_LEN_LIMIT without ever completing - give up
        // instead of waiting forever for data that will never arrive
        LOGE(
            "WAV header exceeds the maximum supported size of %d bytes "
            "without a 'data' chunk - aborting",
            (int)MAX_WAV_HEADER_LEN_LIMIT);
        isFirst = false;
        isValid = false;
        return 0;
      }
      LOGW("WAV header misses 'data' section in len: %d",
           (int)header.available());
      header.dumpHeader();
      return 0;
    }
    // parse header
    if (!header.parse()) {
      LOGE("WAV header parsing failed");
      return 0;
    }

    isFirst = false;

    LOGI("WAV sample_rate: %d", (int)header.audioInfo().sample_rate);
    LOGI("WAV data_length: %u", (unsigned)header.audioInfo().data_length);
    LOGI("WAV is_streamed: %d", header.audioInfo().is_streamed);

    // select the decoder matching the format found in the header (if any
    // was registered via setDecoder()/addDecoder()); otherwise fall back to
    // the natively supported formats (PCM, IEEE_FLOAT, ALAW, MULAW)
    AudioFormat format = header.audioInfo().format;
    p_decoder = findDecoder(format);
    isValid = p_decoder != nullptr ? true : isNativelySupported(format);
    if (isValid) {
      if (p_decoder != nullptr) {
        // update blocksize before begin() is called in setupEncodedAudio()
        int block_size = header.audioInfo().block_align;
        p_decoder->setBlockSize(block_size);
        setupEncodedAudio();
      }

      // update sampling rate if the target supports it
      AudioInfo bi = audioInfo();
      notifyAudioChange(bi);
    } else {
      LOGE("WAV format not supported: 0x%04X", (unsigned)format);
      logSupportedFormats();
    }
    return header.getDataPos();
  }

  /// Logs (at error level) the WAV format tags this instance can currently
  /// decode: the natively handled ones, plus any registered via
  /// setDecoder()/addDecoder()
  void logSupportedFormats() {
    LOGE(
        "Natively supported: PCM (0x%04X), IEEE_FLOAT (0x%04X), ALAW "
        "(0x%04X), MULAW (0x%04X)",
        (unsigned)AudioFormat::PCM, (unsigned)AudioFormat::IEEE_FLOAT,
        (unsigned)AudioFormat::ALAW, (unsigned)AudioFormat::MULAW);
    if (decoders.empty()) {
      LOGE(
          "No additional decoders registered - use "
          "setDecoder()/addDecoder() to support e.g. ADPCM formats");
    } else {
      for (int i = 0; i < decoders.size(); i++) {
        LOGE("Registered decoder for format: 0x%04X",
             (unsigned)decoders[i].format);
      }
    }
  }

  /// Finds the decoder registered for the given format tag, if any
  AudioDecoderExt *findDecoder(AudioFormat format) {
    for (int i = 0; i < decoders.size(); i++) {
      if (decoders[i].format == format) return decoders[i].decoder;
    }
    return nullptr;
  }

  void setupEncodedAudio() {
    if (p_decoder != nullptr) {
      assert(p_print != nullptr);
      dec_out.setOutput(p_print);
      dec_out.setDecoder(p_decoder);
      dec_out.begin(info);
    }
  }

  /// Ends the decoder currently selected for the stream just finished (if
  /// any), so it doesn't retain state (e.g. sample_rate/channels) across
  /// unrelated streams the next time this format is matched
  void endCurrentDecoder() {
    if (p_decoder != nullptr) {
      p_decoder->end();
      p_decoder = nullptr;
    }
  }

  /// Formats that are handled internally without an external decoder
  static bool isNativelySupported(AudioFormat format) {
    return format == AudioFormat::PCM || format == AudioFormat::IEEE_FLOAT ||
           format == AudioFormat::ALAW || format == AudioFormat::MULAW;
  }
};

/**
 * @brief Minimal Print wrapper that counts the bytes actually written to
 * the wrapped output. Used by WAVEncoder to track the number of *encoded*
 * bytes an external AudioEncoderExt has emitted, since AudioEncoder::write()
 * reports the number of raw PCM input bytes it accepted, not the number of
 * encoded bytes it produced (these differ for compressing formats like
 * ADPCM).
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CountingPrint : public Print {
 public:
  void setOutput(Print *out) { p_out = out; }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t *data, size_t len) override {
    size_t written = p_out != nullptr ? p_out->write(data, len) : 0;
    count += written;
    return written;
  }
  size_t count = 0;

 protected:
  Print *p_out = nullptr;
};

/**
 * @brief A simple WAV file encoder. If no AudioEncoderExt is specified the WAV
 * file contains PCM data, otherwise it is encoded as ADPCM. The WAV header is
 * written with the first writing of audio data. Calling begin() is making sure
 * that the header is written again.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class WAVEncoder : public AudioEncoder {
 public:
  /**
   * @brief Construct a new WAVEncoder object for PCM data
   */
  WAVEncoder() = default;

  /**
   * @brief Construct a new WAVEncoder object for ADPCM data
   */
  WAVEncoder(AudioEncoderExt &enc, AudioFormat fmt) { setEncoder(enc, fmt); };

  /// Associates an external encoder for non-PCM formats
  void setEncoder(AudioEncoderExt &enc, AudioFormat fmt) {
    TRACED();
    wav_info.format = fmt;
    p_encoder = &enc;
  }

  /// Defines the otuput stream
  void setOutput(Print &out) override {
    TRACED();
    p_print = &out;
  }

  /// Provides "audio/wav"
  const char *mime() override { return wav_mime; }

  /// Provides the default configuration
  WAVAudioInfo defaultConfig() {
    WAVAudioInfo info;
    info.format = AudioFormat::PCM;
    info.sample_rate = DEFAULT_SAMPLE_RATE;
    info.bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
    info.channels = DEFAULT_CHANNELS;
    info.is_streamed = true;
    info.is_valid = true;
    info.data_length = 0x7fff0000;
    info.file_size = info.data_length + 36;
    return info;
  }

  /// Update actual WAVAudioInfo
  virtual void setAudioInfo(AudioInfo from) override {
    wav_info.sample_rate = from.sample_rate;
    wav_info.channels = from.channels;
    wav_info.bits_per_sample = from.bits_per_sample;
    // recalculate byte rate, block align...
    setAudioInfo(wav_info);
  }

  /// Defines the WAVAudioInfo. Replaces wav_info wholesale - any settings
  /// made via setDataLength()/setDataOffset()/setExtADPCMHeader() are
  /// re-applied on top afterward (see applyPendingOverrides()), so they
  /// stick regardless of whether those setters were called before or after
  /// this one.
  virtual void setAudioInfo(WAVAudioInfo ai) {
    AudioEncoder::setAudioInfo(ai);
    if (p_encoder) p_encoder->setAudioInfo(ai);
    wav_info = ai;
    LOGI("sample_rate: %d", (int)wav_info.sample_rate);
    LOGI("channels: %d", wav_info.channels);
    LOGI("bits_per_sample: %d", wav_info.bits_per_sample);
    // bytes per second
    wav_info.byte_rate = wav_info.sample_rate * wav_info.channels *
                          wav_info.bits_per_sample / 8;
    // uncompressed formats with a fixed size per sample: block_align is
    // simply the frame size. Compressed formats (e.g. ADPCM) are handled
    // via p_encoder instead.
    if (wav_info.format == AudioFormat::PCM ||
        wav_info.format == AudioFormat::IEEE_FLOAT ||
        wav_info.format == AudioFormat::ALAW ||
        wav_info.format == AudioFormat::MULAW) {
      wav_info.block_align =
          wav_info.bits_per_sample / 8 * wav_info.channels;
    }
    applyPendingOverrides();
  }

  /// starts the processing
  bool begin(WAVAudioInfo ai) {
    setAudioInfo(ai);
    return begin();
  }

  /// starts the processing using the actual WAVAudioInfo
  virtual bool begin() override {
    TRACED();
    header.clear();
    if (!setupEncodedAudio()) {
      is_open = false;
      return false;
    }

    // normalize streaming mode and payload limits at start time
    if (wav_info.is_streamed || wav_info.data_length == 0 ||
        wav_info.data_length >= 0x7fff0000) {
      LOGI("is_streamed! because length is %u",
           (unsigned)wav_info.data_length);
      wav_info.is_streamed = true;
      wav_info.data_length = ~0;
      size_limit = 0;
    } else {
      wav_info.is_streamed = false;
      size_limit = wav_info.data_length;
      LOGI("size_limit is %d", (int)size_limit);
    }

    header_written = false;
    is_open = true;
    return true;
  }

  /// stops the processing
  void end() override { is_open = false; }

  /// Writes PCM data to be encoded as WAV
  virtual size_t write(const uint8_t *data, size_t len) override {
    if (!is_open) {
      LOGE("The WAVEncoder is not open - please call begin()");
      return 0;
    }

    if (p_print == nullptr) {
      LOGE("No output stream was provided");
      return 0;
    }

    if (!header_written) {
      LOGI("Writing Header");
      if (!header.writeHeader(p_print, wav_info)) {
        LOGE("Failed to write WAV header");
        is_open = false;
        return 0;
      }
      header_written = true;
    }

    size_t result = 0;
    Print *p_out = p_encoder == nullptr ? p_print : &enc_out;

    if (wav_info.is_streamed) {
      result = p_out->write((uint8_t *)data, len);
    } else if (size_limit > 0) {
      if (p_encoder == nullptr) {
        // uncompressed: input bytes map 1:1 to output bytes, so we can
        // truncate exactly at the declared data_length boundary
        size_t write_size = min((size_t)len, (size_t)size_limit);
        result = p_out->write((uint8_t *)data, write_size);
        size_limit -= result;
      } else {
        // compressed: the number of encoded bytes an external encoder
        // produces for a given amount of PCM input is not known in advance
        // (and may only be emitted once a full internal block is ready), so
        // AudioEncoder::write()'s return value (raw PCM input bytes
        // accepted) cannot be used to track progress towards data_length.
        // Track the *actual* encoded bytes written via counting_print, and
        // feed the encoder in frame-aligned chunks so we can stop as soon
        // as data_length is reached - result then correctly reflects only
        // the bytes of `data` that were actually consumed, so the caller
        // knows what still needs to be resent (e.g. to a new file).
        int frame_size = wav_info.bits_per_sample / 8 * wav_info.channels;
        if (frame_size <= 0) frame_size = 1;
        size_t chunk = (WAV_ENCODER_COMPRESSED_CHUNK_SIZE / frame_size) * frame_size;
        if (chunk == 0) chunk = frame_size;

        while (result < len && size_limit > 0) {
          size_t n = min(chunk, len - result);
          counting_print.count = 0;
          size_t accepted = p_out->write((uint8_t *)data + result, n);
          size_limit -= counting_print.count;
          result += accepted;
          if (accepted < n) break;  // encoder didn't accept the full chunk
        }
      }

      if (size_limit <= 0) {
        LOGI("The defined size was written - so we close the WAVEncoder now");
        is_open = false;
      }
    }
    return result;
  }

  /// Check if encoder is active and ready to write
  operator bool() override { return is_open; }

  /// Check if encoder is open
  bool isOpen() { return is_open; }

  /// Adds n empty bytes at the beginning of the data. Sticks even if
  /// setAudioInfo(WAVAudioInfo)/begin(WAVAudioInfo) is called afterward
  /// (see applyPendingOverrides()).
  void setDataOffset(uint16_t offset) {
    has_pending_offset = true;
    pending_offset = offset;
    wav_info.offset = offset;
  }

  /// Writes the extended 'fmt ' chunk + 'fact' chunk for ADPCM formats
  /// (disabled by default - the standard 16-byte 'fmt ' chunk is written).
  /// Sticks even if setAudioInfo(WAVAudioInfo)/begin(WAVAudioInfo) is
  /// called afterward (see applyPendingOverrides()).
  void setExtADPCMHeader(bool enable) {
    has_pending_ext_adpcm_header = true;
    pending_ext_adpcm_header = enable;
    wav_info.ext_adpcm_header = enable;
  }

  /// Defines the WAV payload length in bytes (without header). Sticks even
  /// if setAudioInfo(WAVAudioInfo)/begin(WAVAudioInfo) is called afterward
  /// (see applyPendingOverrides()) - e.g. calling setDataLength() before
  /// begin(WAVAudioInfo) will not be silently discarded.
  void setDataLength(uint32_t data_length) {
    has_pending_data_length = true;
    pending_data_length = data_length;
    applyPendingOverrides();
    setAudioInfo(wav_info);
  }

  /// Extended WAV specific info
  WAVAudioInfo &audioInfoEx() { return wav_info; }

  /// Access to the internal header parser and info
  WAVHeader &getHeader() { return header; }

 protected:
  WAVHeader header;
  Print *p_print = nullptr;  // final output  CopyEncoder copy; // used for PCM
  AudioEncoderExt *p_encoder = nullptr;
  EncodedAudioOutput enc_out;
  /// tracks the actual encoded bytes p_encoder emits to p_print (see write())
  CountingPrint counting_print;
  WAVAudioInfo wav_info = defaultConfig();
  int64_t size_limit = 0;
  bool header_written = false;
  volatile bool is_open = false;

  // fields set via setDataLength()/setDataOffset()/setExtADPCMHeader() that
  // must survive a subsequent wholesale setAudioInfo(WAVAudioInfo)/
  // begin(WAVAudioInfo) call, regardless of call order - see
  // applyPendingOverrides()
  bool has_pending_data_length = false;
  uint32_t pending_data_length = 0;
  bool has_pending_offset = false;
  uint16_t pending_offset = 0;
  bool has_pending_ext_adpcm_header = false;
  bool pending_ext_adpcm_header = false;

  /// Re-applies any settings configured via setDataLength()/setDataOffset()/
  /// setExtADPCMHeader() on top of wav_info. Called after every wholesale
  /// (re)assignment of wav_info (setAudioInfo(WAVAudioInfo)), so these
  /// settings stick no matter whether the dedicated setters were called
  /// before or after setAudioInfo(WAVAudioInfo)/begin(WAVAudioInfo).
  void applyPendingOverrides() {
    // must run before pending_data_length below: its file_size calculation
    // depends on wav_info.ext_adpcm_header
    if (has_pending_ext_adpcm_header) {
      wav_info.ext_adpcm_header = pending_ext_adpcm_header;
    }
    if (has_pending_offset) {
      wav_info.offset = pending_offset;
    }
    if (has_pending_data_length) {
      wav_info.data_length = pending_data_length;
      wav_info.is_streamed =
          (pending_data_length == 0 || pending_data_length >= 0x7fff0000);
      if (!wav_info.is_streamed) {
        // full file size = RIFF chunk (36) + format specific extra header
        // bytes (e.g. ADPCM 'fmt ' extension + 'fact' chunk) + data chunk
        // payload
        wav_info.file_size = wav_info.data_length + 36 +
                              WAVHeader::extraHeaderBytes(wav_info);
      }
    }
  }

  /// Sets up the external encoder pipeline. Returns false (and logs an
  /// error) instead of crashing if the required output stream has not been
  /// provided yet via setOutput().
  bool setupEncodedAudio() {
    if (p_encoder != nullptr) {
      if (p_print == nullptr) {
        LOGE(
            "setupEncodedAudio: no output stream was provided - call "
            "setOutput() before begin()");
        return false;
      }
      counting_print.setOutput(p_print);
      counting_print.count = 0;
      enc_out.setOutput(&counting_print);
      enc_out.setEncoder(p_encoder);
      enc_out.setAudioInfo(wav_info);
      enc_out.begin();
      // block size only available after begin(): update block size
      wav_info.block_align = p_encoder->blockSize();
    }
    return true;
  }
};

}  // namespace audio_tools
