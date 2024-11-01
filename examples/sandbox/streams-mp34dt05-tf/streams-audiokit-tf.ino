/**
 * @file streams-audiokit-tf.ino
 * @author Phil Schatzmann
 * @brief We use a Nano BLE Sense which has a microphone as input to feed a Tensorflow Light Model which recognises the words yes and no.
 * @version 0.1
 * @date 2022-03-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioMP34DT05.h"
#include "AudioTools/AudioLibs/TfLiteAudioStream.h"
#include "model.h"  // tensorflow model

AudioMP34DT05 mic; // Access I2S as stream
TfLiteAudioFeatureProvider fp;
TfLiteAudioStream tfl;  // Audio sink
const char* kCategoryLabels[4] = {
    "silence",
    "unknown",
    "yes",
    "no",
};
StreamCopy copier(tfl, mic);  // copy mic to tfl
int channels = 1;
int samples_per_second = 16000;

void respondToCommand(const char* found_command, uint8_t score,
                      bool is_new_command) {
  if (is_new_command) {
    char buffer[80];
    sprintf(buffer, "Result: %s, score: %d, is_new: %s", found_command, score,
            is_new_command ? "true" : "false");
    Serial.println(buffer);
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial); // wait for serial to be ready
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  Serial.println("starting...");

  // Setup tensorflow
  fp.kAudioChannels = channels;
  fp.kAudioSampleFrequency = samples_per_second;
  fp.respondToCommand = respondToCommand;
  tfl.begin(g_model, fp, kCategoryLabels, 10 * 1024);

  // setup Audiomic
  auto cfg = mic.defaultConfig(RX_MODE);
  cfg.channels = channels;
  cfg.sample_rate = samples_per_second;
  mic.begin(cfg);

  Serial.println("started!");
}

void loop() { copier.copy(); }
