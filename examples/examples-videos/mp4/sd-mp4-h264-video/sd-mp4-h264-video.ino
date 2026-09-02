/**
 * @file sd-mp4-h264-video.ino
 * @brief Plays both the audio (AAC) and video (H.264) tracks of a local
 * .mp4 file from the microSD card of the Hosyond 2.8" ESP32-S3 Display
 * (see that board's audio-out/lcd-test/player-sdmmc examples): demuxes
 * it live with DemuxerMP4, decodes the video track with H264Decoder
 * (TinyH264, https://github.com/pschatzmann/TinyH264 - pure software,
 * works on any board) and displays the result live on the ILI9341
 * panel, while playing the audio track through the ES8311/FM8002E
 * speaker path via AACDecoderHelix. Same "faststart" MP4 (moov before
 * mdat) DemuxerMP4 requires everywhere else, e.g.:
 *   ffmpeg -i in.mp4 -c:v libx264 -c:a aac -movflags +faststart out.mp4
 *
 * Driven through AudioTools/Video/VideoPlayer.h instead of wiring
 * CodecCopy, PacedVideoOutput, EncodedAudioStream and AudioTimeSourceStream
 * together by hand - see VideoPlayer's own class comment for the pipeline
 * it replaces (identical end to end; VideoPlayer just owns the wiring, and
 * its copy() already keeps the video schedule's fps in sync with the
 * demuxer's own parsed rate, so no manual polling is needed in loop()
 * either).
 *
 * DemuxerMP4 is a *streaming* (forward-only) demuxer - it does not need
 * a seekable source, so a File read sequentially works fine (the same
 * pattern also works from a live HTTP download instead of a File).
 *
 * On an ESP32-S3 board, swap H264Decoder for H264DecoderESP32S3
 * (AudioTools/Video/CodecH264ESP32S3.h) to use the hardware/esp_h264
 * backend (https://github.com/pschatzmann/ESP32S3-h264) instead - same
 * addVideoDecoder() surface, no other change needed below.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/TinyGPU
 * - https://github.com/pschatzmann/TinyH264
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "AudioTools/Video/VideoPlayer.h"
#include "TinyGPU/Boards.h"
#include <SD_MMC.h>

// ---- File on the SD card to play ----
const char *file_path = "/Videos/output.mp4";

DemuxerMP4 mp4Demuxer;
LCDBoardESP32S3_2_8Display board;
OutputTinyGPU tftOutput(board);
AudioBoardStream out(audio_driver::ESP32S3HosyondDisplay);
VideoPlayer player(mp4Demuxer, tftOutput, out);

H264Decoder h264Decoder;
AACDecoderHelix aacDecoder;

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

  player.addVideoDecoder(h264Decoder);
  player.addAudioDecoder(aacDecoder, "audio/aac");

  // This file has a real audio track - schedule video against it instead
  // of wall-clock time (see VideoPlayer's class comment's "Audio clock"
  // section).
  player.setUseAudioClock(true);

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
  // No H264-specific hardware timing data yet to tune this against for
  // this particular file - increase if drops/resyncs are too frequent
  // for your own video/board.
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
