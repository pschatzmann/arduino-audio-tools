/**
 * @file streams-audiokit-effects-audiokit.ino
 * @author Phil Schatzmann
 * @brief A simple Guitar Effects sketch
 * @version 0.1
 * @date 2022-10-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include <ArduinoJson.h>

// Audio Format
const int sample_rate = 44100;
const int channels = 1;
const int bits_per_sample = 16;

// Effect control input
float volumeControl = 1.0;
int16_t clipThreashold = 4990;
float fuzzEffectValue = 6.5;
int16_t distortionControl = 4990;
int16_t tremoloDuration = 200;
float tremoloDepth = 0.5;

// Effects 
Boost boost(volumeControl);
Distortion distortion(clipThreashold);
Fuzz fuzz(fuzzEffectValue);
Tremolo tremolo(tremoloDuration, tremoloDepth, sample_rate);

// Audio IO
AudioKitStream kit; // Access I2S as stream
AudioEffects<GeneratorFromStream<int16_t>> effects(kit, channels ,1.0); // apply effects on kit
GeneratedSoundStream<int16_t> in(effects);
StreamCopy copier(kit, in); // copy in to kit


// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);
    
    auto cfg = kit.defaultConfig(RXTX_MODE);
    cfg.sd_active = false;
    cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
    // minimize lag
    cfg.buffer_count = 2;
    cfg.buffer_size = 256;
    kit.begin(cfg);
    kit.setVolume(1.0); // max output

    // setup effects
    effects.addEffect(boost);
    effects.addEffect(distortion);
    effects.addEffect(fuzz);
    effects.addEffect(tremolo);

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}