/**
 * @file streams-memory_midi-vs1953.ino
 * @author Phil Schatzmann
 * @brief The module can play Midi Files: Compile with Partition Scheme Hughe APP. The midi file was converted with xxd to a header file.
 * It seems that only midi files type 0 are supported.
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"
#include "bora0.h"


VS1053Stream out; // output
MemoryStream music(bora0_midi, bora0_midi_len);
StreamCopyT<int16_t> copier(out, music); // copies sound into i2s

void setup(){
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    auto config = out.defaultConfig(TX_MODE);
    config.is_encoded_data = true; // vs1053 is accepting encoded midi data
  //config.cs_pin = VS1053_CS; 
  //config.dcs_pin = VS1053_DCS;
  //config.dreq_pin = VS1053_DREQ;
  //config.reset_pin = VS1053_RESET;
    out.begin(config);
}

void loop(){
    if (!copier.copy()){
      delay(60000);  // 
      out.end();
      stop();
    }
}