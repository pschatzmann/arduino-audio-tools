#pragma once

#include "AudioTools.h"
#include "AudioKitHAL.h"
#include "AudioI2S/I2SConfig.h"
#include "AudioTools/AudioActions.h"

#ifndef AUDIOKIT_V1
#error Upgrade the AudioKit library
#endif

namespace audio_tools {

class AudioKitStream;
static AudioKitStream *pt_AudioKitStream = nullptr;

/**
 * @brief Configuration for AudioKitStream: we use as subclass of I2SConfig
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioKitStreamConfig : public I2SConfig {

friend class AudioKitStream;

 public:
  AudioKitStreamConfig(RxTxMode mode=RXTX_MODE) { setupI2SPins(mode); };
  // set adc channel with audio_hal_adc_input_t
  audio_hal_adc_input_t input_device = AUDIOKIT_DEFAULT_INPUT;
  // set dac channel 
  audio_hal_dac_output_t output_device = AUDIOKIT_DEFAULT_OUTPUT;
  bool sd_active = true;
  bool default_actions_active = true;
  audio_kit_pins pins;
  audio_hal_func_t driver = AUDIO_DRIVER;

  /// convert to config object needed by HAL
  AudioKitConfig toAudioKitConfig() {
    TRACED();
    audiokit_config.driver = driver;
    audiokit_config.pins = pins;
    audiokit_config.i2s_num = (i2s_port_t)port_no;
    audiokit_config.adc_input = input_device;
    audiokit_config.dac_output = output_device;
    audiokit_config.codec_mode = toCodecMode();
    audiokit_config.master_slave_mode = toMode();
    audiokit_config.fmt = toFormat();
    audiokit_config.sample_rate = toSampleRate();
    audiokit_config.bits_per_sample = toBits();
#if defined(ESP32)
    audiokit_config.buffer_size = buffer_size;
    audiokit_config.buffer_count = buffer_count;
#endif
    // we use the AudioKit library only to set up the codec
    audiokit_config.i2s_active = false;
#if AUDIOKIT_SETUP_SD
    audiokit_config.sd_active = sd_active;
#else
//  SD has been deactivated in the AudioKitConfig.h file
    audiokit_config.sd_active = false;
#endif  
    LOGW("sd_active = %s", sd_active ? "true" : "false" );

    return audiokit_config;
  }


 protected:
  AudioKitConfig audiokit_config;
  board_driver board;

  /// Defines the pins based on the information provided by the AudioKit project
  void setupI2SPins(RxTxMode rxtx_mode) {
    TRACED();
    this->rx_tx_mode = rxtx_mode;
    i2s_pin_config_t i2s_pins = {};
    board.setup(pins);
    board.get_i2s_pins((i2s_port_t)port_no, &i2s_pins);
    pin_mck = i2s_pins.mck_io_num;
    pin_bck = i2s_pins.bck_io_num;
    pin_ws = i2s_pins.ws_io_num;
    if (rx_tx_mode == RX_MODE){
      pin_data = i2s_pins.data_in_num;
      pin_data_rx = I2S_PIN_NO_CHANGE;
    } else {
      pin_data = i2s_pins.data_out_num;
      pin_data_rx = i2s_pins.data_in_num;      
    }
  };

  // convert to audio_hal_iface_samples_t
  audio_hal_iface_bits_t toBits() {
    TRACED();
    static const int ia[] = {16, 24, 32};
    static const audio_hal_iface_bits_t oa[] = {AUDIO_HAL_BIT_LENGTH_16BITS,
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
    TRACED();
    static const int ia[] = {8000,  11025, 16000, 22050,
                             24000, 32000, 44100, 48000};
    static const audio_hal_iface_samples_t oa[] = {
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
        int new_diff = abs((int)(oa[j] - sample_rate));
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
    TRACED();
    static const int ia[] = {I2S_STD_FORMAT,
                             I2S_LSB_FORMAT,
                             I2S_MSB_FORMAT,
                             I2S_PHILIPS_FORMAT,
                             I2S_RIGHT_JUSTIFIED_FORMAT,
                             I2S_LEFT_JUSTIFIED_FORMAT,
                             I2S_PCM};
    static const audio_hal_iface_format_t oa[] = {
        AUDIO_HAL_I2S_NORMAL, AUDIO_HAL_I2S_LEFT,  AUDIO_HAL_I2S_RIGHT,
        AUDIO_HAL_I2S_NORMAL, AUDIO_HAL_I2S_RIGHT, AUDIO_HAL_I2S_LEFT,
        AUDIO_HAL_I2S_DSP};
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
 * @brief AudioKit Stream which uses the
 * https://github.com/pschatzmann/arduino-audiokit library
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioKitStream : public AudioStream {
 public:
  AudioKitStream() { pt_AudioKitStream = this; }

  /// Provides the default configuration
  AudioKitStreamConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    TRACED();
    AudioKitStreamConfig result{mode};
    result.rx_tx_mode = mode;
    return result;
  }

  /// Starts the processing
  bool begin(AudioKitStreamConfig config) {
    TRACED();
    cfg = config;

    AudioStream::setAudioInfo(config);
    cfg.logInfo("AudioKitStream");

    // start codec
    auto kit_cfg = cfg.toAudioKitConfig();
    if (!kit.begin(kit_cfg)){
      LOGE("begin faild: please verify your AUDIOKIT_BOARD setting: %d", AUDIOKIT_BOARD);
      stop();
    }

    // start i2s
    i2s_stream.begin(cfg);

    // Volume control and headphone detection
    if (cfg.default_actions_active){
      setupActions();
    }
    
    // set initial volume
    setVolume(volume_value);
    is_started = true;
    return true;
  }

  // restart after end with initial parameters
  bool begin() override {
    return begin(cfg);
  }

  /// Stops the processing
  void end() override {
    TRACED();
    kit.end();
    i2s_stream.end();
    is_started = false;
  }

  /// We get the data via I2S - we expect to fill one buffer size
  int available() {
    return cfg.rx_tx_mode == TX_MODE ? 0 :  DEFAULT_BUFFER_SIZE;
  }

  size_t write(const uint8_t *data, size_t len) override {
    return i2s_stream.write(data, len);
  }

  /// Reads the audio data
  size_t readBytes(uint8_t *data, size_t len) override {
    return i2s_stream.readBytes(data, len);
  }

  /// Update the audio info with new values: e.g. new sample_rate,
  /// bits_per_samples or channels. 
  void setAudioInfo(AudioInfo info) override {
    TRACEI();

    if (cfg.sample_rate != info.sample_rate
    && cfg.bits_per_sample == info.bits_per_sample
    && cfg.channels == info.channels
    && is_started) {
      // update sample rate only
      LOGW("Update sample rate: %d", info.sample_rate);
      cfg.sample_rate = info.sample_rate;
      i2s_stream.setAudioInfo(cfg);
      kit.setSampleRate(cfg.toSampleRate());
    } else if (cfg.sample_rate != info.sample_rate
    || cfg.bits_per_sample != info.bits_per_sample
    || cfg.channels != info.channels
    || !is_started) {
      // more has changed and we need to start the processing
      cfg.sample_rate = info.sample_rate;
      cfg.bits_per_sample = info.bits_per_sample;
      cfg.channels = info.channels;
      cfg.logInfo("AudioKit");

      // Stop first
      if(is_started){
        end();
      }
      // start kit with new config
      i2s_stream.begin(cfg);
      kit.begin(cfg.toAudioKitConfig());
      is_started = true;
    }
  }

  AudioKitStreamConfig &config() { return cfg; }

  /// Sets the codec active / inactive
  bool setActive(bool active) { return kit.setActive(active); }

  /// Mutes the output
  bool setMute(bool mute) { return kit.setMute(mute); }

  /// Defines the Volume: Range 0 to 100
  bool setVolume(int vol) { 
    if (vol>100) LOGW("Volume is > 100: %d",vol);
    // update variable, so if called before begin we set the default value
    volume_value = vol;
    return kit.setVolume(vol);
  }

  /// Defines the Volume: Range 0 to 1.0
  bool setVolume(float vol) { 
    if (vol>1.0) LOGW("Volume is > 1.0: %f",vol);
    // update variable, so if called before begin we set the default value
    volume_value = 100.0 * vol;
    return kit.setVolume(volume_value);
  }

  /// Defines the Volume: Range 0 to 1.0
  bool setVolume(double vol) {
    return setVolume((float)vol);
  } 

  /// Determines the volume
  int volume() { return kit.volume(); }

  /// Activates/Deactives the speaker
  /// @param active 
  void setSpeakerActive (bool active){
    kit.setSpeakerActive(active);
  }

  /// @brief Returns true if the headphone was detected
  /// @return 
  bool headphoneStatus() {
    return kit.headphoneStatus();
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
  void addAction(int pin, void (*action)(bool,int,void*), void* ref=nullptr ) {
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
  void addAction(int pin, void (*action)(bool,int,void*), AudioActions::ActiveLogic activeLogic, void* ref=nullptr ) {
    TRACEI();
    actions.add(pin, action, activeLogic, ref);
  }

  /// Provides access to the AudioActions
  AudioActions &audioActions() {
    return actions;
  }

  /**
   * @brief Relative volume control
   * 
   * @param vol 
   */
  void incrementVolume(int vol) { 
    volume_value += vol;
    LOGI("incrementVolume: %d -> %d",vol, volume_value);
    kit.setVolume(volume_value);
  }

  /**
   * @brief Increase the volume
   *
   */
  static void actionVolumeUp(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->incrementVolume(+2);
  }

  /**
   * @brief Decrease the volume
   *
   */
  static void actionVolumeDown(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->incrementVolume(-2);
  }

  /**
   * @brief Toggle start stop
   *
   */
  static void actionStartStop(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->active = !pt_AudioKitStream->active;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
  }

  /**
   * @brief Start
   *
   */
  static void actionStart(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->active = true;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
  }

  /**
   * @brief Stop
   *
   */
  static void actionStop(bool, int, void*) {
    TRACEI();
    pt_AudioKitStream->active = false;
    pt_AudioKitStream->setActive(pt_AudioKitStream->active);
  }

  /**
   * @brief Switch off the PA if the headphone in plugged in 
   * and switch it on again if the headphone is unplugged.
   * This method complies with the
   */
  static void actionHeadphoneDetection(bool, int, void*) {
    AudioKit::actionHeadphoneDetection();
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
  I2SStream i2s_stream;
  AudioKitStreamConfig cfg = defaultConfig(RXTX_MODE);
  AudioActions actions;
  int volume_value = 40;
  bool active = true;
  bool is_started = false;

  /// Determines the action logic (ActiveLow or ActiveTouch) for the pin
  AudioActions::ActiveLogic getActionLogic(int pin){
#if defined(USE_EXT_BUTTON_LOGIC)
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    int size = sizeof(input_key_info) / sizeof(input_key_info[0]);
    for (int j=0; j<size; j++){
      if (pin == input_key_info[j].act_id){
        switch(input_key_info[j].type){
          case PERIPH_ID_ADC_BTN:
            LOGD("getActionLogic for pin %d -> %d", pin, AudioActions::ActiveHigh);
            return AudioActions::ActiveHigh;
          case PERIPH_ID_BUTTON:
            LOGD("getActionLogic for pin %d -> %d", pin, AudioActions::ActiveLow);
            return AudioActions::ActiveLow;
          case PERIPH_ID_TOUCH:
            LOGD("getActionLogic for pin %d -> %d", pin, AudioActions::ActiveTouch);
            return AudioActions::ActiveTouch;
        }
      }
    }
    LOGW("Undefined ActionLogic for pin: %d ",pin);
#endif
    return AudioActions::ActiveLow;
  }

  /// Setup the supported default actions
  void setupActions() {
    TRACEI();

    // pin conflicts with the SD CS pin for AIThinker and buttons
    if (! (cfg.sd_active && (AUDIOKIT_BOARD==5 || AUDIOKIT_BOARD==6))){
      LOGD("actionStartStop")
      addAction(kit.pinInputMode(), actionStartStop);
    } else {
      LOGW("Mode Button ignored because of conflict: %d ",kit.pinInputMode());
    }

    // pin conflicts with AIThinker A101 and headphone detection
    if (! (cfg.sd_active && AUDIOKIT_BOARD==6)) {  
      LOGD("actionHeadphoneDetection pin:%d",kit.pinHeadphoneDetect())
      actions.add(kit.pinHeadphoneDetect(), actionHeadphoneDetection, AudioActions::ActiveChange);
    } else {
      LOGW("Headphone detection ignored because of conflict: %d ",kit.pinHeadphoneDetect());
    }

    // pin conflicts with SD Lyrat SD CS GpioPinand buttons / Conflict on Audiokit V. 2957
    if (! (cfg.sd_active && (AUDIOKIT_BOARD==1 || AUDIOKIT_BOARD==7))){
      LOGD("actionVolumeDown")
      addAction(kit.pinVolumeDown(), actionVolumeDown); 
      LOGD("actionVolumeUp")
      addAction(kit.pinVolumeUp(), actionVolumeUp);
    } else {
      LOGW("Volume Buttons ignored because of conflict: %d ",kit.pinVolumeDown());
    }
  }
};

}  // namespace audio_tools
