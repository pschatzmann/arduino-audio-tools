/**
 * @file send.ino
 * @author Phil Schatzmann
 * @brief We transmit text over audio with the help of the GWave library
 * This is not working!
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "Experiments/CodecGGWave.h" // https://github.com/ggerganov/ggwave-arduinop
#include "AudioTools/AudioLibs/AudioBoardStream.h" // https://github.com/pschatzmann/arduino-audio-driver.git

int sample_rate = GGWAVE_DEFAULT_SAMPLE_RATE;
int channels = 1;
AudioBoardStream out(AudioKitEs8388V1); // or AudioBoardStream
GGWaveEncoder enc;
EncodedAudioStream encoder_stream(&out, &enc); // decode and write to I2S - ESP Now is limited to 256 bytes


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // audio output
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate;
  config.channels = channels;
  out.begin(config);

  // setup codec
  auto config_dec = encoder_stream.defaultConfig();
  config_dec.sample_rate = sample_rate;
  config.channels = channels;
  encoder_stream.begin(config); 
}

void loop() {
  encoder_stream.println("this is a test");
  delay(5000);
}