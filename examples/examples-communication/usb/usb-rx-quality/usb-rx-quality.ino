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
 * bytes=176400 samples=44100 zeros=0.0% clicks=0     ← PASS
 * bytes=352800 samples=88200 zeros=0.0% clicks=0     ← PASS
 * bytes=529200 samples=132300 zeros=2.1% clicks=3    ← FAIL: gaps + clicks
 */

#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioStream.h"

AudioInfo info(44100, 2, 16);
USBAudioStream in;
uint8_t buf[1024];
uint32_t total_bytes = 0;
uint32_t zero_samples = 0;
uint32_t click_count = 0;
int16_t prev_sample = 0;

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

  // If already enumerated, additional class driver begin() e.g msc, hid, midi
  // won't take effect until re-enumeration: on ESP32 you can alternatively call USB.begin()
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

}

void loop() {
  int n = in.readBytes(buf, sizeof(buf));
  if (n > 0) {
    int16_t* samples = (int16_t*)buf;
    int count = n / 2;
    for (int i = 0; i < count; i += 2) {  // left channel only
      int16_t s = samples[i];
      // Count zero samples (gaps)
      if (s == 0) zero_samples++;
      // Count clicks (large jumps)
      if (abs(s - prev_sample) > 20000) click_count++;
      prev_sample = s;
    }
    total_bytes += n;
  }

  static unsigned long last_ms = 0;
  if (millis() - last_ms > 1000) {
    last_ms = millis();
    uint32_t total_samples = total_bytes / 4;  // stereo 16-bit
    float zero_pct =
        total_samples ? (100.0f * zero_samples / total_samples) : 0;
    Serial.printf("bytes=%u samples=%u zeros=%.1f%% clicks=%u\n", total_bytes,
                  total_samples, zero_pct, click_count);
  }
}
