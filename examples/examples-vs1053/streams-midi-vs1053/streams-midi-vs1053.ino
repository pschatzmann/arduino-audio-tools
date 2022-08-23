/**
 * @file streams-midi-vs1053.ino
 * @author Phil Schatzmann
 * @brief Demo that shows how to send midi commands to the vs1053
 * @version 0.1
 * @date 2022-08-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"


VS1053Stream vs1053; // final output
MidiStreamOut out(vs1053);
uint16_t note = 65; // 0 to 128
uint16_t amplitude = 128; // 0 to 128
int channel = 0;

//Send a MIDI note-on message.  Like pressing a piano key
void noteOn(uint8_t channel, uint8_t note, uint8_t attack_velocity=100) {
  vs1053.sendMidiMessage( (0x90 | channel), note, attack_velocity);
}

//Send a MIDI note-off message.  Like releasing a piano key
void noteOff(uint8_t channel, uint8_t note, uint8_t release_velocity=100) {
  vs1053.sendMidiMessage( (0x80 | channel), note, release_velocity);
}


void setup() {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);  

 // setup vs1053
  auto cfg = vs1053.defaultConfig();
  cfg.is_midi = true; // vs1053 is accepting streaming midi data
  // Use your custom pins or define in AudioCodnfig.h
  //cfg.cs_pin = VS1053_CS; 
  //cfg.dcs_pin = VS1053_DCS;
  //cfg.dreq_pin = VS1053_DREQ;
  //cfg.reset_pin = VS1053_RESET;
  vs1053.begin(cfg);

}

void loop() {
    Serial.println();
    Serial.print("playing ");
    Serial.println(++note);

    out.noteOn(channel, note, amplitude );
    delay(900);
    out.noteOff(channel, note, 20 );
    delay(200);
    if (note>=90) {
      note = 64;
    }
}
