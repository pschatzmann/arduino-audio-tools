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
// Experiment: H264DecoderESP32S3 wraps the ESP32S3-h264 library (a trimmed
// Arduino port of Espressif's esp_h264 component, linking a precompiled
// OpenH264-based static lib) instead of the portable, pure-C++ TinyH264
// H264Decoder (CodecH264.h) - testing whether it explains the ~120-135ms/
// frame pure-decode cost measured with TinyH264 vs a reported 62.3fps
// decode-only benchmark on similar hardware.
#include "AudioTools/Video/CodecH264ESP32S3.h"
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
// landscape (320x240, matching the rotation passed to tftDriver below) so
// the video fills the panel's long axis instead of playing in a small
// portrait-shaped area. The actual per-frame source size still comes from
// H264Decoder via setVideoInfoSource(); OutputTinyGPU::setScaleToFit(true)
// (see setup()) stretches each decoded frame from that native size up to
// this one.
const uint16_t video_width = 320;
const uint16_t video_height = 240;

// ILI9341's datasheet max SPI write clock is 10MHz officially, but these
// clone panels are commonly run well beyond spec - 40MHz worked reliably;
// 80MHz (the ESP32-S3's SPI peripheral max, half of its 160MHz base clock)
// roughly halves the per-frame SPI transfer time again. Drop back to
// 40000000 if this shows tearing/glitches on your specific panel.
ILI9341Driver<RGB565> tftDriver(SPI, kPinCs, kPinDc, kPinRst,
                                ILI9341Driver<RGB565>::Rotation::kLandscape,
                                80000000);
H264DecoderESP32S3<> h264Decoder;
OutputTinyGPU tftOutput(tftDriver, video_width, video_height, kPinBacklight);
AudioBoardStream out(ESP32S3HosyondDisplay);
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes PCM/AAC/MP3 -> out

File file;
DemuxerAVI aviDemuxer;
CodecCopy copier(aviDemuxer, file);
// Owned here (rather than left as aviDemuxer's internal default) so the
// stats it accumulates - totalFrames()/totalDecodeWriteMs(), see loop() -
// are reachable from the sketch; wired in via setVideoAudioSync() below.
VideoAudioClockSync videoSync;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);

  statusLed.begin();
  statusLed.setBrightness(64);

  Serial.printf("DIAG: PSRAM size=%u free=%u\n", (unsigned)ESP.getPsramSize(),
                (unsigned)ESP.getFreePsram());

  setStage(Stage::AudioCodec, "audio codec (out.begin)");
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sdmmc_active = true;  // board's begin() calls SD_MMC.setPins()+begin()
  // Default I2S DMA buffer (512 * 6 = 3072 bytes => ~16ms @ 48kHz stereo)
  // is far shorter than a single video chunk's blocking decode time
  // (~100ms on this MCU) - confirmed on hardware that a bigger buffer only
  // delays the first underrun, it doesn't eliminate the breakup: decode is
  // ~2-3x too slow for real-time on average, so any fixed buffer
  // eventually drains regardless of size. Kept enlarged (1024 * 64 =
  // 65536 bytes => ~340ms) since it's still strictly better than the
  // default, but the real fix is reducing decode cost (see H264Decoder).
  cfg.buffer_size = 1024;
  cfg.buffer_count = 64;
  if (!out.begin(cfg)) {
    Serial.println("AudioBoardStream begin() failed");
    stop();
  }
  out.setVolume(0.4f);

  setStage(Stage::SdOpen, "SD file open");
  file = SD_MMC.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    stop();
  }
  Serial.printf("opened %s: %u bytes\n", file_path, (unsigned)file.size());

  setStage(Stage::Display, "display (tftOutput.begin)");
  SPI.begin(kPinSclk, kPinMiso, kPinMosi, kPinCs);
  if (!tftOutput.begin()) {
    Serial.println("OutputTinyGPU begin() failed");
    stop();
  }
  // Must come AFTER tftOutput.begin(): it calls tftDriver.begin(), which
  // does an ILI9341 SWRESET that resets every register - including
  // inversion - back to power-on defaults, wiping out any setInvertColor()
  // call made before it.
  tftDriver.setInvertColor(true);
  tftOutput.setScaleToFit(true);  // stretch the decoded 176x144 frame to fill the 320x240 landscape panel

  setStage(Stage::AudioStream, "audio stream (audioOut.begin)");
  multiDecoder.setMimeSource(aviDemuxer);
  if (!audioOut.begin()) {
    Serial.println("EncodedAudioStream begin() failed");
    stop();
  }

  setStage(Stage::VideoDecoder, "video decoder (h264Decoder.begin)");
  h264Decoder.setOutput(tftOutput);
  tftOutput.setVideoInfoSource(h264Decoder);
  h264Decoder.setInputBufferSize(64 * 1024);   // generous for QCIF frames
  // This library's software (non-hardware) decode path only supports I420
  // output - ESP_H264_RAW_FMT_RGB565_LE (the wrapper's default) fails
  // begin() with "Un-supported h264 picture type parameter". OutputTinyGPU
  // converts I420->RGB565 itself (see its writeI420()), so this is just
  // format selection, not a display limitation.
  h264Decoder.setVideoFormat(VideoFormat::I420);
  h264Decoder.setOutputBufferSize(video_width * video_height * 3 / 2);

  if (!h264Decoder.begin()) {
    Serial.println("H264Decoder begin() failed");
    stop();
  }

  setStage(Stage::Demuxer, "AVI demuxer (aviDemuxer.begin)");
  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setVideoAudioSync(&videoSync);
  aviDemuxer.setOutputVideo(h264Decoder);
  if (!aviDemuxer.begin()) {
    Serial.println("DemuxerAVI begin() failed");
    stop();
  }

  videoSync.setMaxConsecutiveRenderSkips(1);

  setStage(Stage::Running, "playing");
}

void loop() {
  static uint32_t diagLast = 0;
  if (millis() - diagLast > 1000) {
    uint32_t frames = videoSync.totalFrames();
    uint64_t decodeWriteMs = videoSync.totalDecodeWriteMs();
    Serial.printf(
        "DIAG: decodeErrors=%u decoded=%u | frames=%u framesPerSec=%.2f | "
        "avgDecodeWriteMs(incl.SD+demux)=%.1f\n",
        (unsigned)h264Decoder.decodeErrors(), (unsigned)h264Decoder.frameCount(),
        (unsigned)frames, millis() > 0 ? frames * 1000.0 / millis() : 0.0,
        frames > 0 ? (double)decodeWriteMs / frames : 0.0);
    diagLast = millis();
  }
  if (file) {
    if (copier.copy() == 0) {
      setStage(Stage::Done, "done");
      Serial.println("Done");
      file.close();
      stop();
    }
  }
}
