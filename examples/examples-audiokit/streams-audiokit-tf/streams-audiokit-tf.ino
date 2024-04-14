
#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"
#include "AudioLibs/TfLiteAudioStream.h"
#include "model.h"  // tensorflow model

AudioBoardStream kit(AudioKitEs8388V1);  // Audio source
TfLiteAudioStream tfl;  // Audio sink
const char* kCategoryLabels[4] = {
    "silence",
    "unknown",
    "yes",
    "no",
};
StreamCopy copier(tfl, kit);  // copy mic to tfl
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
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup Audiokit
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.input_device = ADC_INPUT_LINE2;
  cfg.channels = channels;
  cfg.sample_rate = samples_per_second;
  cfg.use_apll = false;
  //cfg.auto_clear = true;
  cfg.buffer_size = 512;
  cfg.buffer_count = 16;
  kit.begin(cfg);

  // Setup tensorflow
  auto tcfg = tfl.defaultConfig();
  tcfg.setCategories(kCategoryLabels);
  tcfg.channels = channels;
  tcfg.sample_rate = samples_per_second;
  tcfg.kTensorArenaSize = 10 * 1024;
  tcfg.respondToCommand = respondToCommand;
  tcfg.model = g_model;
  tfl.begin(tcfg);
}

void loop() { copier.copy(); }