
/**
 * @file receive-mp3.ino
 * @brief Example of receiving an mp3 stream over serial and playing it over I2S
 * using the AudioTools library.
 * The processing must be synchronized with RTS/CTS flow control to prevent a
 * buffer overflow and lost data.
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"


AudioBoardStream i2s(AudioKitEs8388V1);  // final output of decoded stream
EncodedAudioStream dec(&i2s, new MP3DecoderHelix());  // Decoding stream
HardwareSerial MP3Serial(1);             // define a Serial for UART1
StreamCopy copier(dec, MP3Serial);       // copy url to decoder
// pins
const int MySerialTX = -1;
const int MySerialRX = 18;
const int MySerialRTS = -1;
const int MySerialCTS = 20;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup serial data source with flow control
  MP3Serial.begin(115200, SERIAL_8N1);
  MP3Serial.setPins(MySerialRX, MySerialTX, MySerialCTS, MySerialRTS);
  MP3Serial.setHwFlowCtrlMode(HW_FLOWCTRL_CTS_RTS);
 
  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  dec.begin();

}

void loop() { copier.copy(); }