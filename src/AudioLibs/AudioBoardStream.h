#pragma once

#include "AudioConfig.h"
#include "AudioLibs/I2SCodecStream.h"
#include "AudioTools/AudioActions.h"

namespace audio_tools {
class AudioBoardStream;
AudioBoardStream *selfAudioBoard = nullptr;

/**
 * New functionality which replaces the AudioKitStream that is based on the
 * legicy AudioKit library. This functionality uses the new arduino-audio-driver
 * library! It is the same as I2SCodecStream extended by some AudioActoins and
 * some method calls to determine pin values.
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioBoardStream : public I2SCodecStream {
public:
  AudioBoardStream(audio_driver::AudioBoard &board) : I2SCodecStream(board) {
    selfAudioBoard = this;
  }

  /**
   * @brief Process input keys and pins
   *
   */
  void processActions() {
    //  TRACED();
    actions.processActions();
    yield();
  }

  /**
   * @brief Defines a new action that is executed when the indicated pin is
   * active
   *
   * @param pin
   * @param action
   * @param ref
   */
  void addAction(int pin, void (*action)(bool, int, void *),
                 void *ref = nullptr) {
    TRACEI();
    // determine logic from config
    AudioActions::ActiveLogic activeLogic = getActionLogic(pin);
    actions.add(pin, action, activeLogic, ref);
  }

  /**
   * @brief Defines a new action that is executed when the indicated pin is
   * active
   *
   * @param pin
   * @param action
   * @param activeLogic
   * @param ref
   */
  void addAction(int pin, void (*action)(bool, int, void *),
                 AudioActions::ActiveLogic activeLogic, void *ref = nullptr) {
    TRACEI();
    actions.add(pin, action, activeLogic, ref);
  }

  /// Provides access to the AudioActions
  AudioActions &audioActions() { return actions; }

  /**
   * @brief Relative volume control
   *
   * @param vol
   */
  void incrementVolume(int vol) {
    volume_value += vol;
    LOGI("incrementVolume: %d -> %d", vol, volume_value);
    setVolume(volume_value);
  }

  /**
   * @brief Increase the volume
   *
   */
  static void actionVolumeUp(bool, int, void *) {
    TRACEI();
    selfAudioBoard->incrementVolume(+2);
  }

  /**
   * @brief Decrease the volume
   *
   */
  static void actionVolumeDown(bool, int, void *) {
    TRACEI();
    selfAudioBoard->incrementVolume(-2);
  }

  /**
   * @brief Toggle start stop
   *
   */
  static void actionStartStop(bool, int, void *) {
    TRACEI();
    selfAudioBoard->active = !selfAudioBoard->active;
    selfAudioBoard->setActive(selfAudioBoard->active);
  }

  /**
   * @brief Start
   *
   */
  static void actionStart(bool, int, void *) {
    TRACEI();
    selfAudioBoard->active = true;
    selfAudioBoard->setActive(selfAudioBoard->active);
  }

  /**
   * @brief Stop
   */
  static void actionStop(bool, int, void *) {
    TRACEI();
    selfAudioBoard->active = false;
    selfAudioBoard->setActive(selfAudioBoard->active);
  }

  /**
   * @brief Switch off the PA if the headphone in plugged in
   * and switch it on again if the headphone is unplugged.
   * This method complies with the
   */
  static void actionHeadphoneDetection() {
    if (selfAudioBoard->pinHeadphoneDetect() >= 0) {

      // detect changes
      bool isConnected = selfAudioBoard->headphoneStatus();
      if (selfAudioBoard->headphoneIsConnected != isConnected) {
        selfAudioBoard->headphoneIsConnected = isConnected;

        // update if things have stabilized
        bool powerActive = !isConnected;
        LOGW("Headphone jack has been %s",
             isConnected ? "inserted" : "removed");
        selfAudioBoard->setSpeakerActive(powerActive);
      }
    }
    yield();
  }

  /**
   * @brief  Get the gpio number for auxin detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  Pin pinAuxin() { return getPin(AUXIN_DETECT); }

  /**
   * @brief  Get the gpio number for headphone detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  Pin pinHeadphoneDetect() { return getPin(HEADPHONE_DETECT); }

  /**
   * @brief  Get the gpio number for PA enable
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  Pin pinPaEnable() { return getPin(PA); }

  //   /**
  //    * @brief  Get the gpio number for adc detection
  //    *
  //    * @return  -1      non-existent
  //    *          Others  gpio number
  //    */
  //   Pin pinAdcDetect() { return getPin(AUXIN_DETECT); }

  /**
   * @brief  Get the record-button id for adc-button
   *
   * @return  -1      non-existent
   *          Others  button id
   */
  Pin pinInputRec() { return getPin(KEY, 1); }

  /**
   * @brief  Get the number for mode-button
   *
   * @return  -1      non-existent
   *          Others  number
   */
  Pin pinInputMode() { return getPin(KEY, 2); }

  /**
   * @brief Get number for set function
   *
   * @return -1       non-existent
   *         Others   number
   */
  Pin pinInputSet() { return getPin(KEY, 4); }

  /**
   * @brief Get number for play function
   *
   * @return -1       non-existent
   *         Others   number
   */
  Pin pinInputPlay() { return getPin(KEY, 3); }

  /**
   * @brief number for volume up function
   *
   * @return -1       non-existent
   *         Others   number
   */
  Pin pinVolumeUp() { return getPin(KEY, 6); }

  /**
   * @brief Get number for volume down function
   *
   * @return -1       non-existent
   *         Others   number
   */
  Pin pinVolumeDown() { return getPin(KEY, 5); }

  /**
   * @brief Get LED pin
   *
   * @return -1       non-existent
   *         Others   gpio number
   */
  Pin pinLed(int idx) { return getPin(LED, idx); }

  /// the same as setPAPower()
  void setSpeakerActive(bool active) { setPAPower(active); }

  /**
   * @brief Returns true if the headphone was detected
   *
   * @return true
   * @return false
   */
  bool headphoneStatus() {
    int headphonePin = pinHeadphoneDetect();
    return headphonePin > 0 ? !digitalRead(headphonePin) : false;
  }

  /**
   * @brief The oposite of setMute(): setActive(true) calls setMute(false)
   */
  void setActive(bool active) { setMute(!active); }

  /// Inform this object that SD is active
  void setSDActive(bool active) { sd_active = active; }

protected:
  AudioActions actions;
  int volume_value = 40;
  bool headphoneIsConnected = false;
  bool active = true;
  bool sd_active = false;

  /// Determines the action logic (ActiveLow or ActiveTouch) for the pin
  AudioActions::ActiveLogic getActionLogic(int pin) {
    auto opt = board().getPins().getPin(pin);
    PinLogic logic = PinLogic::Input;
    if (opt)
      logic = opt.value().pin_logic;
    switch (logic) {
    case PinLogic::Input:
    case PinLogic::InputActiveLow:
      return AudioActions::ActiveLow;
    case PinLogic::InputActiveHigh:
      return AudioActions::ActiveHigh;
    case PinLogic::InputActiveTouch:
      return AudioActions::ActiveTouch;
    default:
      return AudioActions::ActiveLow;
    }
  }

  /// Setup the supported default actions
  void setupActions() {
    TRACEI();
    Pin sd_cs = -1;
    auto sd_opt = getPins().spi(SD);
    if (sd_opt) {
      sd_cs = sd_opt.value().cs)
    }
    // pin conflicts for pinInputMode() with the SD CS pin for AIThinker and
    // buttons
    if (pinInputMode() != sd_cs || !sd_active)
      addAction(pinInputMode(), actionStartStop);

    // pin conflicts with AIThinker A101: key6 and headphone detection
    if (getPin(KEY, 6) != pinHeadphoneDetect()) {
      actions.add(pinHeadphoneDetect(), actionHeadphoneDetection,
                  AudioActions::ActiveChange);
    }

    // pin conflicts with SD Lyrat SD CS Pin and buttons / Conflict on Audiokit
    // V. 2957
    if (!sd_active || (pinVolumeDown()!=sd_cs && pinVolumeUp()!=sd_cs ) ) {
      LOGD("actionVolumeDown")
      addAction(pinVolumeDown(), actionVolumeDown);
      LOGD("actionVolumeUp")
      addAction(pinVolumeUp(), actionVolumeUp);
    } else {
      LOGW("Volume Buttons ignored because of conflict: %d ",
           pinVolumeDown());
    }
  }
};

} // namespace audio_tools
