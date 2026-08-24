/**
 * @file sd-avi-audio-video.ino
 * @brief Plays a local .avi file's audio + H.264 video tracks from the
 * microSD card of the Hosyond 2.8" ESP32-S3 Display (see that board's
 * audio-out/lcd-test/player-sdmmc examples): DemuxerAVI demuxes, H264Decoder
 * (TinyH264) decodes video to the ILI9341 panel, DecoderHelix auto-detects
 * and decodes the audio (PCM/AAC/MP3 all valid in AVI) to the ES8311/
 * FM8002E speaker path via AudioBoardStream.
 *
 * Audio/video sync: DemuxerAVI dispatches audio/video as fast as bytes can
 * be parsed - all real pacing happens in videoSyncTask (PacedVideoOutput,
 * see Video/PacedVideoOutput.h), which sits between the demuxer and
 * h264Decoder. It buffers each decoded frame and renders it (H.264
 * decode + panel refresh) from its own background task, timed against
 * audioClock - an AudioTimeSourceStream inserted between multiDecoder and
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
 *   \-> PacedVideoOutput (buffer + schedule) -> H264Decoder ->
 *   OutputTinyGPU (video, own background task)
 *
 * Notes:
 * - Video track must be H264/h264/X264/x264/avc1/AVC1 Annex-B - see
 *   sd-avi-video.ino for building a compatible file with ffmpeg.
 * - The board's WS2812 RGB LED (IO42, see led-test) is set to a distinct
 *   color right before each setup stage starts, so if a stage hangs (never
 *   returns from begin()) rather than cleanly failing, the color still on
 *   the LED tells you which one - no serial capture needed.
 *   I transcoded the video with ffmpeg -i Casablanca.1942.720.x264.YIFY.mkv -vf
 * "scale=176:144,fps=7" -pix_fmt yuv420p -c:v libx264 -profile:v baseline
 * -level 3.0 -g 15 -crf 28 output176x144-v3.avi
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/TinyGPU
 * - https://github.com/pschatzmann/TinyH264
 * - https://github.com/adafruit/Adafruit_NeoPixel
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
// #include "AudioTools/Video/CodecH264ESP32S3.h"
#include <SD_MMC.h>

#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "TinyGPU/Boards.h"

// ---- File on the SD card to play ----
const char* file_path = "/Videos/output176x144.avi";

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
  // On: upscale the decoded 176x144 frame to fill the 320x240 panel -
  // costs more render time (3x the pixels vs. native size: 76800 vs
  // 25344) than leaving this off. See PacedVideoOutput's avgFrameMs()/
  // setScaleSingleBuffer() if render time needs to come back down.
  tftOutput.setScaleToFit(true);

  multiDecoder.setMimeSource(aviDemuxer);
  if (!audioOut.begin()) {
    Serial.println("EncodedAudioStream begin() failed");
    stop();
  }

  h264Decoder.setOutput(tftOutput);
  tftOutput.setVideoInfoSource(h264Decoder);

  if (!h264Decoder.begin()) {
    Serial.println("H264Decoder begin() failed");
    stop();
  }

  videoSyncTask.setAudioClock(audioClock);
  // Pin the render task to core 0 - loop() (SD reads + demuxing + the
  // blocking out.write() into I2S) runs on core 1 by default. Without
  // this, the video task could land on core 1 too and preempt loop() for
  // the length of a slow render call. Call before begin()/first write().
  videoSyncTask.setTaskParameters(4096, 2, 0);
  // call before begin()/first write(). 256KB: this video's frames can
  // individually run into several KB (a low-fps, bitrate-preserving
  // transcode packs more data into each remaining frame), so a queue
  // sized for the earlier, smaller-frame video left room for only 3-4
  // frames total - increase further if drops/resyncs are still too
  // frequent.
  videoSyncTask.setQueueBytes(40 * 1024);
  // Lowered from the 1.0 default: I-frame decode cost on this video is
  // high enough that the per-GOP backlog compounds over time. Can't
  // reduce I-frame decode cost itself (I-frames are never dropped this
  // way), but sheds P-frames sooner in each cycle to keep the queue's
  // average fill lower.
  videoSyncTask.setCatchUpThresholdFrames(0.5f);

  // Raised from the default 3: this video's I:P ratio and I-frame decode
  // cost meant the queue could fill with 3 I-frames before the next
  // P-frame ever rendered - 4 gives more room to absorb I-frame spikes
  // without dropping P-frames unnecessarily.
  videoSyncTask.setMaxQueuedIFrames(4);

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
  Serial.print("avg render ms - I: ");
  Serial.print(videoSyncTask.avgIFrameMs());
  Serial.print(" / P: ");
  Serial.print(videoSyncTask.avgPFrameMs());
  Serial.print(" / overall: ");
  Serial.println(videoSyncTask.avgFrameMs());
  // Splits avgFrameMs() (the whole p_target->write()+flush() call, i.e.
  // decode + I420->RGB565 convert + panel SPI write combined) into its
  // decode share (h264Decoder.totalDecodeMs(), CAVLC decode + picture
  // reconstruction only) vs everything after it - tells us which half of
  // the render budget is actually worth optimizing next.
  uint32_t renderedFrames =
      videoSyncTask.frameCountI() + videoSyncTask.frameCountP();
  float avgDecodeMs = renderedFrames > 0
                          ? (float)h264Decoder.totalDecodeMs() / renderedFrames
                          : 0.0f;
  Serial.print("avg decode ms: ");
  Serial.print(avgDecodeMs);
  Serial.print(" / avg convert+SPI ms: ");
  Serial.println(videoSyncTask.avgFrameMs() - avgDecodeMs);
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
