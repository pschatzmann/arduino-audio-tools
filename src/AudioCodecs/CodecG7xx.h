#pragma once

extern "C"{
  #include "g72x.h"
}


namespace audio_tools {

/**
 * @brief Supported codecs by G7xxDecoder and G7xxEncoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
enum G7xxCODEC_e {g723_24, g721, g723_40, others};

/**
 * @brief g723_24, g721, g723_40 Decoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class G7xxDecoder : public AudioDecoder {
 public:
  G7xxDecoder(G7xxCODEC_e codec) {
    cfg.channels = 1;
    cfg.sample_rate = 8000;
    cfg.bits_per_sample = 16;

    switch(codec){
      case g723_24:
          dec_routine = g723_24_decoder;
          dec_bits = 3;
          break;

      case g721:
          dec_routine = g721_decoder;
          dec_bits = 4;
          break;

      case g723_40:
          dec_routine = g723_40_decoder;
          dec_bits = 5;
          break;
    }
  }

  void setAudioInfo(AudioInfo cfg) override { 
    if (cfg.channels!=1){
      LOGE("channels must be 1 instead of %d", cfg.channels);
    }
    if (cfg.sample_rate!=8000){
      LOGE("sample_rate must be 8000 instead of %d", cfg.sample_rate);
    }
    if (cfg.bits_per_sample!=16){
      LOGE("bits_per_sample must be 16 instead of %d", cfg.bits_per_sample);
    }
  }

  AudioInfo audioInfo()override { return cfg; }

  virtual void begin(AudioInfo cfg)  {
    setAudioInfo(cfg);
    begin();
  }

  void begin() override {
    TRACEI();
    in_buffer = 0;
    in_bits = 0;
    out_size = sizeof(int16_t);
    g72x_init_state(&state);

    is_active = true;
  }

  void end() override {
    TRACEI();
    is_active = false;
  }

  void setNotifyAudioChange(AudioInfoSupport &bi) override {
    p_notify = &bi;
  }

  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  operator bool() { return is_active; }

  size_t write(const void *data, size_t length) override {
    LOGD("write: %d", length);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }

    uint8_t *p_byte = (uint8_t *)data;
    for (int j = 0; j < length; j++) {
      sample = (*dec_routine)(p_byte[j], AUDIO_ENCODING_LINEAR, &state);
      p_print->write((uint8_t*)&sample, out_size);
    }

    return length;
  }

 protected:
  Print *p_print = nullptr;
  AudioInfo cfg;
  AudioInfoSupport *p_notify = nullptr;
  int input_pos = 0;
  bool is_active = false;
  int16_t sample;
  unsigned char code;
  int n;
  struct g72x_state state;
  int out_size;
  int (*dec_routine)(int code, int out_coding, struct g72x_state* state_ptr);
  int dec_bits;
  unsigned int in_buffer = 0;
  int in_bits = 0;

};

/**
 * @brief g723_24, g721, g723_40 Encoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G7xxEncoder : public AudioEncoder {
 public:
  G7xxEncoder(G7xxCODEC_e codec) {
    cfg.channels = 1;
    cfg.sample_rate = 8000;
    cfg.bits_per_sample = 16;

    switch(codec){

      case g721:
          enc_routine = g721_encoder;
          enc_bits = 4;
          p_mime = "audio/g721";
          break;

      case g723_24:
          enc_routine = g723_24_encoder;
          enc_bits = 3;
          p_mime = "audio/g723_24";
          break;

      case g723_40:
          enc_routine = g723_40_encoder;
          enc_bits = 5;
          p_mime = "audio/g723_40";
          break;
    }
  }

  virtual void begin(AudioInfo bi) {
    setAudioInfo(bi);
    begin();
  }

  void begin() override {
    TRACEI();
    g72x_init_state(&state);
    out_buffer = 0;
    out_bits = 0;

    is_active = true;
  }

  void end() override {
    TRACEI();
    is_active = false;
  }

  const char *mime() override { return p_mime; }

  virtual void setAudioInfo(AudioInfo cfg) { 
    if (cfg.channels!=1){
      LOGE("channels must be 1 instead of %d", cfg.channels);
    }
    if (cfg.sample_rate!=8000){
      LOGE("sample_rate must be 8000 instead of %d", cfg.sample_rate);
    }
    if (cfg.bits_per_sample!=16){
      LOGE("bits_per_sample must be 16 instead of %d", cfg.bits_per_sample);
    }
  }

  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  operator bool() { return is_active; }

   size_t write(const void *in_ptr, size_t byte_count) override {
    LOGD("write: %d", byte_count);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    // encode bytes
    int16_t *p_16 = (int16_t *)in_ptr;
    int samples = byte_count / sizeof(int16_t);
    for (int j = 0; j < samples; j++) {
        code = (*enc_routine)(p_16[j], AUDIO_ENCODING_LINEAR, &state);
        p_print->write(&code, 1);
    }
   
    return byte_count;
  }

 protected:
  AudioInfo cfg;
  Print *p_print = nullptr;
  bool is_active = false;
  const char *p_mime = nullptr;
  int resid;
  struct g72x_state state;
  unsigned char sample_char;
  int16_t sample_int16;
  unsigned char code;
  int (*enc_routine)(int sample, int in_coding, struct g72x_state* state_ptr);
  int enc_bits;
  unsigned int out_buffer = 0;
  int out_bits = 0;

};

/**
 * @brief 32Kbps G721 Decoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G721Decoder : public G7xxDecoder {
  public:
  G721Decoder() : G7xxDecoder(g721) {};
};
/**
 * @brief 32Kbps G721 Encoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G721Encoder : public G7xxEncoder {
  public:
  G721Encoder() :  G7xxEncoder(g721) {};
};
/**
 * @brief 24Kbps G723 Decoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G723_24Decoder : public G7xxDecoder {
  public:
  G723_24Decoder() : G7xxDecoder(g723_24) {};
};
/**
 * @brief 24Kbps G723 Encoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G723_24Encoder : public G7xxEncoder {
  public:
  G723_24Encoder() : G7xxEncoder(g723_24) {};
};
/**
 * @brief 40Kbps G723 Decoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G723_40Decoder : public G7xxDecoder {
  public:
  G723_40Decoder() : G7xxDecoder(g723_40) {};
};
/**
 * @brief 40Kbps G723 Encoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G723_40Encoder : public G7xxEncoder {
  public:
  G723_40Encoder() : G7xxEncoder(g723_40) {};
};

/**
 * @brief 64 kbit/s g711 ULOW Encoder based on https://github.com/pschatzmann/arduino-libg7xx
 * Supported encoder parameters: linear2alaw2, linear2ulaw
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G711Encoder : public G7xxEncoder {
  public:
  G711Encoder(uint8_t(*enc)(int)) : G7xxEncoder(others) {
    this->enc = enc;
    assert(this->enc!=nullptr);
  };
  size_t write(const void *in_ptr, size_t in_size) override {
    LOGD("write: %d", in_size);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    // encode bytes
    int samples = in_size/2;
    int16_t *p_16 = (int16_t *)in_ptr;
    uint8_t buffer[samples];
    for (int j = 0; j < samples; j++) {
	      buffer[j] = enc(p_16[j]);
    }
    p_print->write(buffer,samples);
    return in_size;
  }
  protected:
  uint8_t(*enc)(int)=nullptr;
};

/**
 * @brief 64 kbit/s  g711 ULOW Decoder based on https://github.com/pschatzmann/arduino-libg7xx
 * Supported decoder parameters: alaw2linear, ulaw2linear
 * @author Phil Schatzmann
 * @ingroup codecs
 * @ingroup encoder
 * @copyright GPLv3
 */
class G711Decoder : public G7xxDecoder {
  public:
  G711Decoder(int (*dec)(uint8_t a_val)) : G7xxDecoder(others) {
    this->dec = dec;
    assert(this->dec!=nullptr);
  };

  size_t write(const void *in_ptr, size_t in_size) override {
    LOGD("write: %d", in_size);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    // decode bytes
    uint8_t *p_8 = (uint8_t *)in_ptr;
    for (int j = 0; j < in_size; j++) {
	      int16_t result = dec(p_8[j]);
        p_print->write((uint8_t*)&result,sizeof(int16_t));
    }
    return in_size;
  }
  protected:
  int (*dec)(uint8_t a_val)=nullptr;
};


/**
 * @brief 64 kbit/s  g711 ALOW Encoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G711_ALAWEncoder : public G711Encoder {
  public:
  G711_ALAWEncoder() : G711Encoder(linear2alaw) {};
};

/**
 * @brief 64 kbit/s  g711 ALOW Decoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G711_ALAWDecoder : public G711Decoder {
  public:
  G711_ALAWDecoder() : G711Decoder(alaw2linear) {};
};

/**
 * @brief 64 kbit/s  g711 ULOW Encoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G711_ULAWEncoder : public G711Encoder {
  public:
  G711_ULAWEncoder() : G711Encoder(linear2ulaw) {};
};

/**
 * @brief 64 kbit/s  g711 ULOW Decoder based on https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G711_ULAWDecoder : public G711Decoder {
  public:
  G711_ULAWDecoder() : G711Decoder(ulaw2linear) {};
};

}  // namespace audio_tools