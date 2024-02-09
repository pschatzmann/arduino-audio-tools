/**
 * @file streams-tf-i2s.ino
 * @author Phil Schatzmann
 * @brief We use Tenorflow lite to generate some audio data and output it via I2S to the AudioKit
 * @version 0.1
 * @date 2022-04-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/TfLiteAudioStream.h"
#include "AudioLibs/AudioBoardStream.h"
#include "model.h"

TfLiteSineReader tf_reader(20000,0.3);  // Audio generation logic 
TfLiteAudioStream tf_stream;            // Audio source -> no classification so N is 0
AudioBoardStream i2s(AudioKitEs8388V1);                     // Audio destination
StreamCopy copier(i2s, tf_stream);      // copy tf_stream to i2s
int channels = 1;
int samples_per_second = 16000;


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup tensorflow input
  auto tcfg = tf_stream.defaultConfig();
  tcfg.channels = channels;
  tcfg.sample_rate = samples_per_second;
  tcfg.kTensorArenaSize = 2 * 1024;
  tcfg.model = g_model;
  tcfg.reader = &tf_reader;
  tf_stream.begin(tcfg);

  // setup Audioi2s output
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.channels = channels;
  cfg.sample_rate = samples_per_second;
  i2s.begin(cfg);

}

void loop() { copier.copy(); }
