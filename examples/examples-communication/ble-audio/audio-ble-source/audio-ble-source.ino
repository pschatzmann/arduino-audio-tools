/**
 * @file audio-ble-source.ino
 * @brief LE Audio source: sends a sine wave to a peer which is connected to us
 * (e.g. audio-ble-sink or a phone/PC with LE Audio).
 *
 * The sine wave is generated as 48 kHz stereo; the peer must negotiate 48 kHz.
 */

#include "AudioTools.h"
#include "AudioTools/Communication/AudioBLE.h"

SineGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
AudioBLE ble;
StreamCopy copier(ble, sound);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // 48 kHz stereo: the format of the sine wave and of the LC3 preset
  AudioBLEConfig cfg = ble.defaultConfig(TX_MODE);
  cfg.name = "AudioBLE Source";

  sineWave.begin(cfg, N_B4);

  // start advertising: the peer can connect and receive the audio
  if (!ble.begin(cfg)) {
    Serial.println("AudioBLE begin failed");
  }
}

void loop() {
  if (ble.availableForWrite() > 0) {
    copier.copy();
  }
}
