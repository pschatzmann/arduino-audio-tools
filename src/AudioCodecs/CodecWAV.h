#pragma once

#include "AudioCodecs/AudioCodecsBase.h"
#include "AudioCodecs/AudioFormat.h"


#define TAG(a, b, c, d)                                                  \
  ((static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | \
   (static_cast<uint32_t>(c) << 8) | (d))
#define READ_BUFFER_SIZE 512

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
};

static const char *wav_mime = "audio/wav";

/**
 * @brief Parser for Wav header data
 * for details see https://de.wikipedia.org/wiki/RIFF_WAVE
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class WAVHeader {
 public:
  WAVHeader() = default;

  /// Adds data to the 44 byte wav header data buffer and make it available for parsing
  int write(uint8_t *data, size_t data_len) {
    int write_len = min(data_len, 44 - len);
    memmove(buffer, data + len, write_len);
    len += write_len;
    LOGI("WAVHeader::write: %u -> %d -> %d", (unsigned)data_len, write_len,
         (int)len);
    return write_len;
  }

  /// Call begin when header data is complete to parse the data
  void parse() {
    LOGI("WAVHeader::begin: %u", (unsigned)len);
    this->data_pos = 0l;
    memset((void *)&headerInfo, 0, sizeof(WAVAudioInfo));
    while (!eof()) {
      uint32_t tag, tag2, length;
      tag = read_tag();
      if (eof()) break;
      length = read_int32();
      if (!length || length >= 0x7fff0000) {
        headerInfo.is_streamed = true;
        length = ~0;
      }
      if (tag != TAG('R', 'I', 'F', 'F') || length < 4) {
        seek(length, SEEK_CUR);
        continue;
      }
      headerInfo.file_size = length;
      tag2 = read_tag();
      length -= 4;
      if (tag2 != TAG('W', 'A', 'V', 'E')) {
        seek(length, SEEK_CUR);
        continue;
      }
      // RIFF chunk found, iterate through it
      while (length >= 8) {
        uint32_t subtag, sublength;
        subtag = read_tag();
        if (eof()) break;
        sublength = read_int32();
        length -= 8;
        if (length < sublength) break;
        if (subtag == TAG('f', 'm', 't', ' ')) {
          if (sublength < 16) {
            // Insufficient data for 'fmt '
            break;
          }
          headerInfo.format = (AudioFormat)read_int16();
          headerInfo.channels = read_int16();
          headerInfo.sample_rate = read_int32();
          headerInfo.byte_rate = read_int32();
          headerInfo.block_align = read_int16();
          headerInfo.bits_per_sample = read_int16();
          if (headerInfo.format == (AudioFormat) 0xfffe) {
            if (sublength < 28) {
              // Insufficient data for waveformatex
              break;
            }
            skip(8);
            headerInfo.format = (AudioFormat)read_int32();
            skip(sublength - 28);
          } else {
            skip(sublength - 16);
          }
          headerInfo.is_valid = true;
        } else if (subtag == TAG('d', 'a', 't', 'a')) {
          sound_pos = tell();
          headerInfo.data_length = sublength;
          if (!headerInfo.data_length || headerInfo.is_streamed) {
            headerInfo.is_streamed = true;
            logInfo();
            return;
          }
          seek(sublength, SEEK_CUR);
        } else {
          skip(sublength);
        }
        length -= sublength;
      }
      if (length > 0) {
        // Bad chunk?
        seek(length, SEEK_CUR);
      }
    }
    logInfo();
    len = 0;
  }

  /// Returns true if the header is complete (with 44 bytes)
  bool isDataComplete() { return len == 44; }

  /// provides the info from the header
  WAVAudioInfo &audioInfo() { return headerInfo; }

  /// Sets the info in the header
  void setAudioInfo(WAVAudioInfo info){
    headerInfo = info;
  }

  /// Just write a wav header to the indicated output
  void writeHeader(Print *out) {
    SingleBuffer<uint8_t> buffer(50);
    writeRiffHeader(buffer);
    writeFMT(buffer);
    writeDataHeader(buffer);
    len = buffer.available();
    out->write(buffer.data(), buffer.available());
  }

 protected:
  struct WAVAudioInfo headerInfo;
  uint8_t buffer[44];
  size_t len = 0;
  size_t data_pos = 0;
  size_t sound_pos = 0;

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
    if (data_pos < len)
      return buffer[data_pos++];
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

  bool eof() { return data_pos >= len - 1; }

  void logInfo() {
    LOGI("WAVHeader sound_pos: %lu", (unsigned long)sound_pos);
    LOGI("WAVHeader channels: %d ", headerInfo.channels);
    LOGI("WAVHeader bits_per_sample: %d", headerInfo.bits_per_sample);
    LOGI("WAVHeader sample_rate: %d ", (int) headerInfo.sample_rate);
    LOGI("WAVHeader format: %d", (int)headerInfo.format);
  }

  void writeRiffHeader(BaseBuffer<uint8_t> &buffer) {
    buffer.writeArray((uint8_t *)"RIFF", 4);
    write32(buffer, headerInfo.file_size - 8);
    buffer.writeArray((uint8_t *)"WAVE", 4);
  }

  void writeFMT(BaseBuffer<uint8_t> &buffer) {
    uint16_t fmt_len = 16;
    buffer.writeArray((uint8_t *)"fmt ", 4);
    write32(buffer, fmt_len);
    write16(buffer, (uint16_t)headerInfo.format);  // PCM
    write16(buffer, headerInfo.channels);
    write32(buffer, headerInfo.sample_rate);
    write32(buffer, headerInfo.byte_rate);
    write16(buffer, headerInfo.block_align);  // frame size
    write16(buffer, headerInfo.bits_per_sample);
  }

  void write32(BaseBuffer<uint8_t> &buffer, uint64_t value) {
    buffer.writeArray((uint8_t *)&value, 4);
  }

  void write16(BaseBuffer<uint8_t> &buffer, uint16_t value) {
    buffer.writeArray((uint8_t *)&value, 2);
  }

  void writeDataHeader(BaseBuffer<uint8_t> &buffer) {
    buffer.writeArray((uint8_t *)"data", 4);
    write32(buffer, headerInfo.file_size);
    int offset = headerInfo.offset;
    if (offset > 0) {
      uint8_t empty[offset];
      memset(empty, 0, offset);
      buffer.writeArray(empty, offset);  // resolve issue with wrong aligment
    }
  }

};

/**
 * @brief A simple WAVDecoder: We parse the header data on the first record to 
 * determine the format. If no AudioDecoderExt is specified we just write the PCM
 * data to the output that is defined by calling setOutput(). You can define a
 * ADPCM decoder to decode WAV files that contain ADPCM data.
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
   * @brief Construct a new WAVDecoder object for ADPCM data
   * 
  */
  WAVDecoder(AudioDecoderExt &dec, AudioFormat fmt) {
    setDecoder(dec, fmt);
  }

  /// Defines an optional decoder if the format is not PCM
  void setDecoder(AudioDecoderExt &dec, AudioFormat fmt)  {
    TRACED();
    decoder_format = fmt;
    p_decoder = &dec;
  }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override { this->p_print = &out_stream; }

  bool begin() override {
    TRACED();
    setupEncodedAudio();
    buffer24.reset();
    isFirst = true;
    active = true;
    return true;
  }

  void end() override {
    TRACED();
    buffer24.reset();
    active = false;
  }

  const char *mime() { return wav_mime; }

  WAVAudioInfo &audioInfoEx() { return header.audioInfo(); }

  AudioInfo audioInfo() override { return header.audioInfo(); }

  virtual size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    size_t result = 0;
    if (active) {
      if (isFirst) {
        result = decodeHeader((uint8_t*) data, len);
        if (result<len){
          result += write_out((uint8_t *)data+result, len-result);
        }
      } else if (isValid) {
        result = write_out((uint8_t *)data, len);
      }
    }
    return result;
  }

  virtual operator bool() override { return active; }

 protected:
  WAVHeader header;
  bool isFirst = true;
  bool isValid = true;
  bool active = false;
  AudioFormat decoder_format = AudioFormat::PCM;
  AudioDecoderExt *p_decoder = nullptr;
  EncodedAudioOutput dec_out;
  SingleBuffer<uint8_t> buffer24;

  Print& out() {
    return p_decoder==nullptr ? *p_print : dec_out;
  }

  virtual size_t write_out(const uint8_t *in_ptr, size_t in_size) {
    // check if we need to convert int24 data from 3 bytes to 4 bytes
    size_t result = 0;
    if (header.audioInfo().bits_per_sample == 24 && sizeof(int24_t)==4){
      write_out_24(in_ptr, in_size);
      result = in_size;
    } else {
      result = out().write(in_ptr, in_size);
    }
    return result;
  }

  // convert int24 to int32
  size_t write_out_24(const uint8_t *in_ptr, size_t in_size) {
    // make sure we can store a frame of 24bit (3bytes)
    AudioInfo& info = header.audioInfo();
    // in_size might be not a multiple of 3, so we use a buffer for a single frame
    buffer24.resize(info.channels*3);
    int result = 0;
    int32_t frame[info.channels];
    uint8_t val24[3]={0};
    
    // add all bytes to buffer
    for (int j=0;j<in_size;j++){
      buffer24.write(in_ptr[j]);
      // if buffer is full convert and output
      if (buffer24.availableForWrite()==0){
        for (int ch=0;ch<info.channels;ch++){
          buffer24.readArray((uint8_t*)&val24[0], 3);
          frame[ch] = interpret24bitAsInt32(val24);
          //LOGW("%d", frame[ch]);
        }
        assert(buffer24.available()==0);
        buffer24.reset();
        size_t written = out().write((uint8_t*)frame,sizeof(frame));
        assert(written==sizeof(frame));
        result += written;
      }
    }
    return result;
  }

  int32_t interpret24bitAsInt32(uint8_t* byteArray) {     
      return (  
          (static_cast<int32_t>(byteArray[2]) << 24)
      |   (static_cast<int32_t>(byteArray[1]) << 16)
      |   (static_cast<int32_t>(byteArray[0]) << 8)
      );  
  }
  

  int decodeHeader(uint8_t *in_ptr, size_t in_size) {
    int result = 0;
    // we expect at least the full header
    int written = header.write(in_ptr, in_size);
    if (!header.isDataComplete()) {
      return written;
    }
    // parse header
    header.parse();

    size_t len = in_size - written;
    uint8_t *sound_ptr = (uint8_t *)in_ptr + written;
    isFirst = false;
    isValid = header.audioInfo().is_valid;

    LOGI("WAV sample_rate: %d", (int) header.audioInfo().sample_rate);
    LOGI("WAV data_length: %u", (unsigned)header.audioInfo().data_length);
    LOGI("WAV is_streamed: %d", header.audioInfo().is_streamed);
    LOGI("WAV is_valid: %s",
          header.audioInfo().is_valid ? "true" : "false");

    // check format
    AudioFormat format = header.audioInfo().format;
    isValid = format == decoder_format;
    if (isValid) {
      // update blocksize
      if(p_decoder!=nullptr){
        int block_size = header.audioInfo().block_align;
        p_decoder->setBlockSize(block_size);
      }

      // update sampling rate if the target supports it
      AudioInfo bi;
      bi.sample_rate = header.audioInfo().sample_rate;
      bi.channels = header.audioInfo().channels;
      bi.bits_per_sample = header.audioInfo().bits_per_sample;
      notifyAudioChange(bi);
      // write prm data from first record
      LOGI("WAVDecoder writing first sound data");
      result = out().write(sound_ptr, len);
    } else {
      LOGE("WAV format not supported: %d", (int)format);
    }
    return result;
  }

  void setupEncodedAudio() {
    if (p_decoder!=nullptr){
      assert(p_print!=nullptr);
      dec_out.setOutput(p_print);
      dec_out.setDecoder(p_decoder);
      dec_out.begin();
    }
  }
};

/**
 * @brief A simple WAV file encoder. If no AudioEncoderExt is specified the WAV file contains
 * PCM data, otherwise it is encoded as ADPCM. The WAV header is written with the first writing
 * of audio data. Calling begin() is making sure that the header is written again.
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
  WAVEncoder(AudioEncoderExt &enc, AudioFormat fmt) {
    setEncoder(enc, fmt);
  };

  void setEncoder(AudioEncoderExt &enc, AudioFormat fmt)  {
    TRACED();
    audioInfo.format = fmt;
    p_encoder = &enc;
  }

  /// Defines the otuput stream
  void setOutput(Print &out) override {
    TRACED();
    p_print = &out;
  }

  /// Provides "audio/wav"
  const char *mime() override { return wav_mime; }

  // Provides the default configuration
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
  virtual void setAudioInfo(AudioInfo from)  override {
    audioInfo.sample_rate = from.sample_rate;
    audioInfo.channels = from.channels;
    audioInfo.bits_per_sample = from.bits_per_sample;
    // recalculate byte rate, block align...
    setAudioInfo(audioInfo);
  }

  /// Defines the WAVAudioInfo
  virtual void setAudioInfo(WAVAudioInfo ai) {
    AudioEncoder::setAudioInfo(ai);
    audioInfo = ai;
    LOGI("sample_rate: %d", (int)audioInfo.sample_rate);
    LOGI("channels: %d", audioInfo.channels);
    // bytes per second
    audioInfo.byte_rate = audioInfo.sample_rate * audioInfo.channels * audioInfo.bits_per_sample / 8;
    if (audioInfo.format == AudioFormat::PCM){
      audioInfo.block_align = audioInfo.bits_per_sample / 8 * audioInfo.channels;
    }    
    if (audioInfo.is_streamed || audioInfo.data_length == 0 ||
        audioInfo.data_length >= 0x7fff0000) {
      LOGI("is_streamed! because length is %u",
           (unsigned)audioInfo.data_length);
      audioInfo.is_streamed = true;
      audioInfo.data_length = ~0;
    } else {
      size_limit = audioInfo.data_length;
      LOGI("size_limit is %d", (int)size_limit);
    }
  }

  /// starts the processing
  bool begin(WAVAudioInfo ai) {
    setAudioInfo(ai);
    return begin();
  }

  /// starts the processing using the actual WAVAudioInfo
  virtual bool begin() override { 
    TRACED();
    setupEncodedAudio();
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
      header.setAudioInfo(audioInfo);
      header.writeHeader(p_print);
      audioInfo.file_size -= 44;
      header_written = true;
    }

    int32_t result = 0;
    Print *p_out = p_encoder==nullptr ? p_print : &enc_out;;
    if (audioInfo.is_streamed) {
      result = p_out->write((uint8_t *)data, len);
    } else if (size_limit > 0) {
      size_t write_size = min((size_t)len, (size_t)size_limit);
      result = p_out->write((uint8_t *)data, write_size);
      size_limit -= result;

      if (size_limit <= 0) {
        LOGI("The defined size was written - so we close the WAVEncoder now");
        is_open = false;
      }
    }
    return result;
  }

  operator bool() override { return is_open; }

  bool isOpen() { return is_open; }

  /// Adds n empty bytes at the beginning of the data
  void setDataOffset(uint16_t offset) { audioInfo.offset = offset; }

 protected:
  WAVHeader header;
  Print *p_print = nullptr; // final output  CopyEncoder copy; // used for PCM
  AudioEncoderExt *p_encoder = nullptr; 
  EncodedAudioOutput enc_out;
  WAVAudioInfo audioInfo = defaultConfig();
  int64_t size_limit = 0;
  bool header_written = false;
  volatile bool is_open = false;

  void setupEncodedAudio() {
    if (p_encoder!=nullptr){
      assert(p_print!=nullptr);
      enc_out.setOutput(p_print);
      enc_out.setEncoder(p_encoder);
      enc_out.setAudioInfo(audioInfo);
      enc_out.begin();
      // block size only available after begin(): update block size
      audioInfo.block_align = p_encoder->blockSize();
    }
  }
};

}  // namespace audio_tools
