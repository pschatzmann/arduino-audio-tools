/**
 * @file test-mp3-shine.ino
 * @author Phi Schatzmann
 * @brief This sktch is used to test the MP3 encoder using the shine library. It
 * generates a sine wave and encodes it to MP3 format. We measure the encoding
 * speed and print the frames per second to the serial monitor.
 * @version 0.1
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Shine.h"

AudioInfo info(44100, 2, 16);
SineGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
NullStream null_stream;
MP3EncoderShine shine;
EncodedAudioStream out(&null_stream, &shine);
StreamCopy copier(out, sound);  // copies sound into i2s
int frames = 0;
size_t total_frames = 0;
size_t total_time_ms = 0;
size_t counter = 0;

void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("starting...");

  sineWave.setFrequency(440);  // Set frequency to 440 Hz (A4 note)
  sound.begin(info);
  if (!out.begin(info)) {
    Serial.println("Failed to start MP3 encoder");
    stop();
  }

  frames =
      copier.bufferSize() / (info.channels * (info.bits_per_sample / 8));

  // print memory
  checkMemory(true);
}

void loop() {
  unsigned long start = millis();
  copier.copy();
  unsigned long end = millis();
  total_time_ms += (end - start);
  total_frames += frames;

  if (counter++ >= 1000) {
    Serial.print("Frames per second: ");
    Serial.println((float)total_frames / (float)total_time_ms * 1000.0);
    counter = 0;
  }
}
