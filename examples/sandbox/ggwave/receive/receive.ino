/**
 * @file receive.ino
 * @author Phil Schatzmann
 * @brief We receive text over audio with the help of the GWave library
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
AudioBoardStream in(AudioKitEs8388V1);  // or e.g. I2SStream
GGWaveDecoder dec;
EncodedAudioStream encoder_stream(Serial, dec); // decode and write to I2S - ESP Now is limited to 256 bytes
StreamCopy copier(encoder_stream, in, GGWAVE_DEFAULT_BYTES_PER_FRAME);  // copy mic to tfl


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // audio input
  auto config = in.defaultConfig(RX_MODE);
  config.sample_rate = sample_rate;
  config.channels = channels;
  config.input_device = ADC_INPUT_LINE2; // microphone
  in.begin(config);

  // setup codec
  auto config_dec = encoder_stream.defaultConfig();
  config_dec.sample_rate = sample_rate;
  config.channels = channels;
  encoder_stream.begin(config); 
}

void loop() {
    copier.copy();
}