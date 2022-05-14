/**
 * @file CodecFLAC.h
 * @author Phil Schatzmann
 * @brief FLAC Codec using  https://github.com/pschatzmann/arduino-libflac
 * @version 0.1
 * @date 2022-04-24
 */

#pragma once

#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
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
 * you should prefer the streaming interface where you call setOutputStream() before the begin and copy() in the loop.
 * Validated with http://www.2l.no/hires/
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FLACDecoder : public AudioDecoder {
 public:
  FLACDecoder(bool isOgg=false) {
    is_ogg = isOgg;
  }

  ~FLACDecoder() {}

  void setTimeout(uint64_t readTimeout=FLAC_READ_TIMEOUT_MS) {
    read_timeout_ms = readTimeout;
  }
  void setOgg(bool isOgg) {
    is_ogg = isOgg;
  }
  void setBufferSize(int size) {
    buffer_size = size;
  }

  AudioBaseInfo audioInfo() { 
    AudioBaseInfo info;
    info.sample_rate = FLAC__stream_decoder_get_sample_rate(decoder);
    info.channels = FLAC__stream_decoder_get_channels(decoder);
    info.bits_per_sample = 16; // only 16 is supported
    return info;
  }

  void begin() {
    LOGI(LOG_METHOD);
    is_active = true;

    // used buffered stream
    if (!isInputFromStream()) {
      LOGW("This uses a lot of RAM - Consider to use the stream interface!");
      buffer.resize(buffer_size);
    }

    if (decoder == nullptr) {
      if ((decoder = FLAC__stream_decoder_new()) == NULL) {
        LOGE("ERROR: allocating decoder");
        is_active = false;
        return;
      }
    }
    LOGI("FLAC__stream_decoder_new");

    FLAC__stream_decoder_set_md5_checking(decoder, false);

    if (is_ogg){
      init_status = FLAC__stream_decoder_init_ogg_stream( decoder, read_callback, nullptr, nullptr, nullptr, nullptr, write_callback, nullptr, error_callback, this);
    } else {
      init_status = FLAC__stream_decoder_init_stream( decoder, read_callback, nullptr, nullptr, nullptr, nullptr, write_callback, nullptr, error_callback, this);
    }

    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
      LOGE("ERROR: initializing decoder: %s", FLAC__StreamDecoderInitStatusString[init_status]);
      is_active = false;
      return;
    }
    LOGI("FLAC__stream_decoder_init_stream");
  }

  void end() {
    LOGI(LOG_METHOD);
    flush();
    FLAC__stream_decoder_delete(decoder);
    is_active = false;
  }

  /// Process all data in the buffer
  void flush() {
    while(FLAC__stream_decoder_process_single(decoder));
  }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
    p_notify = &bi;
  }

  /// Stream Interfce: Decode directly by taking data from the stream. This is more efficient
  /// then feeding the decoder with write: just call copy() in the loop
  void setInputStream(Stream &input) {
    p_input = &input;
  }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  virtual void setOutputStream(AudioStream &out_stream) {
    p_print = &out_stream;
    setNotifyAudioChange(out_stream);
  }
  virtual void setOutputStream(AudioPrint &out_stream) {
    p_print = &out_stream;
    setNotifyAudioChange(out_stream);
  }

  operator boolean() { return is_active; }

  /// We write the data into a buffer from where we process individual frames
  /// one by one
  size_t write(const void *data, size_t length) {
    LOGI("write: %d", length);
    size_t result = length;
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }

    // consume data if enough space is available in the buffer
    if (buffer.availableForWrite()>=length){
      buffer.writeArray((uint8_t*)data, length);
    } else {
      // data was not consumed
      result = 0;
    }

    // start decoding
    if (buffer.available() > buffer.size()-length){
      LOGI("decoding...");
      if (!FLAC__stream_decoder_process_single(decoder)) {
        LOGE("FLAC__stream_decoder_process_single");
        result = 0;
      }
    }

    return length;
  }

  /// Stream Interface: Process a single frame - only relevant when input stream has been defined
  void copy() {
    LOGI("copy");
    if (!is_active) {
      LOGE("not active");
      return;
    }
    if (p_input == nullptr) {
      LOGE("setInputStream was not called");
      return;
    }
    if (!FLAC__stream_decoder_process_single(decoder)) {
      LOGE("FLAC__stream_decoder_process_single");
      return;
    }
  }

 protected:
  bool is_active = false;
  bool is_ogg = false;
  AudioBaseInfo info;
  AudioBaseInfoDependent *p_notify = nullptr;
  FLAC__StreamDecoder *decoder = nullptr;
  FLAC__StreamDecoderInitStatus init_status;
  RingBuffer<uint8_t> buffer{0};
  int buffer_size = FLAC_BUFFER_SIZE;
  int buffer_pos = 0;
  Print *p_print = nullptr;
  Stream *p_input = nullptr;
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


  /// Callback which reads from stream
  static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte result_buffer[],size_t *bytes, void *client_data) {
    FLAC__StreamDecoderReadStatus result = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    LOGI("read_callback: %d", *bytes);
    FLACDecoder *self = (FLACDecoder *)client_data;
    if (self == nullptr || !self->is_active) {
      return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    if (self->isInputFromStream()) {
      // get data directly from stream
      *bytes = self->p_input->readBytes(result_buffer, *bytes);
      LOGD("-> %d", *bytes);
      if (self->isEof(*bytes)){
         result = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
         self->is_active = false;
      }
    } else {
      // get data from  buffer
      size_t count = min(*bytes, (size_t)self->buffer.available());
      count = min(count, (size_t)1024);
      self->buffer.readArray(result_buffer, count);
      *bytes = count;

      // if the buffer is empty we asume the stream is at it's end
      if (count==0){
        LOGI("EOS");
        result = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
      }
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
    LOGI("write_callback: %d", frame->header.blocksize);
    FLACDecoder *self = (FLACDecoder *)client_data;

    AudioBaseInfo actual_info = self->audioInfo();
    if (self->info != actual_info){
      self->info = actual_info;
      self->info.logInfo();
      int bps = FLAC__stream_decoder_get_bits_per_sample(decoder);
      if (bps!=16){
        LOGI("Converting from %d bits", bps);
      }
      if (self->p_notify != nullptr) {
        self->info = actual_info;
        self->p_notify->setAudioInfo(self->info);
      }
    }

    // write audio data
    int bps = FLAC__stream_decoder_get_bits_per_sample(decoder);
    int16_t sample;
    switch(bps){
      case 8:
        for (int j = 0; j < frame->header.blocksize; j++) {
          for (int i = 0; i < actual_info.channels; i++) {
            //self->output_buffer[j*actual_info.channels + i] = buffer[i][j]<<8;
            sample = buffer[i][j]<<8;;
            self->p_print->write((uint8_t *)&sample,2);
          }
        }
        break;
      case 16:
        for (int j = 0; j < frame->header.blocksize; j++) {
          for (int i = 0; i < actual_info.channels; i++) {
            //self->output_buffer[j*actual_info.channels + i] = buffer[i][j];
            sample = buffer[i][j];
            self->p_print->write((uint8_t *)&sample,2);
          }
        }
        break;
      case 24:
        for (int j = 0; j < frame->header.blocksize; j++) {
          for (int i = 0; i < actual_info.channels; i++) {
            //self->output_buffer[j*actual_info.channels + i] = buffer[i][j]>>8;
            sample = buffer[i][j] >>8;
            self->p_print->write((uint8_t *)&sample,2);
          }
        }
        break;
      case 32:
        for (int j = 0; j < frame->header.blocksize; j++) {
          for (int i = 0; i < actual_info.channels; i++) {
             //self->output_buffer[j*actual_info.channels+ i] = buffer[i][j]>>16;           
            sample = buffer[i][j] >>16;
            self->p_print->write((uint8_t *)&sample,2);
          }
        }
        break;
      default:
        LOGE("Unsupported bps: %d", bps);
    }
  
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
  }

};

}  // namespace audio_tools