/**
 * @file audio-out.ino
 * @brief Sine wave playback for the Guition JC4880P443C_I_W (ESP32-P4)
 * onboard ES8311 codec + NS4150 speaker amp, via I2SCodecStream.
 *
 * UNTESTED - written without access to this hardware, from the pin map
 * documented in guition-jc4880p4-bsp's board_p4_pins.h (a header that's
 * itself explicit that these audio pins are "not driven by the core
 * BSP" - i.e. board-documented, not exercised by that library, since it
 * only handles display/touch). This example is the first thing in this
 * repo to actually drive them.
 *
 * There's no arduino-audio-driver board entry for this board, so this
 * uses arduino-audio-driver's `GenericES8311` (no predefined pins - the
 * same pattern the wiki documents for `GenericWM8960`/ES8388: `Wire`
 * is set up manually for codec I2C, and I2S pins are set directly on
 * the stream config).
 *
 * `GenericES8311` has no `PinFunction::PA` pin, so unlike a
 * board-specific entry it does not toggle the NS4150 amp-enable pin for
 * you - this file does that manually. UNVERIFIED GUESS: PA_EN (GPIO11)
 * is driven active-HIGH here, based on NS4150's datasheet SD (shutdown)
 * pin being active-low - i.e. HIGH = enabled. board_p4_pins.h doesn't
 * state the polarity; if there's no audio but the codec itself looks
 * configured correctly, try driving it LOW instead.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"

// Pins from guition-jc4880p4-bsp's board_p4_pins.h "Audio" section.
constexpr int kPinI2sMclk = 13;
constexpr int kPinI2sBck = 12;
constexpr int kPinI2sWs = 10;
constexpr int kPinI2sDout = 9;   // P4 -> codec
constexpr int kPinCodecScl = 8;  // shared with GT911 touch bus
constexpr int kPinCodecSda = 7;
constexpr int kPinAmpEnable = 11;  // NS4150 PA_EN - see UNVERIFIED note above

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> sound(sineWave);
I2SCodecStream out(GenericES8311);
StreamCopy copier(out, sound);

void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  pinMode(kPinAmpEnable, OUTPUT);
  digitalWrite(kPinAmpEnable, HIGH);  // enable NS4150 - see UNVERIFIED note above

  Wire.begin(kPinCodecSda, kPinCodecScl);

  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.pin_mck = kPinI2sMclk;
  config.pin_bck = kPinI2sBck;
  config.pin_ws = kPinI2sWs;
  config.pin_data = kPinI2sDout;
  if (!out.begin(config)) {
    Serial.println("error!");
    stop();
  }
  out.setVolume(0.5f);

  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

void loop() { copier.copy(); }
