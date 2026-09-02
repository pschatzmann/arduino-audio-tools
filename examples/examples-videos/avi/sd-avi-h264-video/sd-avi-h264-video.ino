/**
 * @file sd-avi-audio-video.ino
 * @brief Plays a local .avi file's audio + H.264 video tracks from the
 * microSD card of the Hosyond 2.8" ESP32-S3 Display (see that board's
 * audio-out/lcd-test/player-sdmmc examples): DemuxerAVI demuxes, H264Decoder
 * (TinyH264) decodes video to the ILI9341 panel, mp3Decoder/aacDecoder/
 * wavDecoder auto-select and decode the audio (PCM/AAC/MP3 all valid in
 * AVI) to the ES8311/FM8002E speaker path via AudioBoardStream.
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
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
// #include "AudioTools/Video/CodecH264ESP32S3.h"
#include <SD_MMC.h>

#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "AudioTools/Video/VideoPlayer.h"
#include "TinyGPU/Boards.h"

// ---- File on the SD card to play ----
const char* file_path = "/Videos/output176x144.avi";

DemuxerAVI aviDemuxer;
LCDBoardESP32S3_2_8Display board;
OutputTinyGPU tftOutput(board);
AudioBoardStream out(audio_driver::ESP32S3HosyondDisplay);
VideoPlayer player(aviDemuxer, tftOutput, out);

H264Decoder h264Decoder;
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
  // On: upscale the decoded 176x144 frame to fill the 320x240 panel -
  // costs more render time (3x the pixels vs. native size: 76800 vs
  // 25344) than leaving this off. See PacedVideoOutput's avgFrameMs()/
  // setScaleSingleBuffer() if render time needs to come back down.
  tftOutput.setScaleToFit(true);

  player.addVideoDecoder(h264Decoder);
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
  // 40KB: this video's frames can individually run into several KB (a
  // low-fps, bitrate-preserving transcode packs more data into each
  // remaining frame) - increase further if drops/resyncs are still too
  // frequent.
  player.setQueueBytes(40 * 1024);
  // Lowered from the 1.0 default: I-frame decode cost on this video is
  // high enough that the per-GOP backlog compounds over time. Can't
  // reduce I-frame decode cost itself (I-frames are never dropped this
  // way), but sheds P-frames sooner in each cycle to keep the queue's
  // average fill lower.
  player.setCatchUpThresholdFrames(0.5f);
  // Raised from the default 3: this video's I:P ratio and I-frame decode
  // cost meant the queue could fill with 3 I-frames before the next
  // P-frame ever rendered - 4 gives more room to absorb I-frame spikes
  // without dropping P-frames unnecessarily.
  player.setMaxQueuedIFrames(4);

  if (!player.begin(file)) {
    Serial.println("VideoPlayer begin() failed");
    stop();
  }
}

void logInfo() {
  player.logTo(Serial);
  // Splits avgFrameMs() (the whole p_target->write()+flush() call, i.e.
  // decode + I420->RGB565 convert + panel SPI write combined) into its
  // decode share (h264Decoder.totalDecodeMs(), CAVLC decode + picture
  // reconstruction only) vs everything after it - tells us which half of
  // the render budget is actually worth optimizing next.
  uint32_t renderedFrames = player.videoSyncTask().frameCountI() +
                             player.videoSyncTask().frameCountP();
  float avgDecodeMs = renderedFrames > 0
                          ? (float)h264Decoder.totalDecodeMs() / renderedFrames
                          : 0.0f;
  Serial.print("avg decode ms: ");
  Serial.print(avgDecodeMs);
  Serial.print(" / avg convert+SPI ms: ");
  Serial.println(player.videoSyncTask().avgFrameMs() - avgDecodeMs);
#ifdef ESP32
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

  if (player.copy() == 0) {
    Serial.println("Done");
    file.close();
    stop();
  }
}
