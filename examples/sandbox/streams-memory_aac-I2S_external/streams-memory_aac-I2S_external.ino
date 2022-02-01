/**
 * @file streams-memory_aac-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-memory_aac-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_HELIX 

#include "AudioTools.h"
#include "AudioCodecs/CodecAACHelix.h"
#include "audio.h"



//recorded with 2 channels at 44100 hz
MemoryStream in(gs_16b_2c_44100hz_aac, gs_16b_2c_44100hz_aac_len);
I2SStream i2s;      //  I2S Output as stream
EncodedAudioStream dec(i2s, new AACDecoderHelix()); // Decoder stream
StreamCopy copier(dec, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  dec.setNotifyAudioChange(i2s);
  dec.begin();

  in.begin();
}

// Arduino loop  
void loop() {
  if (in) {
    copier.copy();
  }
}