/**
 * Quality test for USB audio receive (RX) stream.
 * Host side:
 *
 *
 * Generate and play a test tone:
 *
 * sox -n -r 44100 -c 2 -b 16 test_tone.wav synth 5 sine 440
 * aplay -D hw:Audio test_tone.wav
 * Serial output during playback:
 *
 *
 * Quality: clicks=0, dropouts=0, clipping=0, samples=44100     ← PASS
 * Quality: clicks=0, dropouts=0, clipping=0, samples=88200     ← PASS
 * Quality: clicks=3, dropouts=2, clipping=0, samples=132300    ← FAIL
 */

#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioStream.h"

AudioInfo info(44100, 2, 16);
USBAudioStream in;
QualityAnalysisStream quality(in);
uint8_t buf[1024];

void setup() {
  // Manual begin() is required on core without built-in support e.g. mbed
  // rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  Serial.begin(115200);
  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  config.enable_feedback_ep = false;
  in.begin(config);

  quality.setReporting(10000, Serial);
  quality.begin(info);

  // If already enumerated, additional class driver begin() e.g msc, hid, midi
  // won't take effect until re-enumeration: on ESP32 you can alternatively
  // call USB.begin()
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop() {
  quality.readBytes(buf, sizeof(buf));
}
