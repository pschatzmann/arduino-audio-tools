
#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSD.h" // or AudioSourceIdxSD.h
#include "AudioTools/AudioLibs/AudioBoardStream.h" // for SD pins

AudioSourceSD source("/", "", PIN_AUDIO_KIT_SD_CARD_CS);
MP3HeaderParser mp3;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  while (!SD.begin(PIN_AUDIO_KIT_SD_CARD_CS)) {
       Serial.println("SD.begin failed");
       delay(1000);
  }

  source.begin();

}

void loop() {

  File* p_file = (File*) source.nextStream();
  if (p_file==nullptr) stop();
  File& file = *p_file;  

  uint8_t tmp[1024];
  int len = file.readBytes((char*)tmp, 1024);

  // check if mp3 file
  bool is_mp3 = mp3.isValid(tmp, len);

  // print result
  Serial.print(" ==> ");
  Serial.print(file.name());
  Serial.println(is_mp3 ? " +":" -");
  file.close();

}
