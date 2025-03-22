//  We use the decoding on the input side to provid pcm data

#include "SPI.h"
#include "SD.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
//#include "AudioTools/AudioLibs/AudioBoardStream.h" // for SPI pins

File file;
MP3DecoderHelix mp3;  // or change to MP3DecoderMAD
EncodedAudioStream decoder(&file, &mp3);
BluetoothA2DPSource a2dp_source;
const int chipSelect = SS; //PIN_AUDIO_KIT_SD_CARD_CS;

// callback used by A2DP to provide the sound data - usually len is 128 2 channel int16 frames
int32_t get_sound_data(uint8_t* data, int32_t size) {
  int32_t result = decoder.readBytes((uint8_t*)data, size);
  delay(1); // feed the dog
  return result;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // open file
  //SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  SD.begin(chipSelect);
  file = SD.open("/test.mp3", FILE_READ);
  if (!file) {
    Serial.println("file failed");
    stop();
  }

  // make sure we have enough space for the pcm data
  decoder.transformationReader().resizeResultQueue(1024 * 8);
  if (!decoder.begin()) {
    Serial.println("decoder failed");
    stop();
  }

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.set_data_callback(get_sound_data);
  a2dp_source.start("LEXON MINO L");
}

// Arduino loop - repeated processing 
void loop() {
  delay(1000);
}
