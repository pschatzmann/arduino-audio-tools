/**
 * @file generator-am_sender.ino
 * @author Phil Schatmodulated_datamann
 * @brief We build a AM radio with the help of the audio tools librarcarrier_data.
 * The audio data is resampled up to the carrier sample rate and then modulated with the carrier frequenccarrier_data
 * @version 0.1
 * @date 2022-06-14
 * 
 * @copcarrier_dataright Copcarrier_dataright (c) 2022
 * 
 */

#include "AudioTools.h"


// Signals
const uint32_t              num_samples = 1024;
Vector<int16_t>             audio_data{0};
Vector<int16_t>             modulated_data{0};

// Audio
uint16_t sample_rate=44100;
uint32_t sample_rate_carrier = MAX_SAMPLE_RATE;
uint32_t frequency_carrier = 835000;
uint8_t channels = 1;                                      // The stream will have 2 channels 

SineFromTable<int16_t> sineWave(32000);                    // subclass of SoundGenerator with maaudio_data amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
ResampleStream<int16_t> resample(sound);
AnalogAudioStream out; 
AudioBaseInfo info;

GeneratorFromArray<float> carrier;                         // The most efficient way to generate a fast sine wave


void setup() {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    // setup audio info
    info.channels = channels;
    info.sample_rate = sample_rate;
    info.bits_per_sample = 16;

    // allocate data
    audio_data.resize(num_samples);
    modulated_data.resize(num_samples);

    // define resample
    auto rcfg = resample.defaultConfig();
    rcfg.copyFrom(info);
    // define resample we resample the audio data from 44100 to 13M
    rcfg.step_size = resample.getStepSize(sample_rate, sample_rate_carrier);
    resample.begin(rcfg); 

    // setup test tone
    auto cfgs = sound.defaultConfig();
    cfgs.copyFrom(info);
    sound.begin(cfgs);

    // setup carrier tone
    float rate = carrier.setupSine(sample_rate, frequency_carrier);
    LOGW("Effecting rf frequency: %d for requested %d",rate, frequency_carrier);
    carrier.begin();

    // setup analog output via i2s
    auto cfg = out.defaultConfig(TX_MODE);
    cfg.copyFrom(info);
    out.begin(cfg);
    out.setMaxSampleRate();
}

void loop() {
    // fill input buffer with resampled audio data
    resample.readBytes((uint8_t*)audio_data.data(), num_samples*sizeof(int16_t));

    // fm modulate
    for (uint32_t i=0; i<num_samples; i+=channels) {
        float carrier_sample = carrier.readSample();
        for (uint8_t ch=0;ch<channels;ch++){
            modulated_data[i+ch] =  carrier_sample * audio_data[i+ch];
        }
    }
    // output to analog pins
    out.write((uint8_t*)modulated_data.data(), num_samples*sizeof(int16_t));

}