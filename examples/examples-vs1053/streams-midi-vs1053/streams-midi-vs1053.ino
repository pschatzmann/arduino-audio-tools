/**
 * @file streams-midi-vs1053.ino
 * @author Phil Schatzmann
 * @brief Demo that shows how to send streaming midi commands to the vs1053
 * @version 0.1
 * @date 2022-08-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/VS1053Stream.h"


VS1053Stream vs1053; // final output
uint16_t note = 65; // 0 to 128
uint16_t amplitude = 128; // 0 to 128
int channel = 0;
uint8_t instrument = 0;

//Send a MIDI note-on message.  Like pressing a piano key
void noteOn(uint8_t channel, uint8_t note, uint8_t attack_velocity=100) {
  vs1053.sendMidiMessage( (0x90 | channel), note, attack_velocity);
}

//Send a MIDI note-off message.  Like releasing a piano key
void noteOff(uint8_t channel, uint8_t note, uint8_t release_velocity=100) {
  vs1053.sendMidiMessage( (0x80 | channel), note, release_velocity);
}

void selectInstrument(uint8_t channel, uint8_t instrument ){
 // Continuous controller 0, bank select: 0 gives you the default bank depending on the channel
 // 0x78 (percussion) for Channel 10, i.e. channel = 9 , 0x79 (melodic)  for other channels
  vs1053.sendMidiMessage(0xB0| channel, 0, 0x00); //0x00 default bank 

  // Patch change, select instrument
  vs1053.sendMidiMessage(0xC0| channel, instrument, 0);
}


void setup() {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

 // setup vs1053
  auto cfg = vs1053.defaultConfig();
  cfg.is_midi = true; // vs1053 is accepting streaming midi data
  // Use your custom pins or define in AudioCodnfig.h
  //cfg.cs_pin = VS1053_CS; 
  //cfg.dcs_pin = VS1053_DCS;
  //cfg.dreq_pin = VS1053_DREQ;
  //cfg.reset_pin = VS1053_RESET;
  vs1053.begin(cfg);
  vs1053.setVolume(1.0);

}

void loop() {
    Serial.println();
    Serial.print("playing ");
    Serial.println(++note);

    noteOn(channel, note, amplitude );
    delay(900);
    noteOff(channel, note, 20 );
    delay(200);
    if (note>=90) {
      note = 64;
      Serial.println();
      Serial.print("selecting Instrument ");
      Serial.println(++instrument);
      selectInstrument(channel, instrument);
    }
}
