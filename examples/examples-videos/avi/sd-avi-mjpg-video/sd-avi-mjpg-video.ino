/**
 * @file sd-avi-mjpg-video.ino
 * @brief Plays a local .avi file's audio + Motion-JPEG video tracks from
 * the microSD card of the Hosyond 2.8" ESP32-S3 Display (see that board's
 * audio-out/lcd-test/player-sdmmc examples): DemuxerAVI demuxes,
 * MJPEGDecoder (wraps https://github.com/pschatzmann/TinyJPEG) decodes video
 * to the ILI9341 panel, DecoderHelix auto-detects and decodes the audio
 * (PCM/AAC/MP3 all valid in AVI) to the ES8311/FM8002E speaker path via
 * AudioBoardStream.
 *
 * Audio/video sync: DemuxerAVI dispatches audio/video as fast as bytes can
 * be parsed - all real pacing happens in videoSyncTask (PacedVideoOutput,
 * see Video/PacedVideoOutput.h), which sits between the demuxer and
 * mjpegDecoder. It buffers each decoded frame and renders it (MJPEG decode
 * + panel refresh) from its own background task, timed against audioClock
 * - an AudioTimeSourceStream inserted between multiDecoder and
 * AudioBoardStream that turns "how many decoded PCM bytes have reached
 * the audio device so far" into an elapsed-ms clock, so video is paced to
 * how far audio has actually played rather than a separate wall-clock
 * schedule. Because this all happens off the demuxer's own loop() call, a
 * slow decode/display frame never delays the next audio chunk.
 * videoSyncTask's own diagnostics (frameCount()/avgFrameMs()/inputFPS()/
 * outputFPS()/...) cover throughput monitoring - see logInfo() - so no
 * separate VideoFrameMeter is needed.
 *
 * Pipeline: File (SD_MMC) -> CodecCopy -> DemuxerAVI (demux)
 *   -> EncodedAudioStream (DecoderHelix) -> AudioTimeSourceStream (audio
 *   clock) -> AudioBoardStream (audio)
 *   \-> PacedVideoOutput (buffer + schedule) -> MJPEGDecoder ->
 *   OutputTinyGPU (video, own background task)
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
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include <SD_MMC.h>

#include "AudioTools/Video/CodecJPEG.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "TinyGPU/Boards.h"

// ---- File on the SD card to play ----
const char* file_path = "/Videos/output176x144-mjpeg.avi";

MJPEGDecoder mjpegDecoder;
// 3rd arg: scheduling delay compensating for AudioBoardStream's own
// output buffering (cfg.buffer_size*cfg.buffer_count) - see
// PacedVideoOutput::setSchedulingDelayMs(); ~115ms matches the ~20KB
// output buffer below - tune if you change either.
PacedVideoOutput videoSyncTask(mjpegDecoder, 0, 115);
LCDBoardESP32S3_2_8Display board;
OutputTinyGPU tftOutput(board);
// Qualified deliberately: TinyGPU.h (pulled in by OutputTinyGPU.h) and
// I2SCodecStream.h (pulled in by AudioBoardStream.h) each apply their own
// "using namespace" globally, and both libraries happen to name their
// board for this exact display "ESP32S3HosyondDisplay" - tinygpu::
// ESP32S3HosyondDisplay is a *type alias* for LCDBoardESP32S3_2_8Display
// (unrelated to the board object above), audio_driver::
// ESP32S3HosyondDisplay is the AudioBoard *object* actually needed here -
// left unqualified, the two collide and the sketch fails to compile with
// "reference to 'ESP32S3HosyondDisplay' is ambiguous".
AudioBoardStream out(audio_driver::ESP32S3HosyondDisplay);
AudioTimeSourceStream audioClock(
    out);  // decoded-PCM-bytes-based playback clock
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(
    &audioClock, &multiDecoder);  // decodes PCM/AAC/MP3 -> audioClock -> out
File file;
DemuxerAVI aviDemuxer;
CodecCopy copier(aviDemuxer, file);

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

  multiDecoder.setMimeSource(aviDemuxer);
  if (!audioOut.begin()) {
    Serial.println("EncodedAudioStream begin() failed");
    stop();
  }

  mjpegDecoder.setOutput(tftOutput);
  tftOutput.setVideoInfoSource(mjpegDecoder);

  if (!mjpegDecoder.begin()) {
    Serial.println("MJPEGDecoder begin() failed");
    stop();
  }

  videoSyncTask.setAudioClock(audioClock);
  // Pin the render task to core 0 - loop() (SD reads + demuxing + the
  // blocking out.write() into I2S) runs on core 1 by default. Without
  // this, the video task could land on core 1 too and preempt loop() for
  // the length of a slow render call. Call before begin()/first write().
  videoSyncTask.setTaskParameters(4096, 2, 0);
  // call before begin()/first write(). No MJPEG-specific hardware timing
  // data yet to tune this against - increase if write() blocks/resyncs are
  // too frequent for your own video/board.
  videoSyncTask.setQueueBytes(40 * 1024);
  // Every MJPEG frame is classified as a keyframe (see MJPEGDecoder::
  // isKeyFrame()), so the default (4) would count total queued frames, not
  // GOPs of backlog - firing on a handful of merely-queued frames instead
  // of a real backlog. Disabled (0); the byte-fill (default 80% via
  // setResyncQueueFillFraction()) and lateness (setResyncThresholdMs())
  // triggers already measure real backlog severity regardless of codec.
  videoSyncTask.setMaxQueuedIFrames(0);

  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setOutputVideo(videoSyncTask);
  if (!aviDemuxer.begin()) {
    Serial.println("DemuxerAVI begin() failed");
    stop();
  }
}

void logInfo() {
  Serial.print("input fps: ");
  Serial.print(videoSyncTask.inputFPS());
  Serial.print(" / output fps: ");
  Serial.println(videoSyncTask.outputFPS());
  Serial.print("avg render ms (decode+convert+SPI): ");
  Serial.println(videoSyncTask.avgFrameMs());
  Serial.print("frames: ");
  Serial.print(videoSyncTask.frameCount());
  Serial.print(" / dropped: ");
  Serial.print(videoSyncTask.droppedFrameCount());
  size_t queueCapacity = videoSyncTask.queueCapacityBytes();
  Serial.print(" / queue: ");
  Serial.print((unsigned)videoSyncTask.queuedBytes());
  Serial.print("/");
  Serial.print((unsigned)queueCapacity);
  Serial.print(" bytes (");
  Serial.print(queueCapacity > 0
                   ? 100.0f * videoSyncTask.queuedBytes() / queueCapacity
                   : 0.0f);
  Serial.println("% full)");
#ifdef ESP32
  // Internal heap
  size_t heapFree = ESP.getFreeHeap();
  size_t heapTotal = ESP.getHeapSize();
  size_t heapUsed = heapTotal - heapFree;

  // PSRAM
  size_t psramFree = ESP.getFreePsram();
  size_t psramTotal = ESP.getPsramSize();
  size_t psramUsed = psramTotal - psramFree;

  Serial.printf("Heap:  total=%u, used=%u, free=%u bytes\n", heapTotal,
                heapUsed, heapFree);

  Serial.printf("PSRAM: total=%u, used=%u, free=%u bytes\n", psramTotal,
                psramUsed, psramFree);

  // Largest currently allocatable blocks
  Serial.printf("Largest heap block:  %u bytes\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

  Serial.printf("Largest PSRAM block: %u bytes\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
#endif
}

void loop() {
  static uint32_t diagLast = 0;
  if (millis() - diagLast > 1000) {
    logInfo();
    diagLast = millis();
  }
  // AVI's header (which carries the nominal frame rate) always precedes
  // the movi data - so this becomes available strictly before the first
  // video frame ever reaches videoSyncTask, no gating needed beyond
  // "call it every time, cheaply, until it's non-zero".
  float fps = aviDemuxer.getVideoInfo().fps;
  if (fps > 0) videoSyncTask.setFps(fps);

  if (file) {
    if (copier.copy() == 0) {
      Serial.println("Done");
      file.close();
      stop();
    }
  }
}
