
// This example tests the update of the file size in the WAV header after recording.
#include "AudioTools.h"
#include "AudioTools/Disk/WAVFileInfo.h"
#include "SD_MMC.h"

const char* test_file = "/test.wav";

// record sine data in file
void record() {
  File file = SD_MMC.open(test_file, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  assert(file.seek(0));

  SineGenerator sine;
  GeneratedSoundStream gen(sine);
  WAVEncoder encoder;
  EncodedAudioStream out(&file, &encoder);
  StreamCopy copy(out, gen);

  // setup objects
  gen.begin();
  sine.setFrequency(440);
  out.begin();

  // copy data
  for (int j = 0; j < 50; j++) {
    copy.copy();
  }

  // save file
  file.close();
}

// print file size info
void printInfo() {
  File file = SD_MMC.open(test_file, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  WAVFileInfo wav_info;
  WAVAudioInfo info;
  assert(wav_info.getInfo(file, info));
  Serial.print("WAV header file size:");
  Serial.println(info.file_size);
  Serial.print("WAV file size: ");
  Serial.println(file.size());
  Serial.print("WAV header data length:");
  Serial.println(info.data_length);
  file.close();
}

// update file size
void update() {
  // Use read/write mode without truncating the file.
  File file = SD_MMC.open(test_file, "rb+");
  if (!file) {
    Serial.println("Failed to open file for read/write");
    return;
  }

  WAVFileInfo wav_info;
  if (!wav_info.updateSize(file)) {
    Serial.println("Failed to update file size in WAV header");
  } else {
    Serial.println("WAV header updated with correct file size");
  }
  file.close();

}

void setup() {
  Serial.begin(115200);
  delay(3000);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  Serial.println("starting...");

  if (!SD_MMC.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  SD_MMC.remove(test_file);

  record();
  Serial.println("Before update:");
  printInfo();
  Serial.println("Update:");
  update();
  Serial.println("After update:");
  printInfo();
}

// nothing to do
void loop() {}
