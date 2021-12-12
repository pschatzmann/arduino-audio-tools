#pragma once

#include "AudioKit.h"
#include "AudioTools/AudioActions.h"

#define KEY_RESPONSE_TIME_MS 10

namespace audio_tools {

class AudioKitStream;
AudioKitStream *pt_AudioKitStream = nullptr;

/**
 * @brief AudioKit Stream which uses the
 * https://github.com/pschatzmann/arduino-audiokit library
 *
 */
class AudioKitStream : public AudioStreamX {
  AudioKitStream() { pt_AudioKitStream = this; }

  AudioKitConfig defaultConfig(RxTxMode mode = TX_MODE) {
    return kit.defaultConfig(mode == TX_MODE ? AudioOutput : AudioInput );
  }

  AudioKitConfig defaultConfig(AudioKitInOut inout=AudioInputOutput) {
    return kit.defaultConfig(inout);
  }

  void begin(AudioKitConfig config) {
    LOGD(LOG_METHOD);
    cfg = config;
    kit.begin(cfg);
    setVolume(volume_value);
  }

  void end() { kit.end(); }

  virtual size_t write(const uint8_t *buffer, size_t size) override {
    LOGD(LOG_METHOD);
    return kit.write(buffer, size);
  }

  /// Reads the audio data
  virtual size_t readBytes(uint8_t *data, size_t length) override {
    return kit.read(data, length);
  }

  AudioKitConfig config() { return cfg; }

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
    static unsigned long keys_timeout = 0;
    if (keys_timeout < millis()) {
      // LOGD(LOG_METHOD);
      actions.processActions();
      keys_timeout = millis() + KEY_RESPONSE_TIME_MS;
    }
    yield();
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
  AudioKitConfig cfg;
  AudioActions actions;
  int volume_value = 20;
  bool active = true;

  /**
   * @brief Setup the supported default actions
   *
   */
  void setupActions() {
    LOGI(LOG_METHOD);
    actions.add(kit.pinPaEnable(), actionStartStop);
    actions.add(kit.pinVolumeDown(), actionVolumeDown);
    actions.add(kit.pinVolumeUp(), actionVolumeUp);
  }
};

}  // namespace audio_tools
