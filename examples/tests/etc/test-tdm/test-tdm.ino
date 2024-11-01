// TDM test using a RP2040 with a CS42448 DAC

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

const AudioInfo info_in(44100, 1, 16);
const AudioInfo info_out(44100, 8, 16);
const MusicalNotes notes;
Vector<SineWaveGenerator<int16_t>> sineWaves{info_out.channels};
Vector<GeneratedSoundStream<int16_t>> sound{info_out.channels};
InputMerge<int16_t> imerge;  // merge to 8 channels
DriverPins dac_pins;
AudioBoard board(AudioDriverCS42448, dac_pins);
AudioBoardStream tdm(board); 
StreamCopy copier(tdm, imerge);

// we use the AudioBoard API to set up the pins for the DAC
void setupDACPins() {
  // add i2c codec pins: scl, sda, port
  dac_pins.addI2C(PinFunction::CODEC, 4, 5);
  // add i2s pins: mclk, bck, ws,data_out, data_in ,(port)
  dac_pins.addI2S(PinFunction::CODEC,-1, 3, 4, 5, -1);
}

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup input sound and merge -> we get an merge with 8 channels
  for (int j = 0; j < info_out.channels; j++) {
    // use a different tone for each channel
    sineWaves[j].begin(info_in, notes.frequency(j * 2));
    // setup GeneratedSoundStream 
    sound[j].setInput(sineWaves[j]);
    sound[j].begin(info_in);
    // setup merge input stream channels
    imerge.add(sound[j]);
  }

  // setup DAC pins
  setupDACPins();

  // setup DAC & I2S
  auto cfg = tdm.defaultConfig();
  cfg.copyFrom(info_out);
  cfg.input_device = ADC_INPUT_NONE;
  cfg.output_device = DAC_OUTPUT_ALL;
  cfg.signal_type = TDM;
  tdm.begin();

  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() { copier.copy(); }