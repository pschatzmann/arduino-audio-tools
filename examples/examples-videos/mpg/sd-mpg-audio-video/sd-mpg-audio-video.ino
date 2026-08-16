/**
 * @file sd-mpg-audio-video.ino
 * @brief Plays both the audio and video (MPEG-1) tracks of a local MPEG-1
 * Program Stream (.mpg) file on an SD card: demuxes it live with
 * DemuxerMPG, decodes the video track with MPGDecoder (TinyMPG,
 * https://github.com/pschatzmann/TinyMPG - pure software, works on any
 * board) and displays the result live on a TFT screen with TFT_eSPI, while
 * playing the audio track (a raw MPEG-1 Layer I/II/III elementary stream)
 * through I2S via MP3DecoderHelix - the same choice ContainerMPG.h's own
 * file comment uses.
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerMPG
 * (demux)
 *   -> EncodedAudioStream (MP3DecoderHelix) -> I2SStream (audio)
 *   \-> MPGDecoder (MPEG-1 decode -> RGB565) -> OutputTFT_eSPI (draw) (video)
 *
 * DemuxerMPG is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with StreamCopy (the same
 * way http-client-mpg.ino feeds it from a live HTTP download) works fine.
 * See sd-mpg-audio.ino for an audio-only version of this same file,
 * sd-mpg-video.ino for video-only, and http-client-mpg.ino for the network
 * (HTTP) equivalent.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/Bodmer/TFT_eSPI (configure your display's pins/driver
 *   in that library's User_Setup.h - not done in this sketch)
 * - https://github.com/pschatzmann/TinyMPG
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputTFT_eSPI.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/video.mpg";

TFT_eSPI tft = TFT_eSPI();
MPGDecoder mpgDecoder;
OutputTFT_eSPI tftOutput(tft);
I2SStream i2s;
MP3DecoderHelix mp3Decoder;
EncodedAudioStream audioOut(&i2s, &mp3Decoder);  // decodes MP3/MP2 -> I2S

DemuxerMPG mpgDemuxer;
EncodedAudioOutput mpgInput(&mpgDemuxer);  // bridges raw file bytes -> DemuxerMPG::write()

File file;
StreamCopy copier(mpgInput, file);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
    return;
  }
  file = SD.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    return;
  }

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);
  audioOut.begin();

  mpgDecoder.setOutput(tftOutput);  // RGB565 (the default) matches
                                     // pushImage()'s expected format
  tftOutput.setVideoInfoSource(mpgDecoder);
  mpgDecoder.begin();

  mpgDemuxer.setOutputAudio(audioOut);
  mpgDemuxer.setOutputVideo(mpgDecoder);
  mpgInput.begin();
}

void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
