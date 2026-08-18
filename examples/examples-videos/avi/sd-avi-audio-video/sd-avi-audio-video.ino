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
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include <SD_MMC.h>
#include <Adafruit_NeoPixel.h>

// ---- Status LED (WS2812, IO42) - one color per setup stage; whichever
// color is still showing is where setup() stopped or hung ----
constexpr int kPinLed = 42;
Adafruit_NeoPixel statusLed(1, kPinLed, NEO_GRB + NEO_KHZ800);

enum class Stage { AudioCodec, SdOpen, Display, AudioStream, VideoDecoder, Demuxer, Running, Done };

void setStage(Stage stage, const char *label) {
  Serial.printf("stage: %s\n", label);
  uint8_t r = 0, g = 0, b = 0;
  switch (stage) {
    case Stage::AudioCodec:   r = 255; g = 140; b = 0;   break;  // orange
    case Stage::SdOpen:       r = 0;   g = 255; b = 255; break;  // cyan
    case Stage::Display:      r = 255; g = 255; b = 0;   break;  // yellow
    case Stage::AudioStream:  r = 255; g = 0;   b = 255; break;  // magenta
    case Stage::VideoDecoder: r = 140; g = 0;   b = 255; break;  // purple
    case Stage::Demuxer:      r = 0;   g = 0;   b = 255; break;  // blue
    case Stage::Running:      r = 0;   g = 255; b = 0;   break;  // green
    case Stage::Done:         r = 255; g = 255; b = 255; break;  // white
  }
  statusLed.setPixelColor(0, statusLed.Color(r, g, b));
  statusLed.show();
}

// ---- File on the SD card to play ----
const char *file_path = "/Videos/output176x144.avi";

// ---- Display SPI pins (Hosyond 2.8" ESP32-S3 Display - see that board's
// lcd-test example) ----
constexpr int8_t kPinMosi = 11;
constexpr int8_t kPinMiso = 13;
constexpr int8_t kPinSclk = 12;
constexpr int8_t kPinCs = 10;
constexpr int8_t kPinDc = 46;
constexpr int8_t kPinRst = -1;  // shared with the board's EN/reset line
constexpr int8_t kPinBacklight = 45;

// panel resolution - used by OutputTinyGPU's begin()/clearScreen() sizing;
// the actual per-frame size still comes from H264Decoder via
// setVideoInfoSource()
const uint16_t video_width = 240;
const uint16_t video_height = 320;

ILI9341Driver<RGB565> tftDriver(SPI, kPinCs, kPinDc, kPinRst);
H264Decoder h264Decoder;
OutputTinyGPU tftOutput(tftDriver, video_width, video_height, kPinBacklight);
AudioBoardStream out(ESP32S3HosyondDisplay);
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes PCM/AAC/MP3 -> out

File file;
DemuxerAVI aviDemuxer;
CodecCopy copier(aviDemuxer, file);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);

  statusLed.begin();
  statusLed.setBrightness(64);

  setStage(Stage::AudioCodec, "audio codec (out.begin)");
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sdmmc_active = true;  // board's begin() calls SD_MMC.setPins()+begin()
  if (!out.begin(cfg)) {
    Serial.println("AudioBoardStream begin() failed");
    stop();
  }
  out.setVolume(0.5f);

  setStage(Stage::SdOpen, "SD file open");
  file = SD_MMC.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    stop();
  }

  setStage(Stage::Display, "display (tftOutput.begin)");
  SPI.begin(kPinSclk, kPinMiso, kPinMosi, kPinCs);
  tftDriver.setInvertColor(true);
  if (!tftOutput.begin()) {
    Serial.println("OutputTinyGPU begin() failed");
    stop();
  }

  setStage(Stage::AudioStream, "audio stream (audioOut.begin)");
  multiDecoder.setMimeSource(aviDemuxer);
  if (!audioOut.begin()) {
    Serial.println("EncodedAudioStream begin() failed");
    stop();
  }

  setStage(Stage::VideoDecoder, "video decoder (h264Decoder.begin)");
  h264Decoder.setOutput(tftOutput);
  tftOutput.setVideoInfoSource(h264Decoder);
  tftOutput.setVideoFormat(VideoFormat::RGB565);

  if (!h264Decoder.begin()) {
    Serial.println("H264Decoder begin() failed");
    stop();
  }

  setStage(Stage::Demuxer, "AVI demuxer (aviDemuxer.begin)");
  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setOutputVideo(h264Decoder);
  if (!aviDemuxer.begin()) {
    Serial.println("DemuxerAVI begin() failed");
    stop();
  }

  setStage(Stage::Running, "playing");
}

void loop() {
  if (file && copier.copy()) {
    // continue
  } else {
    setStage(Stage::Done, "done");
    Serial.println("Done");
    file.close();
    stop();
  }
}
