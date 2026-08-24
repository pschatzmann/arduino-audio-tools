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
 * Audio/video sync: DemuxerMP4 dispatches audio/video as fast as bytes
 * can be parsed - all real pacing happens in videoSyncTask
 * (PacedVideoOutput, see Video/PacedVideoOutput.h), which sits
 * between the demuxer and h264Decoder. It buffers each decoded frame and
 * renders it (H.264 decode + panel refresh) from its own background
 * task, timed against audioClock - an AudioTimeSourceStream inserted
 * between aacDecoder and AudioBoardStream that turns "how many decoded
 * PCM bytes have reached the audio device so far" into an elapsed-ms
 * clock, so video is paced to how far audio has actually played rather
 * than a separate wall-clock schedule. Because this all happens off the
 * demuxer's own loop() call, a slow decode/display frame never delays
 * the next audio chunk. videoSyncTask's own diagnostics
 * (frameCount()/avgFrameMs()/inputFPS()/outputFPS()/...) cover
 * throughput monitoring - see logInfo() - so no separate meter is
 * needed.
 *
 * Pipeline: File (SD_MMC) -> CodecCopy -> DemuxerMP4 (demux)
 *   -> EncodedAudioStream (AACDecoderHelix) -> AudioTimeSourceStream
 *   (audio clock) -> AudioBoardStream (audio)
 *   \-> PacedVideoOutput (buffer + schedule) -> H264Decoder (H.264
 *   decode -> RGB565) -> OutputTinyGPU (video, own background task)
 *
 * DemuxerMP4 is a *streaming* (forward-only) demuxer - it does not need
 * a seekable source, so a File read sequentially with CodecCopy works
 * fine (the same pattern also works from a live HTTP download instead
 * of a File).
 *
 * On an ESP32-S3 board, swap H264Decoder for H264DecoderESP32S3
 * (AudioTools/Video/CodecH264ESP32S3.h) to use the hardware/esp_h264
 * backend (https://github.com/pschatzmann/ESP32S3-h264) instead - same
 * setOutput() surface, no other change needed below.
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
#include "TinyGPU/Boards.h"
#include <SD_MMC.h>

// ---- File on the SD card to play ----
const char *file_path = "/Videos/output.mp4";

H264Decoder h264Decoder;
// 3rd arg: scheduling delay compensating for AudioBoardStream's own
// output buffering (cfg.buffer_size*cfg.buffer_count) - see
// PacedVideoOutput::setSchedulingDelayMs(); ~115ms matches the ~20KB
// output buffer below - tune if you change either.
PacedVideoOutput videoSyncTask(h264Decoder, 0, 115);
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
AACDecoderHelix aacDecoder;
EncodedAudioStream audioOut(
    &audioClock, &aacDecoder);  // decodes AAC -> audioClock -> out

DemuxerMP4 mp4Demuxer;

File file;
CodecCopy copier(mp4Demuxer, file);

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

  if (!audioOut.begin()) {
    Serial.println("EncodedAudioStream begin() failed");
    return;
  }

  h264Decoder.setOutput(tftOutput);
  tftOutput.setVideoInfoSource(h264Decoder);
  if (!h264Decoder.begin()) {
    Serial.println("H264Decoder begin() failed");
    return;
  }

  videoSyncTask.setAudioClock(audioClock);
  // Pin the render task to core 0 - loop() (SD reads + demuxing + the
  // blocking out.write() into I2S) runs on core 1 by default. Without
  // this, the video task could land on core 1 too and preempt loop() for
  // the length of a slow render call. Call before begin()/first write().
  videoSyncTask.setTaskParameters(4096, 2, 0);
  // No H264-specific hardware timing data yet to tune this against for
  // this particular file - increase if drops/resyncs are too frequent
  // for your own video/board.
  videoSyncTask.setQueueBytes(40 * 1024);
  videoSyncTask.setQueueUsePSRAM(true);

  mp4Demuxer.setOutputAudio(audioOut);
  mp4Demuxer.setOutputVideo(videoSyncTask);  // buffer + schedule via videoSyncTask
  if (!mp4Demuxer.begin()) {
    Serial.println("DemuxerMP4 begin() failed");
    return;
  }
}

void logInfo() {
  Serial.print("input fps: ");
  Serial.print(videoSyncTask.inputFPS());
  Serial.print(" / output fps: ");
  Serial.println(videoSyncTask.outputFPS());
  Serial.print("avg render ms - I: ");
  Serial.print(videoSyncTask.avgIFrameMs());
  Serial.print(" / P: ");
  Serial.print(videoSyncTask.avgPFrameMs());
  Serial.print(" / overall: ");
  Serial.println(videoSyncTask.avgFrameMs());
  Serial.print("frames - I: ");
  Serial.print(videoSyncTask.frameCountI());
  Serial.print(" / P: ");
  Serial.print(videoSyncTask.frameCountP());
  Serial.print(" / dropped P: ");
  Serial.print(videoSyncTask.droppedFrameCount());
  Serial.print(" / dropped I: ");
  Serial.print(videoSyncTask.droppedIFrameCount());
  Serial.print(" / queued I: ");
  Serial.print(videoSyncTask.queuedIFrameCount());
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
  size_t heapFree = ESP.getFreeHeap();
  size_t heapTotal = ESP.getHeapSize();
  size_t heapUsed = heapTotal - heapFree;

  size_t psramFree = ESP.getFreePsram();
  size_t psramTotal = ESP.getPsramSize();
  size_t psramUsed = psramTotal - psramFree;

  Serial.printf("Heap:  total=%u, used=%u, free=%u bytes\n", heapTotal,
                heapUsed, heapFree);
  Serial.printf("PSRAM: total=%u, used=%u, free=%u bytes\n", psramTotal,
                psramUsed, psramFree);
#endif
}

void loop() {
  static uint32_t diagLast = 0;
  if (millis() - diagLast > 1000) {
    logInfo();
    diagLast = millis();
  }

  // The moov box (which carries the nominal frame rate via its track's
  // timescale/sample_delta) always precedes mdat - so this becomes
  // available strictly before the first video frame ever reaches
  // videoSyncTask (see DemuxerMP4::getVideoInfo()'s own comment), no
  // gating needed beyond "call it every time, cheaply, until it's
  // non-zero".
  float fps = mp4Demuxer.getVideoInfo().fps;
  if (fps > 0) videoSyncTask.setFps(fps);

  if (file) {
    if (copier.copy() == 0) {
      Serial.println("Done");
      file.close();
    }
  }
}
