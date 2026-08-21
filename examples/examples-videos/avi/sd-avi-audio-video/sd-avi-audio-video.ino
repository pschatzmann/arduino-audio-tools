/**
 * @file sd-avi-audio-video.ino
 * @brief Plays a local .avi file's audio + H.264 video tracks from the
 * microSD card of the Hosyond 2.8" ESP32-S3 Display (see that board's
 * audio-out/lcd-test/player-sdmmc examples): DemuxerAVI demuxes, H264Decoder
 * (TinyH264) decodes video to the ILI9341 panel, DecoderHelix auto-detects
 * and decodes the audio (PCM/AAC/MP3 all valid in AVI) to the ES8311/
 * FM8002E speaker path via AudioBoardStream.
 *
 * Pipeline: File (SD_MMC) -> CodecCopy -> DemuxerAVI (demux)
 *   -> EncodedAudioStream (DecoderHelix) -> AudioBoardStream (audio)
 *   \-> H264Decoder -> OutputTinyGPU (video)
 *
 * Notes:
 * - CodecCopy (not StreamCopy) retries on a partial write() accept, so a
 *   full DemuxerAVI parse buffer never desyncs the chunk stream.
 * - SD_MMC (4-bit SDIO, not SPI) is this board's actual card slot, brought
 *   up via cfg.sdmmc_active - see the board's player-sdmmc example.
 * - tftDriver.setInvertColor(true) is required on this panel for correct
 *   colors - see the board's lcd-test example.
 * - Video track must be H264/h264/X264/x264/avc1/AVC1 Annex-B - see
 *   sd-avi-video.ino for building a compatible file with ffmpeg.
 * - The board's WS2812 RGB LED (IO42, see led-test) is set to a distinct
 *   color right before each setup stage starts, so if a stage hangs (never
 *   returns from begin()) rather than cleanly failing, the color still on
 *   the LED tells you which one - no serial capture needed.
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
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
//#include "AudioTools/Video/CodecH264ESP32S3.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "TinyGPU/LCDBoardsESP32.h"
#include <SD_MMC.h>


// ---- File on the SD card to play ----
const char *file_path = "/Videos/output176x144.avi";

H264Decoder h264Decoder;
VideoFrameMeter meter(h264Decoder);
LCDBoardESP32S3_2_8Display board;
OutputTinyGPU tftOutput(board);
AudioBoardStream out(ESP32S3HosyondDisplay);
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes PCM/AAC/MP3 -> out
File file;
DemuxerAVI aviDemuxer;
CodecCopy copier(aviDemuxer, file);
VideoAudioSync videoSync;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sdmmc_active = true;  // board's begin() calls SD_MMC.setPins()+begin()
  cfg.buffer_size = 1024;
  cfg.buffer_count = 64;
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
  tftOutput.setScaleToFit(true);  // stretch the decoded 176x144 frame to fill the 320x240 landscape panel

  multiDecoder.setMimeSource(aviDemuxer);
  if (!audioOut.begin()) {
    Serial.println("EncodedAudioStream begin() failed");
    stop();
  }

  h264Decoder.setOutput(tftOutput);
  tftOutput.setVideoInfoSource(h264Decoder);
  //  h264Decoder.setInputBufferSize(64 * 1024);   // generous for QCIF frames
  //  h264Decoder.setVideoFormat(VideoFormat::I420);
  //  h264Decoder.setOutputBufferSize(video_width * video_height * 3 / 2);

  if (!h264Decoder.begin()) {
    Serial.println("H264Decoder begin() failed");
    stop();
  }

  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setVideoAudioSync(&videoSync);
  aviDemuxer.setOutputVideo(meter);
  if (!aviDemuxer.begin()) {
    Serial.println("DemuxerAVI begin() failed");
    stop();
  }
}

void logInfo() {
  Serial.print("avg fps: ");
  Serial.print(meter.fpsAvg());
  Serial.print(" / max fps: ");
  Serial.println(meter.fpsMax());
#ifdef ESP32
  // Internal heap
  size_t heapFree = ESP.getFreeHeap();
  size_t heapTotal = ESP.getHeapSize();
  size_t heapUsed = heapTotal - heapFree;

  // PSRAM
  size_t psramFree = ESP.getFreePsram();
  size_t psramTotal = ESP.getPsramSize();
  size_t psramUsed = psramTotal - psramFree;

  Serial.printf(
    "Heap:  total=%u, used=%u, free=%u bytes\n",
    heapTotal, heapUsed, heapFree);

  Serial.printf(
    "PSRAM: total=%u, used=%u, free=%u bytes\n",
    psramTotal, psramUsed, psramFree);

  // Largest currently allocatable blocks
  Serial.printf(
    "Largest heap block:  %u bytes\n",
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

  Serial.printf(
    "Largest PSRAM block: %u bytes\n",
    heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
#endif
}

void loop() {
  static uint32_t diagLast = 0;
  if (millis() - diagLast > 1000) {
    logInfo();
    diagLast = millis();
  }
  if (file) {
    if (copier.copy() == 0) {
      Serial.println("Done");
      file.close();
      stop();
    }
  }
}
