/**
 * @file example-serial-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio over ESPNow
 * @version 0.1
 * @date 2022-03-09
 *
 * @copyright Copyright (c) 2022
 *
 */

uint16_t sample_rate = 44100;
uint8_t channels = 2;  // The stream will have 2 channels
SineWaveGenerator<sound_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<sound_t> sound( sineWave); // Stream generated from sine wave
ESPNowStream now;
StreamCopy copier(now, sound);  // copies sound into i2s
const char *ssd = "ssid";
const char *password = "password";

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  now.add
  now.begin(ssid, pasword);

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

void loop() { 
    copier.copy();
}