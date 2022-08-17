/**
 * @file streams-memory_midi-vs1953.ino
 * @author Phil Schatzmann
 * @brief The module can play Midi Files: Compile with Partition Scheme Hughe APP!
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"
#include "BohemianRhapsody.h"


VS1053Stream out; // output
MemoryStream music(QueenBohemianRhapsody_midi, QueenBohemianRhapsody_len);
StreamCopyT<int16_t> copier(out, music); // copies sound into i2s

void setup(){
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    auto config = out.defaultConfig(TX_MODE);
    config.is_encoded_data = true; // vs1053 is accepting encoded midi data
    out.begin(config);
}

void loop(){
    if (!copier.copy()){
      delay(10000);
      out.end();
      stop();
    }
}