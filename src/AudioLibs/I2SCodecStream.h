#pragma once
#include "AudioBoard.h"  // install audio-driver library
#include "AudioConfig.h"
#include "AudioI2S/I2SStream.h"

//#pragma GCC diagnostic ignored "-Wclass-memaccess"

// Added to be compatible with the AudioKitStream.h
#ifndef PIN_AUDIO_KIT_SD_CARD_CS
#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK 14
#endif

namespace audio_tools {

/**
 * @brief  Configuration for I2SCodecStream
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct I2SCodecConfig : public I2SConfig {
  input_device_t input_device = ADC_INPUT_LINE1;
  output_device_t output_device = DAC_OUTPUT_ALL;
  // to be compatible with the AudioKitStream -> do not activate SD spi if false
  bool sd_active = true;

  bool operator==(I2SCodecConfig alt) {
    return input_device == alt.input_device &&
           output_device == alt.output_device && *((AudioInfo *)this) == alt;
  }

  bool operator!=(I2SCodecConfig alt) { return !(*this == alt); }
};

/**
 * @brief  I2S Stream which also sets up a codec chip and i2s
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SCodecStream : public AudioStream, public VolumeSupport {
 public:
  /// Default Constructor (w/o codec)
  I2SCodecStream() = default;
  /**
   * @brief Default constructor: for available AudioBoard values check
   * audioboard variables in
   * https://pschatzmann.github.io/arduino-audio-driver/html/group__audio__driver.html
   * Further information can be found in
   * https://github.com/pschatzmann/arduino-audio-driver/wiki
   */
  I2SCodecStream(AudioBoard &board) { setBoard(board); }
  /// Provide board via pointer
  I2SCodecStream(AudioBoard *board) { setBoard(board); }

  /// Provides the default configuration
  I2SCodecConfig defaultConfig(RxTxMode mode = TX_MODE) {
    auto cfg1 = i2s.defaultConfig(mode);
    I2SCodecConfig cfg;
    memcpy(&cfg, &cfg1, sizeof(cfg1));
    cfg.input_device = ADC_INPUT_LINE1;
    cfg.output_device = DAC_OUTPUT_ALL;
    cfg.sd_active = true;
    return cfg;
  }

  bool begin() {
    TRACED();
    return begin(cfg);
  }

  /// Starts the I2S interface
  virtual bool begin(I2SCodecConfig cfg) {
    TRACED();
    this->cfg = cfg;
    return begin1();
  }

  /// Stops the I2S interface
  void end() {
    TRACED();
    if (p_board) p_board->end();
    i2s.end();
    is_active = false;
  }

  /// updates the sample rate dynamically
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    AudioStream::setAudioInfo(info);
    i2s.setAudioInfo(info);

    cfg.sample_rate = info.sample_rate;
    cfg.bits_per_sample = info.bits_per_sample;
    cfg.channels = info.channels;

    // update codec_cfg
    codec_cfg.i2s.bits = toCodecBits(cfg.bits_per_sample);
    codec_cfg.i2s.rate = toRate(cfg.sample_rate);

    // return if we we are not ready
    if (!is_active || p_board == nullptr) {
      return;
    }

    // return if there is nothing to do
    if (cfg.sample_rate == info.sample_rate &&
        cfg.bits_per_sample == info.bits_per_sample &&
        cfg.channels == info.channels) {
      return;
    }

    // update cfg
    p_board->setConfig(codec_cfg);
  }

  /// Writes the audio data to I2S
  virtual size_t write(const uint8_t *data, size_t len) {
    LOGD("I2SStream::write: %d", len);
    return i2s.write(data, len);
  }

  /// Reads the audio data
  virtual size_t readBytes(uint8_t *data, size_t len) override {
    return i2s.readBytes(data, len);
  }

  /// Provides the available audio data
  virtual int available() override { return i2s.available(); }

  /// Provides the available audio data
  virtual int availableForWrite() override { return i2s.availableForWrite(); }

  /// sets the volume (range 0.0f - 1.0f)
  bool setVolume(float vol) override {
    VolumeSupport::setVolume(vol);
    if (!is_active || p_board == nullptr) return false;
    return p_board->setVolume(vol * 100.0);
  }

  /// Provides the actual volume (0.0f - 1.0f)
  float volume() override {
    if (p_board == nullptr) return 0.0f;
    return static_cast<float>(p_board->getVolume()) / 100.0f;
  }
  /// legacy: same as volume()
  float getVolume()  { return volume(); }

  /// Mute / unmote
  bool setMute(bool mute) {
    if (p_board == nullptr) return false;
    return p_board->setMute(mute);
  }
  /// Mute / unmute of an individual line (codec)
  bool setMute(bool mute, int line) {
    if (p_board == nullptr) return false;
    return p_board->setMute(mute, line);
  }

  /// Sets the output of the PA Power Pin
  bool setPAPower(bool active) {
    if (p_board == nullptr) return false;
    return p_board->setPAPower(active);
  }

  /// Provides the board
  AudioBoard &board() { return *p_board; }
  /// (re)defines the board
  void setBoard(AudioBoard &board) { p_board = &board; }
  /// (re)defines the board
  void setBoard(AudioBoard *board) { p_board = board; }
  /// checks if a board has been defined
  bool hasBoard() { return p_board != nullptr; }

  /// Provides the gpio for the indicated function
  GpioPin getPinID(PinFunction function) {
    if (p_board == nullptr) return -1;
    return p_board->getPins().getPinID(function);
  }

  /// Provides the gpio for the indicated function
  GpioPin getPinID(PinFunction function, int pos) {
    if (p_board == nullptr) return -1;
    return p_board->getPins().getPinID(function, pos);
  }

  /// Provides the gpio for the indicated key pos
  GpioPin getKey(int pos) { return getPinID(PinFunction::KEY, pos); }

  /// Provides access to the pin information
  DriverPins &getPins() { return p_board->getPins(); }

  /// Provides the i2s driver
  I2SDriver *driver() { return i2s.driver(); }

 protected:
  I2SStream i2s;
  I2SCodecConfig cfg;
  CodecConfig codec_cfg;
  AudioBoard *p_board = nullptr;
  bool is_active = false;

  bool begin1() {
    setupI2SPins();
    if (!beginCodec(cfg)) {
      TRACEE();
      is_active = false;
      return false;
    }
    is_active = i2s.begin(cfg);

    // if setvolume was called before begin
    if (is_active && volume() >= 0.0f) {
      setVolume(volume());
    }
    return is_active;
  }


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
      switch (cfg.rx_tx_mode) {
        case RX_MODE:
          cfg.pin_data = i2s_pins.data_in;
          break;
        case TX_MODE:
          cfg.pin_data = i2s_pins.data_out;
          break;
        default:
          cfg.pin_data = i2s_pins.data_out;
          cfg.pin_data_rx = i2s_pins.data_in;
          break;
      }
    }
  }

  bool beginCodec(I2SCodecConfig info) {
    switch (cfg.rx_tx_mode) {
      case RX_MODE:
        codec_cfg.input_device = info.input_device;
        codec_cfg.output_device = DAC_OUTPUT_NONE;
        break;
      case TX_MODE:
        codec_cfg.output_device = info.output_device;
        codec_cfg.input_device = ADC_INPUT_NONE;
        break;
      default:
        codec_cfg.input_device = info.input_device;
        codec_cfg.output_device = info.output_device;
        break;
    }
    codec_cfg.sd_active = info.sd_active;
    LOGD("input: %d", info.input_device);
    LOGD("output: %d", info.output_device);
    codec_cfg.i2s.bits = toCodecBits(info.bits_per_sample);
    codec_cfg.i2s.rate = toRate(info.sample_rate);
    codec_cfg.i2s.fmt = toFormat(info.i2s_format);
    // use reverse logic for codec setting
    codec_cfg.i2s.mode = info.is_master ? MODE_SLAVE : MODE_MASTER;
    if (p_board == nullptr) return false;

    // setup driver only on changes
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
      LOGD("RATE_8K");
      return RATE_8K;
    }
    if (rate <= 11000) {
      LOGD("RATE_11K");
      return RATE_11K;
    }
    if (rate <= 16000) {
      LOGD("RATE_16K");
      return RATE_16K;
    }
    if (rate <= 22050) {
      LOGD("RATE_22K");
      return RATE_22K;
    }
    if (rate <= 32000) {
      LOGD("RATE_32K");
      return RATE_32K;
    }
    if (rate <= 44100) {
      LOGD("RATE_44K");
      return RATE_44K;
    }
    if (rate <= 48000 || rate > 48000) {
      LOGD("RATE_48K");
      return RATE_44K;
    }
    LOGE("Invalid rate: %d using 44K", rate);
    return RATE_44K;
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

}  // namespace audio_tools