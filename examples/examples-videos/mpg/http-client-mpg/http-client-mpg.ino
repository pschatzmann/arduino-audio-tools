/**
 * @file http-client-mpg.ino
 * @brief Connects to an MPEG-1 Program Stream HTTP video feed, demuxes it
 * with DemuxerMPG, decodes the MPEG-1 (ISO/IEC 11172-2) video track with
 * MPGDecoder (TinyMPG, https://github.com/pschatzmann/TinyMPG - pure
 * software, works on any board) and displays the result live on a TFT
 * screen with TFT_eSPI.
 *
 * Pipeline: URLStream (HTTP, chunked) -> EncodedAudioOutput (Print bridge)
 * -> DemuxerMPG (demux)
 *      \-> MPGDecoder (MPEG-1 decode -> RGB565) -> OutputTFT_eSPI (draw)
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
 * - https://github.com/Bodmer/TFT_eSPI (configure your display's pins/driver
 *   in that library's User_Setup.h - not done in this sketch)
 * - https://github.com/pschatzmann/TinyMPG
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputTFT_eSPI.h"

// ---- WiFi ----
const char *ssid = "ssid";
const char *password = "password";
const char *video_url = "http://192.168.1.100/";


TFT_eSPI tft = TFT_eSPI();
MPGDecoder mpgDecoder;
OutputTFT_eSPI tftOutput(tft);
DemuxerMPG mpgDemuxer;
EncodedAudioOutput mpgInput(&mpgDemuxer);  // bridges raw HTTP bytes -> DemuxerMPG::write()
URLStream url(ssid, password);
StreamCopy copier(mpgInput, url);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

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
    // RGB565 frames to the TFT via OutputTFT_eSPI
    copier.copy();
  } else {
    Serial.println("Disconnected - reconnecting...");
    delay(2000);
    if (url.begin(video_url, "video/mpeg")) {
      copier.begin(mpgInput, url);
    }
  }
}
