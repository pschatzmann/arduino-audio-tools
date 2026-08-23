/**
 * @file sd-mpg-audio-video.ino
 * @brief Plays both the audio and video (MPEG-1) tracks of a local MPEG-1
 * Program Stream (.mpg) file on an SD card: demuxes it live with
 * DemuxerMPG, decodes the video track with MPGDecoder (TinyMPG,
 * https://github.com/pschatzmann/TinyMPG - pure software, works on any
 * board) and displays the result live on an SPI TFT, while playing the
 * audio track (a raw MPEG-1 Layer I/II/III elementary stream) through I2S
 * via MP3DecoderHelix - the same choice ContainerMPG.h's own file comment
 * uses.
 *
 * Audio/video sync: DemuxerMPG dispatches audio/video as fast as bytes can
 * be parsed - all real pacing happens in videoSyncTask (VideoAudioSyncTask,
 * see Video/VideoAudioSyncTask.h), which sits between the demuxer and
 * mpgDecoder. It buffers each decoded frame and renders it (MPEG-1 decode
 * + panel refresh) from its own background task, timed against audioClock
 * - an AudioTimeSourceStream inserted between mp3Decoder and i2s that turns
 * "how many decoded PCM bytes have reached the audio device so far" into
 * an elapsed-ms clock, so video is paced to how far audio has actually
 * played rather than a separate wall-clock schedule. Because this all
 * happens off the demuxer's own loop() call, a slow decode/display frame
 * never delays the next audio chunk. videoSyncTask's own diagnostics
 * (frameCount()/avgFrameMs()/inputFPS()/outputFPS()/...) cover throughput
 * monitoring - see logInfo() - so no separate meter is needed.
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerMPG
 * (demux)
 *   -> EncodedAudioStream (MP3DecoderHelix) -> AudioTimeSourceStream (audio
 *   clock) -> I2SStream (audio)
 *   \-> VideoAudioSyncTask (buffer + schedule) -> MPGDecoder (MPEG-1
 *   decode -> RGB565) -> OutputTinyGPU (video, own background task)
 *
 * DemuxerMPG is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with StreamCopy (the same
 * way http-client-mpg.ino feeds it from a live HTTP download) works fine.
 * See sd-mpg-audio.ino for an audio-only version of this same file,
 * sd-mpg-video.ino for video-only, and http-client-mpg.ino for the network
 * (HTTP) equivalent.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/pschatzmann/TinyGPU (SPI/display pins below match its
 *   bouncing-ball example - adjust for your own wiring)
 * - https://github.com/pschatzmann/TinyMPG
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/video.mpg";

// ---- SPI / display pins (adjust for your wiring) ----
constexpr int8_t kPinMosi = 13;
constexpr int8_t kPinMiso = 12;
constexpr int8_t kPinSclk = 14;
constexpr int8_t kPinCs = 15;
constexpr int8_t kPinDc = 2;
constexpr int8_t kPinRst = -1;
constexpr int8_t kPinBacklight = 27;

ILI9341Driver<RGB565> tftDriver(SPI, kPinCs, kPinDc, kPinRst);
MPGDecoder mpgDecoder;
// 3rd arg: scheduling delay compensating for I2SStream's own output
// buffering (cfg.buffer_size*cfg.buffer_count) - see
// VideoAudioSyncTask::setSchedulingDelayMs(); tune to match your own
// I2S buffer configuration.
VideoAudioSyncTask videoSyncTask(mpgDecoder, 0, 100);
OutputTinyGPU tftOutput(tftDriver, kPinBacklight);
I2SStream i2s;
AudioTimeSourceStream audioClock(i2s);  // decoded-PCM-bytes-based playback clock
MP3DecoderHelix mp3Decoder;
EncodedAudioStream audioOut(&audioClock,
                             &mp3Decoder);  // decodes MP3/MP2 -> audioClock -> i2s

DemuxerMPG mpgDemuxer;
EncodedAudioOutput mpgInput(
    &mpgDemuxer);  // bridges raw file bytes -> DemuxerMPG::write()

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

  SPI.begin(kPinSclk, kPinMiso, kPinMosi, kPinCs);
  tftOutput.begin();

  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  if (!audioOut.begin()) {
    Serial.println("EncodedAudioStream begin() failed");
    return;
  }

  mpgDecoder.setOutput(tftOutput);
  tftOutput.setVideoInfoSource(mpgDecoder);
  if (!mpgDecoder.begin()) {
    Serial.println("MPGDecoder begin() failed");
    return;
  }

  videoSyncTask.setAudioClock(audioClock);
  // Pin the render task to core 0 - loop() (SD reads + demuxing + the
  // blocking audioOut.write() into I2S) runs on core 1 by default. Without
  // this, the video task could land on core 1 too and preempt loop() for
  // the length of a slow render call. Call before begin()/first write().
  videoSyncTask.setTaskParameters(4096, 2, 0);
  videoSyncTask.setQueueBytes(40 * 1024);
  videoSyncTask.setQueueUsePSRAM(true);

  mpgDemuxer.setOutputAudio(audioOut);
  mpgDemuxer.setOutputVideo(videoSyncTask);  // buffer + schedule via videoSyncTask
  mpgInput.begin();
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

  // The sequence_header (which carries the nominal frame rate) always
  // precedes any picture data - so this becomes available strictly before
  // the first video frame ever reaches videoSyncTask, no gating needed
  // beyond "call it every time, cheaply, until it's non-zero".
  float fps = mpgDemuxer.getVideoInfo().fps;
  if (fps > 0) videoSyncTask.setFps(fps);

  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
