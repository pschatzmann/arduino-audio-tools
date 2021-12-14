#pragma once

#include "AudioKit.h"
#include "AudioTools/AudioActions.h"

#define KEY_RESPONSE_TIME_MS 10

namespace audio_tools {

class AudioKitStream;
AudioKitStream *pt_AudioKitStream = nullptr;

/**
 * @brief Configuration for AudioKitStream: we use as subclass of I2SConfig
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioKitStreamConfig : public I2SConfig {
 public:
  AudioKitStreamConfig() = default;
  // set adc channel with audio_hal_adc_input_t
  audio_hal_adc_input_t input_device = AUDIOKIT_DEFAULT_INPUT;
  // set dac channel 
  audio_hal_dac_output_t output_device = AUDIOKIT_DEFAULT_OUTPUT;
  int masterclock_pin = 0;

  /// convert to config object needed by HAL
  AudioKitConfig toAudioKitConfig() {
    LOGD(LOG_METHOD);
    AudioKitConfig result;
    result.i2s_num = (i2s_port_t)port_no;
    result.mclk_gpio = (gpio_num_t)masterclock_pin;
    result.adc_input = input_device;
    result.dac_output = output_device;
    result.codec_mode = toCodecMode();
    result.master_slave_mode = toMode();
    result.fmt = toFormat();
    result.sample_rate = toSampleRate();
    result.bits_per_sample = toBits();
    return result;
  }

 protected:
  // convert to audio_hal_iface_samples_t
  audio_hal_iface_bits_t toBits() {
    LOGD(LOG_METHOD);
    const static int ia[] = {16, 24, 32};
    const static audio_hal_iface_bits_t oa[] = {AUDIO_HAL_BIT_LENGTH_16BITS,
                                                AUDIO_HAL_BIT_LENGTH_24BITS,
                                                AUDIO_HAL_BIT_LENGTH_32BITS};
    for (int j = 0; j < 3; j++) {
      if (ia[j] == bits_per_sample) {
        LOGD("-> %d",ia[j])
        return oa[j];
      }
    }
    LOGE("Bits per sample not supported: %d", bits_per_sample);
    return AUDIO_HAL_BIT_LENGTH_16BITS;
  }

  /// Convert to audio_hal_iface_samples_t
  audio_hal_iface_samples_t toSampleRate() {
    LOGD(LOG_METHOD);
    const static int ia[] = {8000,  11025, 16000, 22050,
                             24000, 32000, 44100, 48000};
    const static audio_hal_iface_samples_t oa[] = {
        AUDIO_HAL_08K_SAMPLES, AUDIO_HAL_11K_SAMPLES, AUDIO_HAL_16K_SAMPLES,
        AUDIO_HAL_22K_SAMPLES, AUDIO_HAL_24K_SAMPLES, AUDIO_HAL_32K_SAMPLES,
        AUDIO_HAL_44K_SAMPLES, AUDIO_HAL_48K_SAMPLES};
    int diff = 99999;
    int result = 0;
    for (int j = 0; j < 8; j++) {
      if (ia[j] == sample_rate) {
        LOGD("-> %d",ia[j])
        return oa[j];
      } else {
        int new_diff = abs(oa[j] - sample_rate);
        if (new_diff < diff) {
          result = j;
          diff = new_diff;
        }
      }
    }
    LOGE("Sample Rate not supported: %d - using %d", sample_rate, ia[result]);
    return oa[result];
  }

  /// Convert to audio_hal_iface_format_t
  audio_hal_iface_format_t toFormat() {
    LOGD(LOG_METHOD);
    const static int ia[] = {I2S_STD_FORMAT,
                             I2S_LSB_FORMAT,
                             I2S_MSB_FORMAT,
                             I2S_PHILIPS_FORMAT,
                             I2S_RIGHT_JUSTIFIED_FORMAT,
                             I2S_LEFT_JUSTIFIED_FORMAT,
                             I2S_PCM_LONG,
                             I2S_PCM_SHORT};
    const static audio_hal_iface_format_t oa[] = {
        AUDIO_HAL_I2S_NORMAL, AUDIO_HAL_I2S_LEFT,  AUDIO_HAL_I2S_RIGHT,
        AUDIO_HAL_I2S_NORMAL, AUDIO_HAL_I2S_RIGHT, AUDIO_HAL_I2S_LEFT,
        AUDIO_HAL_I2S_DSP,    AUDIO_HAL_I2S_DSP};
    for (int j = 0; j < 8; j++) {
      if (ia[j] == i2s_format) {
        LOGD("-> %d",j)
        return oa[j];
      }
    }
    LOGE("Format not supported: %d", i2s_format);
    return AUDIO_HAL_I2S_NORMAL;
  }

  /// Determine if ESP32 is master or slave - this is just the oposite of the
  /// HAL device
  audio_hal_iface_mode_t toMode() {
    return (is_master) ? AUDIO_HAL_MODE_SLAVE : AUDIO_HAL_MODE_MASTER;
  }

  /// Convert to audio_hal_codec_mode_t
  audio_hal_codec_mode_t toCodecMode() {
    switch (rx_tx_mode) {
      case TX_MODE:
        LOGD("-> %s","AUDIO_HAL_CODEC_MODE_DECODE");
        return AUDIO_HAL_CODEC_MODE_DECODE;
      case RX_MODE:
        LOGD("-> %s","AUDIO_HAL_CODEC_MODE_ENCODE");
        return AUDIO_HAL_CODEC_MODE_ENCODE;
      default:
        LOGD("-> %s","AUDIO_HAL_CODEC_MODE_BOTH");
        return AUDIO_HAL_CODEC_MODE_BOTH;
    }
  }
};

/**
 * @brief Converts a AudioKit into a AudioStream, so that we can pass it
 * to the converter
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioKitStreamAdapter : public AudioStreamX {
 public:
  AudioKitStreamAdapter(AudioKit *kit) { this->kit = kit; }
  size_t write(const uint8_t *data, size_t len) override {
//    LOGD(LOG_METHOD);
    return kit->write(data, len);
  }
  size_t readBytes(uint8_t *data, size_t len) override {
//    LOGD(LOG_METHOD);
    return kit->read(data, len);
  }

 protected:
  AudioKit *kit = nullptr;
};

/**
 * @brief AudioKit Stream which uses the
 * https://github.com/pschatzmann/arduino-audiokit library
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioKitStream : public AudioStreamX {
 public:
  AudioKitStream() { pt_AudioKitStream = this; }

  /// Provides the default configuration
  AudioKitStreamConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    LOGD(LOG_METHOD);
    AudioKitStreamConfig result;
    result.rx_tx_mode = mode;
    return result;
  }

  void begin(AudioKitStreamConfig config) {
    LOGD(LOG_METHOD);
    cfg = config;
    cfg.logInfo();
    kit.begin(cfg.toAudioKitConfig());

    // convert format if necessary
    converter.setInputInfo(cfg);
    output_config = cfg;
    output_config.channels = 2;
    LOGI("Channels %d->%d", cfg.channels, output_config.channels);
    converter.setInfo(output_config);

    // Volume control and headphone detection
    setupActions();
    // set initial volume
    setVolume(volume_value);
  }

  void end() {
    LOGD(LOG_METHOD);
    kit.end();
  }

  virtual size_t write(const uint8_t *buffer, size_t size) override {
//    LOGD("write: %zu",size);
    return converter.write(buffer, size);
  }

  /// Reads the audio data
  virtual size_t readBytes(uint8_t *data, size_t length) override {
    if (cfg.channels == 2) {
      return kit.read(data, length);
    }
    LOGE("Unsuported number of channels :%", cfg.channels);
    return 0;
  }

  /// Update the audio info with new values: e.g. new sample_rate,
  /// bits_per_samples or channels
  virtual void setAudioInfo(AudioBaseInfo info) {
    cfg.sample_rate = info.sample_rate;
    cfg.bits_per_sample = info.bits_per_sample;
    cfg.channels = info.channels;
    kit.begin(cfg.toAudioKitConfig());
    // update input format
    converter.setInputInfo(cfg);
  }

  AudioKitStreamConfig config() { return cfg; }

  /// Sets the codec active / inactive
  bool setActive(bool active) { return kit.setActive(active); }

  /// Mutes the output
  bool setMute(bool mute) { return kit.setMute(mute); }

  /// Defines the Volume
  bool setVolume(int vol) { return kit.setVolume(vol); }

  /// Determines the volume
  int volume() { return kit.volume(); }

  /**
   * @brief Process input keys and pins
   *
   */
  void processActions() {
//    LOGI(LOG_METHOD);
//    static unsigned long keys_timeout = 0;
//    if (keys_timeout < millis()) {
      // LOGD(LOG_METHOD);
      actions.processActions();
//      keys_timeout = millis() + KEY_RESPONSE_TIME_MS;
//    }
    delay(1);
  }

  /**
   * @brief Defines a new action that is executed when the indicated pin is
   * active
   *
   * @param pin
   * @param action
   */
  void addAction(int pin, void (*action)()) {
    LOGI(LOG_METHOD);
    actions.add(pin, action);
  }

  void incrementVolume(int vol) { volume_value += vol; }

  /**
   * @brief Increase the volume
   *
   */
  static void actionVolumeUp() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->incrementVolume(+2);
  }

  /**
   * @brief Decrease the volume
   *
   */
  static void actionVolumeDown() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->incrementVolume(-2);
  }

  /**
   * @brief Toggle start stop
   *
   */
  static void actionStartStop() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->active = !pt_AudioKitStream->active;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
  }

  /**
   * @brief Start
   *
   */
  static void actionStart() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->active = true;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
  }

  /**
   * @brief Stop
   *
   */
  static void actionStop() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->active = false;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
  }

  /**
   * @brief  Get the gpio number for auxin detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinAuxin() { return kit.pinAuxin(); }

  /**
   * @brief  Get the gpio number for headphone detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinHeadphoneDetect() { return kit.pinHeadphoneDetect(); }

  /**
   * @brief  Get the gpio number for PA enable
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinPaEnable() { return kit.pinPaEnable(); }

  /**
   * @brief  Get the gpio number for adc detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinAdcDetect() { return kit.pinAdcDetect(); }

  /**
   * @brief  Get the mclk gpio number of es7243
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  int8_t pinEs7243Mclk() { return kit.pinEs7243Mclk(); }

  /**
   * @brief  Get the record-button id for adc-button
   *
   * @return  -1      non-existent
   *          Others  button id
   */
  int8_t pinInputRec() { return kit.pinInputRec(); }

  /**
   * @brief  Get the number for mode-button
   *
   * @return  -1      non-existent
   *          Others  number
   */
  int8_t pinInputMode() { return kit.pinInputMode(); }

  /**
   * @brief Get number for set function
   *
   * @return -1       non-existent
   *         Others   number
   */
  int8_t pinInputSet() { return kit.pinInputSet(); };

  /**
   * @brief Get number for play function
   *
   * @return -1       non-existent
   *         Others   number
   */
  int8_t pinInputPlay() { return kit.pinInputPlay(); }

  /**
   * @brief number for volume up function
   *
   * @return -1       non-existent
   *         Others   number
   */
  int8_t pinVolumeUp() { return kit.pinVolumeUp(); }

  /**
   * @brief Get number for volume down function
   *
   * @return -1       non-existent
   *         Others   number
   */
  int8_t pinVolumeDown() { return kit.pinVolumeDown(); }

  /**
   * @brief Get green led gpio number
   *
   * @return -1       non-existent
   *        Others    gpio number
   */
  int8_t pinResetCodec() { return kit.pinResetCodec(); }

  /**
   * @brief Get DSP reset gpio number
   *
   * @return -1       non-existent
   *         Others   gpio number
   */
  int8_t pinResetBoard() { return kit.pinResetBoard(); }

  /**
   * @brief Get DSP reset gpio number
   *
   * @return -1       non-existent
   *         Others   gpio number
   */
  int8_t pinGreenLed() { return kit.pinGreenLed(); }

  /**
   * @brief Get green led gpio number
   *
   * @return -1       non-existent
   *         Others   gpio number
   */
  int8_t pinBlueLed() { return kit.pinBlueLed(); }

 protected:
  AudioKit kit;
  AudioKitStreamConfig cfg;
  AudioActions actions;
  int volume_value = 20;
  bool active = true;
  // channel and sample size conversion support
  AudioKitStreamAdapter kit_stream = AudioKitStreamAdapter(&kit);
  FormatConverterStream converter = FormatConverterStream(kit_stream);
  AudioBaseInfo output_config;

  /// Setup the supported default actions
  void setupActions() {
    LOGI(LOG_METHOD);
    actions.add(kit.pinHeadphoneDetect(), AudioKit::actionHeadphoneDetection);
    actions.add(kit.pinPaEnable(), actionStartStop);
    actions.add(kit.pinVolumeDown(), actionVolumeDown);
    actions.add(kit.pinVolumeUp(), actionVolumeUp);
  }
};

}  // namespace audio_tools
