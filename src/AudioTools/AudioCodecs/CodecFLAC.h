/**
 * @file CodecFLAC.h
 * @author Phil Schatzmann
 * @brief FLAC Codec using  https://github.com/pschatzmann/arduino-libflac
 * @version 0.1
 * @date 2022-04-24
 */

#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/AudioBasic/Net.h"
#include "flac.h"

#ifndef FLAC_READ_TIMEOUT_MS
#define FLAC_READ_TIMEOUT_MS 10000
#endif

#ifndef FLAC_BUFFER_SIZE
#define FLAC_BUFFER_SIZE (8 * 1024)
#endif


namespace audio_tools {

/**
 * @brief Decoder for FLAC. Depends on https://github.com/pschatzmann/arduino-libflac. We support an efficient streaming API and an very memory intensitiv standard interface. So 
 * you should prefer the streaming interface where you call setOutput() before the begin and copy() in the loop.
 * Validated with http://www.2l.no/hires/
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FLACDecoder : public StreamingDecoder {
 public:
  /// Default Constructor
  FLACDecoder(bool isOgg=false) {
    is_ogg = isOgg;
  }

  /// Destructor - calls end();
  ~FLACDecoder() { end(); }

  void setTimeout(uint64_t readTimeout=FLAC_READ_TIMEOUT_MS) {
    read_timeout_ms = readTimeout;
  }
  void setOgg(bool isOgg) {
    is_ogg = isOgg;
  }

  AudioInfo audioInfo() { 
    AudioInfo info;
    info.sample_rate = FLAC__stream_decoder_get_sample_rate(decoder);
    info.channels = FLAC__stream_decoder_get_channels(decoder);
    info.bits_per_sample = 16; // only 16 is supported
    return info;
  }

  bool begin() {
    TRACEI();
    is_active = false;
    if (decoder == nullptr) {
      if ((decoder = FLAC__stream_decoder_new()) == NULL) {
        LOGE("ERROR: allocating decoder");
        is_active = false;
        return false;
      }
      LOGI("FLAC__stream_decoder_new");
    }

    // if it is already active we close it
    auto state = FLAC__stream_decoder_get_state(decoder);
    if (state != FLAC__STREAM_DECODER_UNINITIALIZED){
      FLAC__stream_decoder_finish(decoder);
    }

    // deactivate md5 checking
    FLAC__stream_decoder_set_md5_checking(decoder, is_md5_checing);

    // init decoder
    if (is_ogg){
      init_status = FLAC__stream_decoder_init_ogg_stream( decoder, read_callback, nullptr, nullptr, nullptr, nullptr, write_callback, nullptr, error_callback, this);
    } else {
      init_status = FLAC__stream_decoder_init_stream( decoder, read_callback, nullptr, nullptr, nullptr, nullptr, write_callback, nullptr, error_callback, this);
    }

    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
      LOGE("ERROR: initializing decoder: %s", FLAC__StreamDecoderInitStatusString[init_status]);
      is_active = false;
      return false;
    }
    LOGI("FLAC is open");
    is_active = true;
    return true;
  }

  void end() {
    TRACEI();
    if (decoder != nullptr){
      flush();
      FLAC__stream_decoder_delete(decoder);
      decoder = nullptr;
    }
    is_active = false;
  }

  /// Process all data in the buffer
  void flush() {
    while(FLAC__stream_decoder_process_single(decoder));
  }


  operator bool() { return is_active; }


  /// Stream Interface: Process a single frame - only relevant when input stream has been defined
  bool copy() {
    LOGD("copy");
    if (!is_active) {
      LOGW("FLAC not active");
      return false;
    }
    if (p_input == nullptr) {
      LOGE("setInput was not called");
      return false;
    }
    if (!FLAC__stream_decoder_process_single(decoder)) {
      LOGE("FLAC__stream_decoder_process_single");
      return false;
    }
    return true;
  }

  /// Activate/deactivate md5 checking: call this before calling begin()
  void setMD5(bool flag){
    is_md5_checing = flag;
  }

 protected:
  bool is_active = false;
  bool is_ogg = false;
  bool is_md5_checing = false;
  AudioInfo info;
  FLAC__StreamDecoder *decoder = nullptr;
  FLAC__StreamDecoderInitStatus init_status;
  uint64_t time_last_read = 0;
  uint64_t read_timeout_ms = FLAC_READ_TIMEOUT_MS;


  /// Check if input is directly from stream - instead of writes
  bool isInputFromStream() { return p_input != nullptr; }

  /// Error callback
  static void error_callback(const FLAC__StreamDecoder *decoder,
                             FLAC__StreamDecoderErrorStatus status,
                             void *client_data) {
    LOGE(FLAC__StreamDecoderErrorStatusString[status]);
  }

  size_t readBytes(uint8_t *data, size_t len) override {
      return p_input->readBytes(data, len);
  }

  /// Callback which reads from stream
  static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte result_buffer[],size_t *bytes, void *client_data) {
    FLAC__StreamDecoderReadStatus result = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    LOGD("read_callback: %d", (int) *bytes);
    FLACDecoder *self = (FLACDecoder *)client_data;
    if (self == nullptr || !self->is_active) {
      return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    // get data directly from stream
    *bytes = self->readBytes(result_buffer, *bytes);
    LOGD("-> %d", (int) *bytes);
    if (self->isEof(*bytes)){
        result = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        self->is_active = false;
    }
    return result;
  }

  /// We return eof when we were subsequently getting 0 bytes for the timeout period.
  bool isEof(int bytes) {
    bool result = false;
      if (bytes==0){
        delay(5);
      } else {
        time_last_read=millis();
      }
      if (millis() - time_last_read >= read_timeout_ms){
        result = true;
      }
      return result;
  }

    /// Output decoded result to final output stream
  static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,const FLAC__int32 *const buffer[], void *client_data) {
    LOGD("write_callback: %u", (unsigned)frame->header.blocksize);
    FLACDecoder *self = (FLACDecoder *)client_data;

    AudioInfo actual_info = self->audioInfo();
    if (self->info != actual_info){
      self->info = actual_info;
      self->info.logInfo();
      int bps = FLAC__stream_decoder_get_bits_per_sample(decoder);
      if (bps!=16){
        LOGI("Converting from %d bits", bps);
      }
      self->info = actual_info;
      self->notifyAudioChange(self->info);
    }

    // write audio data
    int bps = FLAC__stream_decoder_get_bits_per_sample(decoder);
    int16_t result_frame[actual_info.channels];

    switch(bps){
      case 8:
        for (int j = 0; j < frame->header.blocksize; j++) {
          for (int i = 0; i < actual_info.channels; i++) {
            //self->output_buffer[j*actual_info.channels + i] = buffer[i][j]<<8;
            result_frame[i] = buffer[i][j]<<8;
          }
          self->p_print->write((uint8_t *)result_frame, sizeof(result_frame));
        }
        break;
      case 16:
        for (int j = 0; j < frame->header.blocksize; j++) {
          for (int i = 0; i < actual_info.channels; i++) {
            result_frame[i] = buffer[i][j];
          }
          self->p_print->write((uint8_t *)result_frame, sizeof(result_frame));
        }
        break;
      case 24:
        for (int j = 0; j < frame->header.blocksize; j++) {
          for (int i = 0; i < actual_info.channels; i++) {
            result_frame[i]  = buffer[i][j] >> 8;
          }
          self->p_print->write((uint8_t *)result_frame, sizeof(result_frame));
        }
        break;
      case 32:
        for (int j = 0; j < frame->header.blocksize; j++) {
          for (int i = 0; i < actual_info.channels; i++) {
            result_frame[i] = buffer[i][j] >> 16;
          }
          self->p_print->write((uint8_t *)result_frame, sizeof(result_frame));
        }
        break;
      default:
        LOGE("Unsupported bps: %d", bps);
    }
  
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
  }
};


/**
 * @brief FLACEncoder
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FLACEncoder : public AudioEncoder {
 public:
  /// Default Constructor
  FLACEncoder(bool isOgg = false) {
    setOgg(isOgg);
  }

  /// Destructor - calls end();
  ~FLACEncoder() { end(); }

  void setOgg(bool isOgg) {
    is_ogg = isOgg;
  }

  bool isOgg() {return is_ogg;}

  void setBlockSize(int size){
    flac_block_size = size;
  }

  int blockSize() {return flac_block_size; }

  void setCompressionLevel(int level){
    flac_compression_level = level;
  }

  int compressionLevel() {return flac_compression_level;}

  /// Defines the output Stream
  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  /// Provides "audio/pcm"
  const char *mime() override { return "audio/flac"; }

  /// We update the audio information which will be used in the begin method
  virtual void setAudioInfo(AudioInfo from) override { 
    cfg = from; 
    cfg.logInfo(); 
  }

  /// starts the processing using the actual AudioInfo
  virtual bool begin() override {
    TRACED();
    if (p_encoder==nullptr){
      p_encoder = FLAC__stream_encoder_new();
      if (p_encoder==nullptr){
        LOGE("FLAC__stream_encoder_new");
        return false;
      }
    }

    is_open = false;

    FLAC__stream_encoder_set_channels(p_encoder, cfg.channels);
    FLAC__stream_encoder_set_bits_per_sample(p_encoder, cfg.bits_per_sample);
    FLAC__stream_encoder_set_sample_rate(p_encoder, cfg.sample_rate);
    FLAC__stream_encoder_set_blocksize(p_encoder, flac_block_size);
    FLAC__stream_encoder_set_compression_level(p_encoder, flac_compression_level);

    // setup stream
    FLAC__StreamEncoderInitStatus status;
    if (is_ogg){
      status = FLAC__stream_encoder_init_ogg_stream(p_encoder, nullptr, write_callback, nullptr, nullptr, nullptr, this);
    } else {
      status = FLAC__stream_encoder_init_stream(p_encoder, write_callback, nullptr, nullptr, nullptr, this);
    }
    if (status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
      LOGE("ERROR: initializing decoder: %s", FLAC__StreamEncoderInitStatusString[status]);
      if (status==FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR){
        LOGE(" -> %s", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(p_encoder)]);
      }
      return false;
    }
    is_open = true;
    return true;
  }

  /// starts the processing
  bool begin(Print &out) {
    p_print = &out;
    return begin();
  }

  /// stops the processing
  void end() override {
    TRACED();
    if (p_encoder != nullptr) {
      FLAC__stream_encoder_delete(p_encoder);
      p_encoder = nullptr;
      is_open = false;
    }
  }

  /// Writes FLAC Packet
  virtual size_t write(const uint8_t *data, size_t len) override {
    if (!is_open || p_print == nullptr) return 0;
    LOGD("write: %zu", len);
    size_t result = 0;
    int samples=0;
    int frames=0;
    int32_t *data32=nullptr;
    switch(cfg.bits_per_sample){
      case 16:
        samples = len / sizeof(int16_t); 
        frames = samples / cfg.channels;
        writeBuffer((int16_t*)data, samples);
        data32 = buffer.data();
        break;

      case 24:
      case 32:
        samples = len / sizeof(int32_t);
        frames = samples / cfg.channels;
        data32 = (int32_t*) data;
        break;

      default:
        LOGE("bits_per_sample not supported: %d", (int) cfg.bits_per_sample);
        break;
    }

    if (frames>0){
      if (FLAC__stream_encoder_process_interleaved(p_encoder, data32, frames)){
        result = len;
      } else {
        LOGE("FLAC__stream_encoder_process_interleaved");
      }
    }

    return result;
  }

  operator bool() override { return is_open; }

  bool isOpen() { return is_open; }

 protected:
  AudioInfo cfg;
  Vector<FLAC__int32> buffer;
  Print *p_print = nullptr;
  FLAC__StreamEncoder *p_encoder=nullptr;
  bool is_open = false;
  bool is_ogg = false;
  int flac_block_size = 512; // small value to minimize allocated memory
  int flac_compression_level = 8;

  static FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void *client_data){
    FLACEncoder *self = (FLACEncoder *)client_data;
    if (self->p_print!=nullptr){
      size_t written = self->p_print->write((uint8_t*)buffer, bytes);
      if (written!=bytes){
        LOGE("write_callback %zu -> %zu", bytes, written);
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
      }
    }
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
  }

  void writeBuffer(int16_t * data, size_t samples) {
    buffer.resize(samples);
    for (int j=0;j<samples;j++){
      buffer[j] = data[j];
    }
  }
};

}  // namespace audio_tools

