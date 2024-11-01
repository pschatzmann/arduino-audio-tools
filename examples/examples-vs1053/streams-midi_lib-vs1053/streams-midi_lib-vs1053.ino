/**
 * @file streams-midi-vs1053.ino
 * @author Phil Schatzmann
 * @brief Demo that shows how to send streaming midi commands to the vs1053
 * This example using my https://github.com/pschatzmann/arduino-midi.git library 
 * @version 0.1
 * @date 2022-08-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/VS1053Stream.h"
#include "Midi.h"


VS1053Stream vs1053; // final output
MidiStreamOut midi_out(vs1053);
uint8_t note = 65; // 0 to 128
uint8_t amplitude = 128; // 0 to 128
uint8_t instrument = 0;

void selectInstrument(uint8_t instrument, uint8_t bank=0x00){
 // Continuous controller 0, bank select: 0 gives you the default bank depending on the channel
 // 0x78 (percussion) for Channel 10, i.e. channel = 9 , 0x79 (melodic)  for other channels
  //vs1053.sendMidiMessage(0xB0| channel, 0, 0x00); //0x00 default bank 
  midi_out.controlChange(BANK_SELECT, bank);

  // Patch change, select instrument
  midi_out.programChange(instrument);
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

    midi_out.noteOn(note, amplitude );
    delay(900);
    midi_out.noteOff(note, 20 );
    delay(200);
    if (note>=90) {
      note = 64;
      Serial.println();
      Serial.print("selecting Instrument ");
      Serial.println(++instrument);
      selectInstrument(instrument);
    }
}