/**
 * @file sd-measure-fps.ino
 * @brief Benchmarks real, achievable video playback throughput for a
 * local AVI/MP4/MPG file on the Hosyond 2.8" ESP32-S3 Display (see that
 * board's audio-out/lcd-test/player-sdmmc examples), using
 * LCDBoardESP32S3_2_8Display/OutputTinyGPU as the default display output:
 * no audio decode, no PacedVideoOutput pacing/audio clock - just demux ->
 * decode -> display, back to back, as fast as CodecCopy can feed bytes
 * in. Reports the average fps actually achieved (frames decoded / wall-
 * clock time spent on them), plus a breakdown by frame type (I vs P).
 *
 * The number this answers is "how fast can this board actually play this
 * file if nothing were pacing it" - exactly the ceiling PacedVideoOutput
 * itself needs to stay under (see its own inputFPS()/outputFPS()/
 * avgFrameMs() diagnostics, and the "Audio/Video Synchronization" wiki
 * chapter) to avoid ever falling behind and dropping/resyncing during
 * real playback with audio. Run this first against your own file/board
 * to know what frame rate is actually achievable before picking a
 * PacedVideoOutput::setSchedulingDelayMs()/setQueueBytes()/... tuning for
 * one of the sd-*-video.ino playback examples.
 *
 * Generic across every container/codec this library ships a decoder for:
 * MultiVideoDemuxerFull auto-detects AVI/MP4/MPG from the file's own
 * signature, MultiVideoDecoderFull auto-detects H264/MJPEG/MPEG-1 from
 * the container's parsed format (falling back to content-sniffing if the
 * container doesn't report one) - point file_path at any file those
 * cover and this just works, no per-format sketch needed.
 *
 * OutputFPSMeter (AudioTools/Video/OutputFPSMeter.h) wraps the decoder
 * (not the final pixel target): it times each write() call and
 * classifies it I vs P via the decoder's own isKeyFrame(), the same way
 * PacedVideoOutput classifies frames for its own frameCountI()/
 * frameCountP() stats - see its own class comment for the full picture.
 * tftOutput (a real display) is the decoder's own output below, so - as
 * that class comment explains - every decoder pushes each picture to it
 * synchronously and OutputTinyGPU's own SPI/QSPI write blocks until the
 * transfer actually completes, meaning the reported numbers already
 * include real display cost, not just decode. Swap tftOutput for
 * NullVideoOutput (commented out below) instead if you want to isolate
 * pure codec throughput from display cost.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/TinyGPU
 * - https://github.com/pschatzmann/TinyH264
 * - https://github.com/pschatzmann/TinyMPG
 * - https://github.com/pschatzmann/TinyJPEG
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include <SD_MMC.h>

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/CodecJPEG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/MultiVideoDecoderFull.h"
#include "AudioTools/Video/MultiVideoDemuxerFull.h"
#include "AudioTools/Video/OutputFPSMeter.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "TinyGPU/Boards.h"

// ---- File on the SD card to measure - any .avi/.mp4/.mpg this library
// can decode ----
const char* file_path = "/Videos/output176x144.avi";
//const char* file_path = "/Videos/output176x144.mp4"; // x264
//const char* file_path = "/Videos/output176x144-mjpeg.avi"; // mjpeg
//const char* file_path = "/Videos/output176x144.mpg";

// Uncomment to isolate pure codec throughput from display cost (see the
// file comment above) - swap tftOutput for this in videoDecoder.setOutput()
// below if you use it.
// class NullVideoOutput : public VideoOutput {
//  public:
//   size_t write(const uint8_t *data, size_t len) override { return len; }
// };
// NullVideoOutput nullOutput;

LCDBoardESP32S3_2_8Display board;
OutputTinyGPU tftOutput(board);  // default output target - see file comment
// Only used to bring up the shared SD_MMC bus (cfg.sdmmc_active below) -
// this sketch decodes no audio, so the codec chip itself stays idle.
AudioBoardStream out(audio_driver::ESP32S3HosyondDisplay);

MultiVideoDecoderFull videoDecoder;
OutputFPSMeter meter(videoDecoder);
MultiVideoDemuxerFull demuxer;  // auto-detects AVI/MP4/MPG

File file;
CodecCopy copier(demuxer, file);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sdmmc_active = true;  // board's begin() calls SD_MMC.setPins()+begin()
  if (!out.begin(cfg)) {
    Serial.println("AudioBoardStream begin() failed");
    return;
  }

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
  tftOutput.setScaleToFit(true);
  tftOutput.setVideoInfoSource(demuxer);

  videoDecoder.setOutput(tftOutput);
  // Exact codec detection from the container's own parsed format, same
  // pattern as every sd-*-video.ino example - falls back to content-
  // sniffing (VideoDecoder::isValid()) for any container that doesn't
  // report one.
  videoDecoder.setVideoInfoSource(demuxer);
  if (!videoDecoder.begin()) {
    Serial.println("MultiVideoDecoderFull begin() failed");
    return;
  }

  // No setOutputAudio() call - any audio track is still parsed by the
  // demuxer (chunk headers etc.), its payload bytes just go nowhere.
  // This sketch measures video decode(+display) throughput only.
  demuxer.setOutputVideo(meter);
  if (!demuxer.begin()) {
    Serial.println("MultiVideoDemuxerFull begin() failed");
    return;
  }

  Serial.println("Decoding at full speed (no pacing, no audio) ...");
}

void loop() {
  static uint32_t diagLast = 0;
  if (file && millis() - diagLast > 1000) {
    meter.logTo(Serial);
    diagLast = millis();
  }

  // file is only ever truthy up to the point it's closed below - once
  // that happens this whole block is skipped on every subsequent loop()
  // call, so "Done"/the final report print exactly once.
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
    Serial.println("---- final result ----");
    meter.logTo(Serial);
  }
}
