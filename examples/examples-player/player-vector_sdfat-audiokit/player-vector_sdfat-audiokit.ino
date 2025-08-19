/**
 * @file player-vector_sdfat.ino
 * @brief example that shows how to use the versatile AudioSourceVector class
 * as data source. We use the SdFat library to read audio files from an SD card.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

 #include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "SPI.h"
#include "SdFat.h"

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK 14

// Callback to convert file path to stream for AudioSourceVector
File32* fileToStream(const char* path, File32& oldFile);

// Global data
AudioSourceVector<File32> source(fileToStream);
AudioBoardStream i2s(AudioKitEs8388V1); // or replace with e.g. I2SStream
MP3DecoderHelix decoder;  // or change to MP3DecoderMAD
AudioPlayer player(source, i2s, decoder);
SdFat sd;
File32 audioFile;


// Callback to convert file path to stream for AudioSourceVector
File32* fileToStream(const char* path, File32& oldFile) {
  oldFile.close();
  audioFile.open(path);

  if (!audioFile) {
    LOGE("Failed to open: %s", path);
    return nullptr;
  }
  return &audioFile;
}

bool setupDrive() {
  // Initialize SD card
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO,
            PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

  if (!sd.begin(PIN_AUDIO_KIT_SD_CARD_CS, SPI_HALF_SPEED)) {
    Serial.println("SD card initialization failed!");
    return false;
  }

  // Use SDFAT's ls method to list files and automatically add file names 
  const char* path = "/Bob Dylan/Bringing It All Back Home";
  NamePrinter namePrinter(source, path);
  auto dir = sd.open(path, FILE_READ);
  dir.ls(&namePrinter, 0);
  dir.close();

  return true;
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup vector data source
  if (!setupDrive()) {
    Serial.println("Failed to setup drive");
    stop();
  }

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  // we setup up the SPI (pins) outselfs...
  cfg.sd_active = false;
  if (!i2s.begin(cfg)) {
    Serial.println("Failed to start I2S");
    stop();
  }

  // setup player
  player.setVolume(0.7);
  if (!player.begin()) {
    Serial.println("Failed to start player");
    stop();
  }
}

void loop() {
  player.copy();
}
