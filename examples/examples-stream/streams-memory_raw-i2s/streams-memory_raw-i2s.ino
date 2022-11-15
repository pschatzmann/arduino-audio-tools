/**
 * @file streams-memory_raw-i2s_external_dac.ino
 * @author Phil Schatzmann
 * @brief Compile with Partition Scheme Hughe APP!
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "AudioTools.h"
#include "StarWars30.h"



uint8_t channels = 1;
uint16_t sample_rate = 22050;

I2SStream i2s;  // Output to I2S
MemoryStream music(StarWars30_raw, StarWars30_raw_len);
StreamCopyT<int16_t> copier(i2s, music); // copies sound into i2s


void setup(){
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    auto config = i2s.defaultConfig(TX_MODE);
    config.sample_rate = sample_rate;
    config.channels = channels;
    config.bits_per_sample = 16;
    i2s.begin(config);

}

void loop(){
    if (!copier.copy()){
      i2s.end();
      stop();
    }
}