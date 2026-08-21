/**
 * @file sd-mpg-audio-video.ino
 * @brief Plays both the audio and video (MPEG-1) tracks of a local MPEG-1
 * Program Stream (.mpg) file on an SD card: demuxes it live with
 * DemuxerMPG, decodes the video track with MPGDecoder (TinyMPG,
 * https://github.com/pschatzmann/TinyMPG - pure software, works on any
 * board) and displays the result live on a TinyGPU-driven TFT, while
 * playing the audio track (a raw MPEG-1 Layer I/II/III elementary stream)
 * through I2S via MP3DecoderHelix - the same choice ContainerMPG.h's own
 * file comment uses.
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerMPG
 * (demux)
 *   -> EncodedAudioStream (MP3DecoderHelix) -> I2SStream (audio)
 *   \-> MPGDecoder (MPEG-1 decode -> RGB565) -> OutputTinyGPU (draw) (video)
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
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
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

// display resolution - used by OutputTinyGPU's begin()/clearScreen()
// sizing; the actual per-frame size still comes from MPGDecoder via
// setVideoInfoSource()

ILI9341Driver<RGB565> tftDriver(SPI, kPinCs, kPinDc, kPinRst);
MPGDecoder mpgDecoder;
OutputTinyGPU tftOutput(tftDriver, kPinBacklight);
I2SStream i2s;
MP3DecoderHelix mp3Decoder;
EncodedAudioStream audioOut(&i2s, &mp3Decoder);  // decodes MP3/MP2 -> I2S

DemuxerMPG mpgDemuxer;
EncodedAudioOutput mpgInput(&mpgDemuxer);  // bridges raw file bytes -> DemuxerMPG::write()

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
  audioOut.begin();

  mpgDecoder.setOutput(tftOutput);  // RGB565 (the default) matches
                                     // pushImage()'s expected format
  tftOutput.setVideoInfoSource(mpgDecoder);
  mpgDecoder.begin();

  mpgDemuxer.setOutputAudio(audioOut);
  mpgDemuxer.setOutputVideo(mpgDecoder);
  mpgInput.begin();
}

void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
