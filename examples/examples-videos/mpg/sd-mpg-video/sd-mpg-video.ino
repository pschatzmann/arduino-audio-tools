/**
 * @file sd-mpg-video.ino
 * @brief Plays both the audio and video (MPEG-1) tracks of a local MPEG-1
 * Program Stream (.mpg) file from the microSD card of the Hosyond 2.8"
 * ESP32-S3 Display (see that board's audio-out/lcd-test/player-sdmmc
 * examples): demuxes it live with DemuxerMPG, decodes the video track
 * with MPGDecoder (TinyMPG, https://github.com/pschatzmann/TinyMPG - pure
 * software, works on any board) and displays the result live on the
 * ILI9341 panel, while playing the audio track (a raw MPEG-1 Layer
 * I/II/III elementary stream) through the ES8311/FM8002E speaker path.
 * mp3/aac/wav/mp2 are registered explicitly below since VideoPlayer's
 * built-in audio multi-decoder starts empty (see its own class comment) -
 * together they match what DecoderHelix bundles by default, plus MP2Decoder
 * for DemuxerMPG's Layer II mime (see the addAudioDecoder() call below).
 *
 * Driven through AudioTools/Video/VideoPlayer.h instead of wiring
 * CodecCopy, PacedVideoOutput, EncodedAudioStream and AudioTimeSourceStream
 * together by hand - see VideoPlayer's own class comment for the pipeline
 * it replaces (identical end to end; VideoPlayer just owns the wiring, and
 * its copy() already keeps the video schedule's fps in sync with the
 * demuxer's own parsed rate, so no manual polling is needed in loop()
 * either).
 *
 * @note transcode file e.g. with ffmpeg -i Casablanca.1942.720.x264.YIFY.mkv
 -vf "scale=176:144,fps=24" -c:v mpeg1video -g 15 -bf 0 -q:v 5 -c:a mp2 -b:a
 128k output176x144-v2.mpg
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/TinyGPU
 * - https://github.com/pschatzmann/TinyMPG
 * - https://github.com/pschatzmann/TinyMP2
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include <SD_MMC.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/AudioCodecs/CodecMP2.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "AudioTools/Video/VideoPlayer.h"
#include "TinyGPU/Boards.h"

// ---- File on the SD card to play ----
const char* file_path = "/Videos/output176x144.mpg";

DemuxerMPG mpgDemuxer;
LCDBoardESP32S3_2_8Display board;
OutputTinyGPU tftOutput(board);
AudioBoardStream out(audio_driver::ESP32S3HosyondDisplay);
VideoPlayer player(mpgDemuxer, tftOutput, out);

MPGDecoder mpgDecoder;
MP3DecoderHelix mp3Decoder;
AACDecoderHelix aacDecoder;
WAVDecoder wavDecoder;
MP2Decoder mp2Decoder;

File file;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sdmmc_active = true;  // board's begin() calls SD_MMC.setPins()+begin()
  cfg.buffer_size = 1024;
  cfg.buffer_count = 20;  // 1024*20 = 20KB output buffer
  if (!out.begin(cfg)) {
    Serial.println("AudioBoardStream begin() failed");
    return;
  }
  out.setVolume(0.4f);

  file = SD_MMC.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    return;
  }

  if (!board.begin()) {
    Serial.println("OutputTinyGPU begin() failed");
    return;
  }
  tftOutput.setRotation(DisplayRotation::kLandscape);
  // On: upscale the decoded frame to fill the 320x240 panel - costs more
  // render time (more pixels to convert/write) than leaving this off. See
  // PacedVideoOutput's avgFrameMs()/setScaleSingleBuffer() if render
  // time needs to come back down.
  tftOutput.setScaleToFit(true);

  player.addVideoDecoder(mpgDecoder);
  // DemuxerMPG::mime() reports "audio/mpeg; codecs=\"mpeg1-layer2\""
  // specifically for Layer II, falling back to the plain MP3 mime
  // otherwise (see its own comment) - MultiDecoder::selectDecoder()
  // matches that exact string first, so this makes the player pick
  // mp2Decoder for Layer II while still falling back to mp3Decoder/
  // aacDecoder/wavDecoder (matched by the plain base mime) for anything
  // else.
  player.addAudioDecoder(mp3Decoder, "audio/mpeg");
  player.addAudioDecoder(aacDecoder, "audio/aac");
  player.addAudioDecoder(wavDecoder, "audio/vnd.wave");
  player.addAudioDecoder(mp2Decoder, "audio/mpeg; codecs=\"mpeg1-layer2\"");

  // This file has a real audio track - schedule video against it instead
  // of wall-clock time (see VideoPlayer's class comment's "Audio clock"
  // section).
  player.setUseAudioClock(true);

  // delta frames are too slow, so just ignore them ?
  player.setIgnorePFrames(true);
  // Compensates for AudioBoardStream's own output buffering
  // (cfg.buffer_size*cfg.buffer_count) - see
  // PacedVideoOutput::setSchedulingDelayMs(); ~115ms matches the ~20KB
  // output buffer above - tune if you change either.
  player.setSchedulingDelayMs(115);
  // Pin the render task to core 0 - loop() (SD reads + demuxing + the
  // blocking out.write() into I2S) runs on core 1 by default. Without
  // this, the video task could land on core 1 too and preempt loop() for
  // the length of a slow render call. Call before begin()/first write().
  player.setTaskParameters(4096, 2, 0);
  player.setQueueBytes(40 * 1024);
  player.setQueueUsePSRAM(true);

  if (!player.begin(file)) {
    Serial.println("VideoPlayer begin() failed");
    return;
  }
}

void loop() {
  static uint32_t diagLast = 0;
  if (millis() - diagLast > 1000) {
    player.logTo(Serial);
    diagLast = millis();
  }

  if (player.copy() == 0) {
    Serial.println("Done");
    file.close();
  }
}
