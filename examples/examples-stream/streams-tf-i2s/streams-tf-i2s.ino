/**
 * @file streams-tf-i2s.ino
 * @author Phil Schatzmann
 * @brief We use Tenorflow lite to generate some audio data and output it via I2S
 * @version 0.1
 * @date 2022-04-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/TfLiteAudioStream.h"
#include "model.h"

TfLiteSineReader tf_reader(20000,0.3);       // Audio generation logic 
TfLiteAudioStream tf_stream;            // Audio source -> no classification so N is 0
I2SStream i2s;                          // Audio destination
StreamCopy copier(i2s, tf_stream);      // copy tf_stream to i2s
int channels = 1;
int samples_per_second = 16000;


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

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
