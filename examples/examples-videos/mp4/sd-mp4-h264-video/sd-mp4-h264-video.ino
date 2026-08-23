/**
 * @file sd-mp4-audio-video.ino
 * @brief Plays both the audio (AAC) and video (H.264) tracks of a local
 * .mp4 file on an SD card: demuxes it live with DemuxerMP4, decodes the
 * video track with H264Decoder (TinyH264,
 * https://github.com/pschatzmann/TinyH264 - pure software, works on any
 * board) and displays the result live on a TinyGPU-driven TFT, while
 * playing the audio track through I2S. Same "faststart" MP4 (moov before
 * mdat) DemuxerMP4 requires everywhere else, e.g.:
 *   ffmpeg -i in.mp4 -c:v libx264 -c:a aac -movflags +faststart out.mp4
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerMP4
 * (demux)
 *   -> EncodedAudioStream (AACDecoderHelix) -> I2SStream (audio)
 *   \-> H264Decoder (H.264 decode -> RGB565) -> OutputTinyGPU (draw) (video)
 *
 * DemuxerMP4 is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with StreamCopy (the same
 * way http-client-mp4.ino feeds it from a live HTTP download) works fine.
 * See sd-mp4-audio.ino for an audio-only version of this same file, and
 * http-client-mp4.ino for the network (HTTP) equivalent of this one.
 *
 * On an ESP32-S3 board, swap H264Decoder for H264DecoderESP32S3
 * (AudioTools/Video/CodecH264ESP32S3.h) to use the hardware/esp_h264
 * backend (https://github.com/pschatzmann/ESP32S3-h264) instead - same
 * setOutput()/setVideoFormat() surface, no other change needed below.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/pschatzmann/TinyGPU (SPI/display pins below match its
 *   bouncing-ball example - adjust for your own wiring)
 * - https://github.com/pschatzmann/TinyH264
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/video.mp4";

// ---- SPI / display pins (adjust for your wiring) ----
constexpr int8_t kPinMosi = 13;
constexpr int8_t kPinMiso = 12;
constexpr int8_t kPinSclk = 14;
constexpr int8_t kPinCs = 15;
constexpr int8_t kPinDc = 2;
constexpr int8_t kPinRst = -1;
constexpr int8_t kPinBacklight = 27;

// display resolution - used by OutputTinyGPU's begin()/clearScreen()
// sizing; the actual per-frame size still comes from H264Decoder via
// setVideoInfoSource()

ILI9341Driver<RGB565> tftDriver(SPI, kPinCs, kPinDc, kPinRst);
H264Decoder h264Decoder;
OutputTinyGPU tftOutput(tftDriver, kPinBacklight);
I2SStream i2s;
AACDecoderHelix aacDecoder;
EncodedAudioStream audioOut(&i2s, &aacDecoder);  // decodes AAC -> I2S

DemuxerMP4 mp4Demuxer;
EncodedAudioOutput mp4Input(&mp4Demuxer);  // bridges raw file bytes -> DemuxerMP4::write()

File file;
StreamCopy copier(mp4Input, file);

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
  audioOut.begin();

  h264Decoder.setOutput(tftOutput);
  h264Decoder.setVideoFormat(VideoFormat::RGB565);  // matches pushImage()'s expected format
  tftOutput.setVideoInfoSource(h264Decoder);
  h264Decoder.begin();

  mp4Demuxer.setOutputAudio(audioOut);
  mp4Demuxer.setOutputVideo(h264Decoder);
  mp4Input.begin();
}

void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
