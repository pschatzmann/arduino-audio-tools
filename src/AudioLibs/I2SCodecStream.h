#pragma once
#include "AudioBoard.h" // install audio-driver library
#include "AudioConfig.h"
#include "AudioI2S/I2SStream.h"

namespace audio_tools {

struct I2SCodecConfig : public I2SConfig {
  adc_input_t adc_input = ADC_INPUT_LINE1;
  dac_output_t dac_output = DAC_OUTPUT_ALL;
  // to be compatible with the AudioKitStream -> do not activate SD spi if false
  bool sd_active = true;
};

/**
 * @brief  I2S Stream which also sets up a codec chip
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SCodecStream : public AudioStream {
public:
  /// Default Constructor (w/o codec)
  I2SCodecStream() = default;

  I2SCodecStream(AudioBoard &board) { setBoard(board); }
  /// Provide board via pointer
  I2SCodecStream(AudioBoard *board) { setBoard(board); }

  /// Provides the default configuration
  I2SCodecConfig defaultConfig(RxTxMode mode = TX_MODE) {
    auto cfg1 = i2s.defaultConfig(mode);
    I2SCodecConfig cfg;
    memcpy(&cfg, &cfg1, sizeof(cfg1));
    cfg.adc_input = ADC_INPUT_LINE1;
    cfg.dac_output = DAC_OUTPUT_ALL;
    return cfg;
  }

  bool begin() {
    setupI2SPins();
    if (!beginCodec(cfg)) {
      TRACEE()
    }
    return i2s.begin(cfg);
  }

  /// Starts the I2S interface
  virtual bool begin(I2SCodecConfig cfg) {
    TRACED();
    this->cfg = cfg;
    return begin();
  }

  /// Stops the I2S interface
  void end() {
    TRACED();
    if (p_board)
      p_board->end();
    i2s.end();
  }

  /// updates the sample rate dynamically
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    AudioStream::setAudioInfo(info);
    beginCodec(info);
    i2s.setAudioInfo(info);
  }

  /// Writes the audio data to I2S
  virtual size_t write(const uint8_t *buffer, size_t size) {
    LOGD("I2SStream::write: %d", size);
    return i2s.write(buffer, size);
  }

  /// Reads the audio data
  virtual size_t readBytes(uint8_t *data, size_t length) override {
    return i2s.readBytes(data, length);
  }

  /// Provides the available audio data
  virtual int available() override { return i2s.available(); }

  /// Provides the available audio data
  virtual int availableForWrite() override { return i2s.availableForWrite(); }

  /// sets the volume (range 0.0 - 1.0)
  bool setVolume(float vol) {
    if (p_board == nullptr)
      return false;
    return p_board->setVolume(vol * 100.0);
  }
  int getVolume() {
    if (p_board == nullptr)
      return -1;
    return p_board->getVolume();
  }
  bool setMute(bool mute) {
    if (p_board == nullptr)
      return false;
    return p_board->setMute(mute);
  }
  bool setPAPower(bool active) {
    if (p_board == nullptr)
      return false;
    return p_board->setPAPower(active);
  }

  AudioBoard &board() { return *p_board; }
  void setBoard(AudioBoard &board) { p_board = &board; }
  void setBoard(AudioBoard *board) { p_board = board; }
  bool hasBoard() { return p_board != nullptr; }

  Pin getPin(PinFunctionEnum function) {
    if (p_board == nullptr)
      return -1;
    return p_board->getPins().getPin(function);
  }

  Pin getPin(PinFunctionEnum function, int pos) {
    if (p_board == nullptr)
      return -1;
    return p_board->getPins().getPin(function, pos);
  }

  DriverPins &getPins() { return p_board->getPins(); }

protected:
  I2SStream i2s;
  I2SCodecConfig cfg;
  AudioBoard *p_board = nullptr;

  /// We use the board pins if they are available
  void setupI2SPins() {
    // setup pins in i2s config
    auto i2s = p_board->getPins().getI2SPins();
    if (i2s) {
      // determine i2s pins from board definition
      PinsI2S i2s_pins = i2s.value();
      cfg.pin_bck = i2s_pins.bck;
      cfg.pin_mck = i2s_pins.mclk;
      cfg.pin_ws = i2s_pins.ws;
      cfg.pin_data = i2s_pins.data_out;
      cfg.pin_data_rx = i2s_pins.data_in;
    }
  }

  bool beginCodec(AudioInfo info) {
    cfg.sample_rate = info.sample_rate;
    cfg.bits_per_sample = info.bits_per_sample;
    cfg.channels = info.channels;
    return beginCodec(cfg);
  }

  bool beginCodec(I2SCodecConfig info) {
    CodecConfig codec_cfg;
    codec_cfg.sd_active = info.sd_active;
    codec_cfg.adc_input = info.adc_input;
    LOGD("input: %d", info.adc_input);
    codec_cfg.dac_output = info.dac_output;
    LOGD("output: %d", info.dac_output);
    codec_cfg.i2s.bits = toCodecBits(info.bits_per_sample);
    codec_cfg.i2s.rate = toRate(info.sample_rate);
    codec_cfg.i2s.fmt = toFormat(info.i2s_format);
    // use reverse logic for codec setting
    codec_cfg.i2s.mode = info.is_master ? MODE_SLAVE : MODE_MASTER;
    if (p_board == nullptr)
      return false;

    return p_board->begin(codec_cfg);
  }

  sample_bits_t toCodecBits(int bits) {
    switch (bits) {
    case 16:
      LOGD("BIT_LENGTH_16BITS");
      return BIT_LENGTH_16BITS;
    case 24:
      LOGD("BIT_LENGTH_24BITS");
      return BIT_LENGTH_24BITS;
    case 32:
      LOGD("BIT_LENGTH_32BITS");
      return BIT_LENGTH_32BITS;
    }
    LOGE("Unsupported bits: %d", bits);
    return BIT_LENGTH_16BITS;
  }
  samplerate_t toRate(int rate) {
    if (rate <= 8000) {
      LOGD("RATE_08K");
      return RATE_08K;
    }
    if (rate <= 11000) {
      LOGD("RATE_08K");
      return RATE_11K;
    }
    if (rate <= 16000) {
      LOGD("RATE_08K");
      return RATE_16K;
    }
    if (rate <= 22000) {
      LOGD("RATE_08K");
      return RATE_22K;
    }
    if (rate <= 32000) {
      LOGD("RATE_08K");
      return RATE_32K;
    }
    if (rate <= 44000) {
      LOGD("RATE_08K");
      return RATE_44K;
    }
    LOGD("RATE_08K");
    return RATE_48K;
  }

  i2s_format_t toFormat(I2SFormat fmt) {
    switch (fmt) {
    case I2S_PHILIPS_FORMAT:
    case I2S_STD_FORMAT:
      LOGD("I2S_NORMAL");
      return I2S_NORMAL;
    case I2S_LEFT_JUSTIFIED_FORMAT:
    case I2S_MSB_FORMAT:
      LOGD("I2S_LEFT");
      return I2S_LEFT;
    case I2S_RIGHT_JUSTIFIED_FORMAT:
    case I2S_LSB_FORMAT:
      LOGD("I2S_RIGHT");
      return I2S_RIGHT;
    case I2S_PCM:
      LOGD("I2S_DSP");
      return I2S_DSP;
    default:
      LOGE("unsupported mode");
      return I2S_NORMAL;
    }
  }
};

} // namespace audio_tools