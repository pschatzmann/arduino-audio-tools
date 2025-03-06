#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "AudioTools/CoreAudio/AudioActions.h"

namespace audio_tools {

/**
 * @brief New functionality which replaces the AudioKitStream that is based on
 * the legacy AudioKit library. This functionality uses the new
 * arduino-audio-driver library! It is the same as I2SCodecStream extended by
 * some AudioActions and some method calls to determine defined pin values.
 * See https://github.com/pschatzmann/arduino-audio-driver
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioBoardStream : public I2SCodecStream {
  struct AudioBoardAction : public AudioActions::Action {
    AudioBoardAction(AudioBoard &board, AudioDriverKey key) {
      this->key = key;
      this->p_board = &board;
    }
    AudioDriverKey key;
    AudioBoard *p_board;
    int id() override { return key | 0x400; }
    bool readValue() override { return p_board->isKeyPressed(key); }
  };

 public:
  /**
   * @brief Default constructor: for available AudioBoard values check
   * the audioboard variables in
   * https://pschatzmann.github.io/arduino-audio-driver/html/group__audio__driver.html
   * Further information can be found in
   * https://github.com/pschatzmann/arduino-audio-driver/wiki
   */
  AudioBoardStream(audio_driver::AudioBoard &board) : I2SCodecStream(board) {
    // pin mode already set up by driver library
    actions.setPinMode(false);
  }

  bool begin() override { return I2SCodecStream::begin(); }

  bool begin(I2SCodecConfig cfg) override { return I2SCodecStream::begin(cfg); }

  /**
   * @brief Process input keys and pins
   *
   */
  void processActions() {
    //  TRACED();
    actions.processActions();
    delay(1);
  }


  /**
   * @brief Defines a new action that is executed when the Button is pressed
   */
  void addAction(AudioDriverKey key, void (*action)(bool, int, void *),
                 void *ref = nullptr) {
      AudioBoardAction *abo = new AudioBoardAction(board(), key);
      abo->actionOn = action;
      abo->ref = (ref == nullptr) ? this : ref; 
      actions.add(*abo);            
  }

  /**
   * @brief Defines a new action that is executed when the Button is pressed and released
   */
  void addAction(AudioDriverKey key, void (*actionOn)(bool, int, void *),
                void (*actionOff)(bool, int, void *),
                void *ref = nullptr) {

      AudioBoardAction *abo = new AudioBoardAction(board(), key);
      abo->actionOn = actionOn;
      abo->actionOn = actionOff;
      abo->ref = (ref == nullptr) ? this : ref; 
      actions.add(*abo);            
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
    actions.add(pin, action, activeLogic, ref == nullptr ? this : ref);
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
    actions.add(pin, action, activeLogic, ref == nullptr ? this : ref);
  }

  /// Provides access to the AudioActions
  AudioActions &audioActions() { return actions; }

  AudioActions &getActions() { return actions; }

  /**
   * @brief Relative volume control
   *
   * @param vol
   */
  void incrementVolume(float inc) {
    float current_volume = getVolume();
    float new_volume = current_volume + inc;
    LOGI("incrementVolume: %f -> %f", current_volume, new_volume);
    setVolume(new_volume);
  }

  /**
   * @brief Increase the volume
   *
   */
  static void actionVolumeUp(bool, int, void *ref) {
    TRACEI();
    AudioBoardStream *self = (AudioBoardStream *)ref;
    self->incrementVolume(+self->actionVolumeIncrementValue());
  }

  /**
   * @brief Decrease the volume
   *
   */
  static void actionVolumeDown(bool, int, void *ref) {
    TRACEI();
    AudioBoardStream *self = (AudioBoardStream *)ref;
    self->incrementVolume(-self->actionVolumeIncrementValue());
  }

  /**
   * @brief Toggle start stop
   *
   */
  static void actionStartStop(bool, int, void *ref) {
    TRACEI();
    AudioBoardStream *self = (AudioBoardStream *)ref;
    self->active = !self->active;
    self->setActive(self->active);
  }

  /**
   * @brief Start
   *
   */
  static void actionStart(bool, int, void *ref) {
    TRACEI();
    AudioBoardStream *self = (AudioBoardStream *)ref;
    self->active = true;
    self->setActive(self->active);
  }

  /**
   * @brief Stop
   */
  static void actionStop(bool, int, void *ref) {
    TRACEI();
    AudioBoardStream *self = (AudioBoardStream *)ref;
    self->active = false;
    self->setActive(self->active);
  }

  /**
   * @brief Switch off the PA if the headphone in plugged in
   * and switch it on again if the headphone is unplugged.
   * This method complies with the
   */
  static void actionHeadphoneDetection(bool, int, void *ref) {
    AudioBoardStream *self = (AudioBoardStream *)ref;
    if (self->pinHeadphoneDetect() >= 0) {
      // detect changes
      bool isConnected = self->headphoneStatus();
      if (self->headphoneIsConnected != isConnected) {
        self->headphoneIsConnected = isConnected;

        // update if things have stabilized
        bool powerActive = !isConnected;
        LOGW("Headphone jack has been %s",
             isConnected ? "inserted" : "removed");
        self->setSpeakerActive(powerActive);
      }
    }
    delay(1);
  }

  /**
   * @brief  Get the gpio number for auxin detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  GpioPin pinAuxin() { return getPinID(PinFunction::AUXIN_DETECT); }

  /**
   * @brief  Get the gpio number for headphone detection
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  GpioPin pinHeadphoneDetect() {
    return getPinID(PinFunction::HEADPHONE_DETECT);
  }

  /**
   * @brief  Get the gpio number for PA enable
   *
   * @return  -1      non-existent
   *          Others  gpio number
   */
  GpioPin pinPaEnable() { return getPinID(PinFunction::PA); }

  //   /**
  //    * @brief  Get the gpio number for adc detection
  //    *
  //    * @return  -1      non-existent
  //    *          Others  gpio number
  //    */
  //   GpioPin pinAdcDetect() { return getPin(AUXIN_DETECT); }

  /**
   * @brief  Get the record-button id for adc-button
   *
   * @return  -1      non-existent
   *          Others  button id
   */
  GpioPin pinInputRec() { return getPinID(PinFunction::KEY, 1); }

  /**
   * @brief  Get the number for mode-button
   *
   * @return  -1      non-existent
   *          Others  number
   */
  GpioPin pinInputMode() { return getPinID(PinFunction::KEY, 2); }

  /**
   * @brief Get number for set function
   *
   * @return -1       non-existent
   *         Others   number
   */
  GpioPin pinInputSet() { return getPinID(PinFunction::KEY, 4); }

  /**
   * @brief Get number for play function
   *
   * @return -1       non-existent
   *         Others   number
   */
  GpioPin pinInputPlay() { return getPinID(PinFunction::KEY, 3); }

  /**
   * @brief number for volume up function
   *
   * @return -1       non-existent
   *         Others   number
   */
  GpioPin pinVolumeUp() { return getPinID(PinFunction::KEY, 6); }

  /**
   * @brief Get number for volume down function
   *
   * @return -1       non-existent
   *         Others   number
   */
  GpioPin pinVolumeDown() { return getPinID(PinFunction::KEY, 5); }

  /**
   * @brief Get LED pin
   *
   * @return -1       non-existent
   *         Others   gpio number
   */
  GpioPin pinLed(int idx) { return getPinID(PinFunction::LED, idx); }

  /// the same as setPAPower()
  void setSpeakerActive(bool active) { setPAPower(active); }

  /**
   * @brief Returns true if the headphone was detected
   *
   * @return true
   * @return false
   */
  bool headphoneStatus() {
    int headphoneGpioPin = pinHeadphoneDetect();
    return headphoneGpioPin > 0 ? !digitalRead(headphoneGpioPin) : false;
  }

  /**
   * @brief The oposite of setMute(): setActive(true) calls setMute(false)
   */
  void setActive(bool active) { setMute(!active); }

  /// add start/stop on inputMode
  void addStartStopAction() {
    // pin conflicts for pinInputMode() with the SD CS pin for AIThinker and
    // buttons
    int sd_cs = getSdCsPin();
    int input_mode = pinInputMode();
    if (input_mode != -1 && (input_mode != sd_cs || !cfg.sd_active)) {
      LOGD("actionInputMode")
      addAction(input_mode, actionStartStop);
    }
  }

  /// add volume up and volume down action
  void addVolumeActions() {
    // pin conflicts with SD Lyrat SD CS GpioPin and buttons / Conflict on
    // Audiokit V. 2957
    int sd_cs = getSdCsPin();
    int vol_up = pinVolumeUp();
    int vol_down = pinVolumeDown();
    if ((vol_up != -1 && vol_down != -1) &&
        (!cfg.sd_active || (vol_down != sd_cs && vol_up != sd_cs))) {
      LOGD("actionVolumeDown")
      addAction(vol_down, actionVolumeDown);
      LOGD("actionVolumeUp")
      addAction(vol_up, actionVolumeUp);
    } else {
      LOGW("Volume Buttons ignored because of conflict: %d ", pinVolumeDown());
    }
  }

  /// Adds headphone determination
  void addHeadphoneDetectionAction() {
    // pin conflicts with AIThinker A101: key6 and headphone detection
    int head_phone = pinHeadphoneDetect();
    if (head_phone != -1 && (getPinID(PinFunction::KEY, 6) != head_phone)) {
      actions.add(head_phone, actionHeadphoneDetection,
                  AudioActions::ActiveChange, this);
    }
  }

  /**
   * @brief Setup the supported default actions (volume, start/stop, headphone
   * detection)
   */
  void addDefaultActions() {
    TRACEI();
    addHeadphoneDetectionAction();
    addStartStopAction();
    addVolumeActions();
  }

  /// Defines the increment value used by actionVolumeDown/actionVolumeUp
  void setActionVolumeIncrementValue(float value) {
    action_increment_value = value;
  }

  float actionVolumeIncrementValue() { return action_increment_value; }

  bool isKeyPressed(int key) {
    if (!board()) return false;
    return board().isKeyPressed(key);
  }

 protected:
  AudioActions actions;
  bool headphoneIsConnected = false;
  bool active = true;
  float action_increment_value = 0.02;

  int getSdCsPin() {
    static GpioPin sd_cs = -2;
    // execute only once
    if (sd_cs != -2) return sd_cs;

    auto sd_opt = getPins().getSPIPins(PinFunction::SD);
    if (sd_opt) {
      sd_cs = sd_opt.value().cs;
    } else {
      // no spi -> no sd
      LOGI("No sd defined -> sd_active=false")
      cfg.sd_active = false;
      sd_cs = -1;
    }
    return sd_cs;
  }

  /// Determines the action logic (ActiveLow or ActiveTouch) for the pin
  AudioActions::ActiveLogic getActionLogic(int pin) {
    auto opt = board().getPins().getPin(pin);
    PinLogic logic = PinLogic::Input;
    if (opt) logic = opt.value().pin_logic;
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
};

}  // namespace audio_tools
