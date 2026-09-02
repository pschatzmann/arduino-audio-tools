/**
 * @file sd-avi-mjpg-video.ino
 * @brief Plays a local .avi file's audio + Motion-JPEG video tracks from
 * the microSD card of the Hosyond 2.8" ESP32-S3 Display (see that board's
 * audio-out/lcd-test/player-sdmmc examples): DemuxerAVI demuxes,
 * MJPEGDecoder (wraps https://github.com/pschatzmann/TinyJPEG) decodes video
 * to the ILI9341 panel, mp3Decoder/aacDecoder/wavDecoder auto-select and
 * decode the audio (PCM/AAC/MP3 all valid in AVI) to the ES8311/FM8002E
 * speaker path via AudioBoardStream.
 *
 * Driven through AudioTools/Video/VideoPlayer.h instead of wiring
 * CodecCopy, PacedVideoOutput, EncodedAudioStream and AudioTimeSourceStream
 * together by hand - see VideoPlayer's own class comment for the pipeline
 * it replaces (identical end to end; VideoPlayer just owns the wiring, and
 * its copy() already keeps the video schedule's fps in sync with the
 * demuxer's own parsed rate, so no manual polling is needed in loop()
 * either). mp3/aac/wav are registered explicitly below since VideoPlayer's
 * built-in audio multi-decoder starts empty (see its own class comment) -
 * together they match what DecoderHelix bundles by default.
 *
 * Notes:
 * - Video track must be Motion-JPEG (fourcc MJPG), baseline (non-
 *   progressive) JPEG frames - see TinyJPEG's own README (it wraps ChaN's
 *   TJpgDec) for its format limits. Every MJPEG frame is a complete,
 *   independently decodable image
 *   (no inter-frame prediction) - MJPEGDecoder::isKeyFrame() therefore
 *   always returns true (MJPEG is effectively "all I-frames"), never
 *   dropped by PacedVideoOutput's proactive catch-up path; a persistent
 *   backlog instead falls to its byte-fill/lateness resync (see
 *   setMaxQueuedIFrames(0) below).
 * - e.g. ffmpeg -i input.mkv -vf "scale=176:144,fps=7" -c:v mjpeg -q:v 5
 *   output176x144-mjpeg.avi
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/TinyGPU
 * - https://github.com/pschatzmann/TinyJPEG
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include <SD_MMC.h>

#include "AudioTools/Video/CodecJPEG.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "AudioTools/Video/VideoPlayer.h"
#include "TinyGPU/Boards.h"

// ---- File on the SD card to play ----
const char* file_path = "/Videos/output176x144-mjpeg.avi";

DemuxerAVI aviDemuxer;
LCDBoardESP32S3_2_8Display board;
OutputTinyGPU tftOutput(board);
AudioBoardStream out(audio_driver::ESP32S3HosyondDisplay);
VideoPlayer player(aviDemuxer, tftOutput, out);

MJPEGDecoder mjpegDecoder;
MP3DecoderHelix mp3Decoder;
AACDecoderHelix aacDecoder;
WAVDecoder wavDecoder;

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
  Serial.print("opened ");
  Serial.print(file_path);
  Serial.print(": ");
  Serial.print((unsigned)file.size());
  Serial.println(" bytes");

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

  player.addVideoDecoder(mjpegDecoder);
  player.addAudioDecoder(mp3Decoder, "audio/mpeg");
  player.addAudioDecoder(aacDecoder, "audio/aac");
  player.addAudioDecoder(wavDecoder, "audio/vnd.wave");

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
  // No MJPEG-specific hardware timing data yet to tune this against -
  // increase if write() blocks/resyncs are too frequent for your own
  // video/board.
  player.setQueueBytes(40 * 1024);
  // Every MJPEG frame is classified as a keyframe (see MJPEGDecoder::
  // isKeyFrame()), so the default (4) would count total queued frames, not
  // GOPs of backlog - firing on a handful of merely-queued frames instead
  // of a real backlog. Disabled (0); the byte-fill (default 80% via
  // setResyncQueueFillFraction()) and lateness (setResyncThresholdMs())
  // triggers already measure real backlog severity regardless of codec.
  player.setMaxQueuedIFrames(0);

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
    stop();
  }
}
