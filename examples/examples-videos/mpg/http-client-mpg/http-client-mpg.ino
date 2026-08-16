/**
 * @file http-client-mpg.ino
 * @brief Connects to an MPEG-1 Program Stream HTTP video feed, demuxes it
 * with DemuxerMPG, decodes the MPEG-1 (ISO/IEC 11172-2) video track with
 * MPGDecoder (TinyMPG, https://github.com/pschatzmann/TinyMPG - pure
 * software, works on any board) and displays the result live on a
 * TinyGPU-driven TFT.
 *
 * Pipeline: URLStream (HTTP, chunked) -> EncodedAudioOutput (Print bridge)
 * -> DemuxerMPG (demux)
 *      \-> MPGDecoder (MPEG-1 decode -> RGB565) -> OutputTinyGPU (draw)
 *
 * Companion server: http-server-mpg.ino (this file's counterpart, publishes
 * exactly this stream from an ESP32-S3 + camera). See that file's own
 * comment for why MuxerMPG/DemuxerMPG need no external wrapper format -
 * ISO/IEC 11172-1 framing *is* the container, unlike the AVI/MP4 pairs
 * elsewhere in examples-videos.
 *
 * This example is video-only, matching its AVI+H.264 counterpart
 * (http-client-avi-h264.ino). DemuxerMPG also demuxes an MPEG-1 audio track
 * if the source has one - see ContainerMPG.h's own file comment for the
 * setOutputAudio()/MP3DecoderHelix wiring http-server-mpg.ino doesn't use.
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
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputTinyGPU.h"

// ---- WiFi ----
const char *ssid = "ssid";
const char *password = "password";
const char *video_url = "http://192.168.1.100/";

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
const uint16_t video_width = 320;
const uint16_t video_height = 240;

ILI9341Driver<RGB565> tftDriver(SPI, kPinCs, kPinDc, kPinRst);
MPGDecoder mpgDecoder;
OutputTinyGPU tftOutput(tftDriver, video_width, video_height, kPinBacklight);
DemuxerMPG mpgDemuxer;
EncodedAudioOutput mpgInput(&mpgDemuxer);  // bridges raw HTTP bytes -> DemuxerMPG::write()
URLStream url(ssid, password);
StreamCopy copier(mpgInput, url);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  SPI.begin(kPinSclk, kPinMiso, kPinMosi, kPinCs);
  tftOutput.begin();

  mpgDecoder.setOutput(tftOutput);  // RGB565 (the default) matches
                                     // pushImage()'s expected format
  tftOutput.setVideoInfoSource(mpgDecoder);
  mpgDecoder.begin();

  mpgDemuxer.setOutputVideo(mpgDecoder);
  mpgInput.begin();

  if (!url.begin(video_url, "video/mpeg")) {
    Serial.println("Connection to video server failed");
    while (true) delay(1000);
  }

  copier.begin(mpgInput, url);
}

void loop() {
  if (url) {
    // pumps bytes from the HTTP stream into DemuxerMPG, which routes
    // demuxed MPEG-1 pictures to MPGDecoder, which in turn pushes decoded
    // RGB565 frames to the TFT via OutputTinyGPU
    copier.copy();
  } else {
    Serial.println("Disconnected - reconnecting...");
    delay(2000);
    if (url.begin(video_url, "video/mpeg")) {
      copier.begin(mpgInput, url);
    }
  }
}
