/**
 * @file streams-audiokit-sd_wav.ino
 * @author Phil Schatzmann
 * @brief Demonstrates how to use an encoder to write to a file
 * @version 0.1
 * @date 2023-05-04
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <SD.h>
#include <SPI.h>

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

const char* file_name = "/rec.wav";
AudioInfo info(16000, 1, 16);
AudioBoardStream in(AudioKitEs8388V1);
File file;  // final output stream
EncodedAudioStream out(&file, new WAVEncoder());
StreamCopy copier(out, in);  // copies data
uint64_t timeout;


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup input
  auto cfg = in.defaultConfig(RX_MODE);
  cfg.sd_active = true;
  cfg.copyFrom(info);
  cfg.input_device = ADC_INPUT_LINE2; // microphone
  in.begin(cfg);

  // Open SD drive
  if (!SD.begin(PIN_AUDIO_KIT_SD_CARD_CS)) {
    Serial.println("Initialization failed!");
    stop();
  }
  
  // Cleanup if necessary
  if (SD.exists(file_name))
    SD.remove(file_name);

  // open file
  file = SD.open(file_name, FILE_WRITE);
  if (!file){
    Serial.println("file failed!");
    stop();
  }

  // setup output
  out.begin(info);

  // set timeout - recoring for 5 seconds
  timeout = millis() + 5000;
}

void loop() {
  if (millis() < timeout) {
    // record to wav file
    copier.copy();
  } else {
    // close file when done
    if (file) {
      file.flush();
      Serial.print("File has ");
      Serial.print(file.size());
      Serial.println(" bytes");
      file.close();
    }
  }
}
