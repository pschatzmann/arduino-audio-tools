#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/TfLiteAudioOutput.h"
#include "model.h" // tensorflow model

AudioKitStream kit; //Audio source
TfLiteAudioFeatureProvider fp;
TfLiteAudioOutput<4> tfl; // Audio sink
const char* kCategoryLabels[4] = {
    "silence",
    "unknown",
    "yes",
    "no",
};
StreamCopy copier(tfl, kit); // copy mic to tfl
int channels = 2;
int samples_per_second = 16000;

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
    tfl.begin(g_model, fp, kCategoryLabels, 10 * 1024);
}

void loop() {
    copier.copy();
}
