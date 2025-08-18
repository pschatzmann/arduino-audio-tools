#include "AudioTools.h"
#include "AudioTools/Disk/AudioSource.h"
#include "SPI.h"
#include "SdFat.h"

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK 14

// SDFAT objects
SdFat sd;
File32 file;

// file names in program mem
const char* filesNames[] = {
    "/Bob Dylan/Bringing It All Back Home/04 Love minus zero_no limit.mp3",
    "/Bob Dylan/Bringing It All Back Home/11 It's all over now, Baby Blue.mp3",
    "/Bob Dylan/Bringing It All Back Home/06 On the road again.mp3",
    "/Bob Dylan/Bringing It All Back Home/08 Mr. Tambourine Man.mp3",
    "/Bob Dylan/Bringing It All Back Home/10 It's alright, Ma (I'm only bl.mp3",
    "/Bob Dylan/Bringing It All Back Home/02 She belongs to me.mp3",
    "/Bob Dylan/Bringing It All Back Home/03 Maggie's farm.mp3",
    "/Bob Dylan/Bringing It All Back Home/01 Subterranean homesick blues.mp3",
    "/Bob Dylan/Bringing It All Back Home/07 Bob Dylan's 115th dream.mp3",
    "/Bob Dylan/Bringing It All Back Home/05 Outlaw blues.mp3",
    "/Bob Dylan/Bringing It All Back Home/09 Gates of Eden.mp3"};

// Audio objects
File32* fileToStreamCB(const char* path, File32& oldFile);
AudioSourceArray<File32> audioSource(filesNames, fileToStreamCB);
File32 audioFile;

// Callback to convert file path to stream for AudioSourceVector
File32* fileToStreamCB(const char* path, File32& oldFile) {
  oldFile.close();
  audioFile.open(path);

  if (!audioFile) {
    Serial.print("Failed to open: ");
    Serial.println(path);
    return nullptr;
  }
  return &audioFile;
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  Serial.println("AudioSourceVector with SDFAT Test");

  // Initialize SD card
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO,
            PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  if (!sd.begin(PIN_AUDIO_KIT_SD_CARD_CS, SPI_HALF_SPEED)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully");
  Serial.print("\nTotal files: ");
  Serial.println(audioSource.size());

  // Display collected files
  Serial.println("\n--- Collected Files ---");
  for (int i = 0; i < audioSource.size(); i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.println(audioSource.name(i));
  }

  // Test navigation
  if (audioSource.size() > 0) {
    Serial.println("\n--- Testing AudioSource Navigation ---");

    // Start playback simulation
    audioSource.begin();

    // Select first file
    File32* stream = audioSource.selectStream(0);
    if (stream) {
      Serial.print("Selected file 0: ");
      Serial.println(audioSource.toStr());
      stream->close();
    }

    // Try next file
    if (audioSource.size() > 1) {
      stream = audioSource.nextStream(1);
      if (stream) {
        Serial.print("Next file: ");
        Serial.println(audioSource.toStr());
        stream->close();
      }
    }

    // Test selectStream by path
    if (audioSource.size() > 0) {
      const char* firstFile = audioSource.name(0);
      Serial.print("Selecting by path: ");
      Serial.println(firstFile);

      stream = audioSource.selectStream(firstFile);
      if (stream) {
        Serial.println("Successfully selected by path!");
        stream->close();
      } else {
        Serial.println("Failed to select by path");
      }
    }
  }

  Serial.println("\nTest completed!");
}

void loop() {
  // Nothing to do in loop for this test
  delay(1000);
}
