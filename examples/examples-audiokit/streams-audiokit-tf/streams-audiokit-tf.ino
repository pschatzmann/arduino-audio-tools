
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/TfLiteAudioOutput.h"
#include "model.h"  // tensorflow model

AudioKitStream kit;  // Audio source
TfLiteAudioFeatureProvider fp;
TfLiteAudioOutput<4> tfl;  // Audio sink
const char* kCategoryLabels[4] = {
    "silence",
    "unknown",
    "yes",
    "no",
};
StreamCopy copier(tfl, kit);  // copy mic to tfl
int channels = 2;
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
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup Audiokit
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  cfg.channels = channels;
  cfg.sample_rate = samples_per_second;
  kit.begin(cfg);

  // Setup tensorflow
  fp.kAudioChannels = channels;
  fp.kAudioSampleFrequency = samples_per_second;
  fp.respondToCommand = respondToCommand;
  tfl.begin(g_model, fp, kCategoryLabels, 10 * 1024);
}

void loop() { copier.copy(); }
