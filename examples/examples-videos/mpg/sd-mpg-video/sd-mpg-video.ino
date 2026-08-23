/**
 * @file sd-mpg-video.ino
 * @brief Plays both the audio and video (MPEG-1) tracks of a local MPEG-1
 * Program Stream (.mpg) file from the microSD card of the Hosyond 2.8"
 * ESP32-S3 Display (see that board's audio-out/lcd-test/player-sdmmc
 * examples): demuxes it live with DemuxerMPG, decodes the video track
 * with MPGDecoder (TinyMPG, https://github.com/pschatzmann/TinyMPG - pure
 * software, works on any board) and displays the result live on the
 * ILI9341 panel, while playing the audio track (a raw MPEG-1 Layer
 * I/II/III elementary stream) through the ES8311/FM8002E speaker path via
 * DecoderHelix - a MultiDecoder that auto-selects the right decoder from
 * DemuxerMPG::mime(): MP2Decoder (TinyMP2,
 * https://github.com/pschatzmann/TinyMP2) for Layer II, its own built-in
 * MP3DecoderHelix for Layer III (DemuxerMPG::mime() only distinguishes
 * Layer II - see its own comment), or AAC/WAV if the file happens to
 * carry one of those instead.
 *
 * Audio/video sync: DemuxerMPG dispatches audio/video as fast as bytes can
 * be parsed - all real pacing happens in videoSyncTask (VideoAudioSyncTask,
 * see Video/VideoAudioSyncTask.h), which sits between the demuxer and
 * mpgDecoder. It buffers each decoded frame and renders it (MPEG-1 decode
 * + panel refresh) from its own background task, timed against audioClock
 * - an AudioTimeSourceStream inserted between multiDecoder and
 * AudioBoardStream that turns "how many decoded PCM bytes have reached
 * the audio device so far" into an elapsed-ms clock, so video is paced to
 * how far audio has actually played rather than a separate wall-clock
 * schedule. Because this all happens off the demuxer's own loop() call, a
 * slow decode/display frame never delays the next audio chunk.
 * videoSyncTask's own diagnostics (frameCount()/avgFrameMs()/inputFPS()/
 * outputFPS()/...) cover throughput monitoring - see logInfo() - so no
 * separate meter is needed.
 *
 * Pipeline: File (SD_MMC) -> CodecCopy -> DemuxerMPG (demux)
 *   -> EncodedAudioStream (DecoderHelix) -> AudioTimeSourceStream (audio
 *   clock) -> AudioBoardStream (audio)
 *   \-> VideoAudioSyncTask (buffer + schedule) -> MPGDecoder (MPEG-1
 *   decode -> RGB565) -> OutputTinyGPU (video, own background task)
 *
 * DemuxerMPG is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with CodecCopy works fine
 * (the same pattern also works from a live HTTP download instead of a
 * File).
 * @note transcode file e.g. with ffmpeg -i Casablanca.1942.720.x264.YIFY.mkv
 -vf "scale=176:144,fps=24" -c:v mpeg1video -g 1 -bf 0 -q:v 5 -c:a mp2 -b:a 128k
 output176x144-v2.mpg



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
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "TinyGPU/Boards.h"

// ---- File on the SD card to play ----
const char* file_path = "/Videos/output176x144.mpg";

MPGDecoder mpgDecoder;
// 3rd arg: scheduling delay compensating for AudioBoardStream's own
// output buffering (cfg.buffer_size*cfg.buffer_count) - see
// VideoAudioSyncTask::setSchedulingDelayMs(); ~115ms matches the ~20KB
// output buffer below - tune if you change either.
VideoAudioSyncTask videoSyncTask(mpgDecoder, 0, 115);
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
DecoderHelix
    multiDecoder;  // MP3/AAC/WAV built in - MP2Decoder added in setup()
MP2Decoder mp2Decoder;
EncodedAudioStream audioOut(
    &audioClock,
    &multiDecoder);  // decodes MP2/MP3/AAC/WAV -> audioClock -> out

DemuxerMPG mpgDemuxer;

File file;
CodecCopy copier(mpgDemuxer, file);

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
  // VideoAudioSyncTask's avgFrameMs()/setScaleSingleBuffer() if render
  // time needs to come back down.
  tftOutput.setScaleToFit(true);

  // DemuxerMPG::mime() reports "audio/mpeg; codecs=\"mpeg1-layer2\""
  // specifically for Layer II, falling back to the plain MP3 mime
  // otherwise (see its own comment) - MultiDecoder::selectDecoder()
  // matches that exact string first, so this makes multiDecoder pick
  // mp2Decoder for Layer II while still falling back to its own built-in
  // MP3DecoderHelix/AACDecoderHelix/WAVDecoder (matched by the plain
  // base mime) for anything else. Must be registered before the first
  // write() reaches multiDecoder.
  multiDecoder.setMimeSource(mpgDemuxer);
  multiDecoder.addDecoder(mp2Decoder, "audio/mpeg; codecs=\"mpeg1-layer2\"");

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
  // blocking out.write() into I2S) runs on core 1 by default. Without
  // this, the video task could land on core 1 too and preempt loop() for
  // the length of a slow render call. Call before begin()/first write().
  videoSyncTask.setTaskParameters(4096, 2, 0);
  videoSyncTask.setQueueBytes(40 * 1024);
  videoSyncTask.setQueueUsePSRAM(true);

  mpgDemuxer.setOutputAudio(audioOut);
  mpgDemuxer.setOutputVideo(
      videoSyncTask);  // buffer + schedule via videoSyncTask
  if (!mpgDemuxer.begin()) {
    Serial.println("DemuxerMPG begin() failed");
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
