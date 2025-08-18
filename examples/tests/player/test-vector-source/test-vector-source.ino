#include "AudioTools.h"
#include "AudioTools/Disk/AudioSource.h"
#include "SdFat.h"
#include "SPI.h"

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK 14

// SDFAT objects
SdFat sd;
File32 file;
const char* path = "/Bob Dylan/Bringing It All Back Home";

// Audio objects
AudioSourceVector<File32> audioSource;
NamePrinter namePrinter(audioSource, path);
File32 audioFile;

// Callback to convert file path to stream for AudioSourceVector
File32* fileToStream(const char* path, File32& oldFile) {
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
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  if (!sd.begin(PIN_AUDIO_KIT_SD_CARD_CS, SPI_HALF_SPEED)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized successfully");

  Serial.println("\n--- Collecting audio files from SD card ---");

  // Set up the callback for AudioSourceVector
  audioSource.setNameToStreamCallback(fileToStream);

  // Use SDFAT's ls method to list files and automatically add them via NamePrinter
  // This will print each file name, and NamePrinter will capture each line
  // and call audioSource.addName() for each file
  auto dir = sd.open(path, FILE_READ);
  dir.ls(&namePrinter, 0);
  dir.close();

  Serial.print("\nTotal files collected: ");
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
