/**
 * @file basic-a2dp-audiospdif.ino
 * @brief A2DP Sink with output to SPDIFStream
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#define USE_A2DP
#include "AudioTools.h"
#include "AudioOutputSPDIF.h"


BluetoothA2DPSink a2dp_sink;
AudioOutputSPDIF spdif;

// Write data to AudioKit in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    int16_t* samples = (int16_t* ) data;
    int count = length/4;
    for (int j=0; j<count;j+=2){
      spdif.ConsumeSample(samples+j);
    }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  spdif.SetPinout(-1, -1, 22);
  spdif.SetBitsPerSample(16); 
  spdif.SetChannels(2); 
  spdif.begin();

  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);
  a2dp_sink.start("a2dp-spdif");
}

void loop() {
  delay(100);
}