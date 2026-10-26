/**
 * @file sd-mts-h264-video.ino
 * @brief Plays both the audio (AAC) and video (H.264) tracks of a local
 * MPEG Transport Stream (.ts) file from the microSD card of the Hosyond
 * 2.8" ESP32-S3 Display (see that board's audio-out/lcd-test/player-sdmmc
 * examples): demuxes it live with DemuxerMTS, decodes the video track
 * with H264Decoder (TinyH264, https://github.com/pschatzmann/TinyH264 -
 * pure software, works on any board) and displays the result live on the
 * ILI9341 panel, while playing the audio track through the ES8311/FM8002E
 * speaker path via AACDecoderHelix.
 *
 * Driven through AudioTools/Video/VideoPlayer.h instead of wiring
 * CodecCopy, PacedVideoOutput, EncodedAudioStream and AudioTimeSourceStream
 * together by hand - see VideoPlayer's own class comment for the pipeline
 * it replaces (identical end to end; VideoPlayer just owns the wiring, and
 * its copy() already keeps the video schedule's fps in sync with the
 * demuxer's own parsed rate, so no manual polling is needed in loop()
 * either).
 *
 * DemuxerMTS is a *streaming* (forward-only) demuxer, same as
 * DemuxerAVI/DemuxerMP4/DemuxerMPG - it does not need a seekable source,
 * so a File read sequentially works fine (the same pattern also works
 * from a live HTTP download instead of a File). Unlike DemuxerMP4, there
 * is no "faststart" requirement or upfront metadata pass: it discovers
 * the video/audio track PIDs and codecs from the stream's own PAT/PMT as
 * it arrives (see ContainerMTS.h's own class comment).
 *
 * @note transcode a file e.g. with:
 *   ffmpeg -i in.mkv -vf "scale=176:144,fps=15" -pix_fmt yuv420p -c:v libx264 \
 *     -profile:v baseline -level 3.0 -g 15 -crf 28 -c:a aac \
 *     -f mpegts output176x144.ts
 * Baseline profile is required (TinyH264 is a baseline-only decoder - no
 * B-frames, no CABAC); `-g 15` keeps GOPs short so a dropped/resynced
 * backlog recovers quickly. See the Video-Playback wiki page's
 * H264Decoder section for lower-power encode variants and measured
 * numbers - the same guidance applies here since it's the same decoder,
 * just fed by a different container.
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
#include "AudioTools/AudioCodecs/ContainerMTS.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "AudioTools/Video/VideoPlayer.h"
#include "TinyGPU/Boards.h"
#include <SD_MMC.h>

// ---- File on the SD card to play ----
const char *file_path = "/Videos/output176x144.ts";

DemuxerMTS mtsDemuxer;
LCDBoardESP32S3_2_8Display board;
OutputTinyGPU tftOutput(board);
AudioBoardStream out(audio_driver::ESP32S3HosyondDisplay);
VideoPlayer player(mtsDemuxer, tftOutput, out);

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
    stop();
  }
  out.setVolume(0.4f);

  file = SD_MMC.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    stop();
  }

  if (!board.begin()) {
    Serial.println("OutputTinyGPU begin() failed");
    stop();
  }
  tftOutput.setRotation(DisplayRotation::kLandscape);
  // On: upscale the decoded frame to fill the 320x240 panel - costs more
  // render time (more pixels to convert/write) than leaving this off. See
  // PacedVideoOutput's avgFrameMs()/setScaleSingleBuffer() if render
  // time needs to come back down.
  tftOutput.setScaleToFit(true);

  player.addVideoDecoder(h264Decoder);
  player.addAudioDecoder(aacDecoder, "audio/aac");

  // This file has a real audio track - VideoPlayer schedules video against
  // it by default (see VideoPlayer's class comment's "Audio clock"
  // section).

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
  // No MTS-specific hardware timing data yet to tune this against for
  // this particular file - increase if drops/resyncs are too frequent
  // for your own video/board.
  player.setQueueBytes(40 * 1024);

  if (!player.begin(file)) {
    Serial.println("VideoPlayer begin() failed");
    stop();
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
