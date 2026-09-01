/**
 * @brief Testing AC3AudioDecoder
 * install https://github.com/pschatzmann/codec-ac3
 *
 * Reads a raw AC-3 elementary stream (e.g. sample-1.ac3 from the
 * codec-ac3 library) from the SD card and plays it back via I2S.
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecAC3.h"
#include "SD.h"

const char* file_name = "/sample-1.ac3";
File ac3File;
AudioBoardStream out(AudioKitEs8388V1);
AC3AudioDecoder ac3dec;
EncodedAudioStream decoder(&out, &ac3dec);  // output to decoder
StreamCopy copier(decoder, ac3File);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup i2s
  out.begin(out.defaultConfig());

  // setup SD card
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO,
            PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  while (!SD.begin(PIN_AUDIO_KIT_SD_CARD_CS)) {
    Serial.println("SD.begin failed");
    delay(1000);
  }
  ac3File = SD.open(file_name);

  decoder.begin();
}

void loop() { copier.copy(); }
