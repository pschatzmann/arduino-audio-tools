/**
 * @file http-client-avi-h264.ino
 * @brief Connects to an H.264-in-AVI HTTP stream, demuxes it with
 * DemuxerAVI, decodes the H.264 video track with H264Decoder (TinyH264,
 * https://github.com/pschatzmann/TinyH264 - pure software, works on any
 * board) and displays the result live on a TFT screen with TFT_eSPI. Also
 * plays the audio track through I2S, if the stream has one.
 *
 * Pipeline: URLStream (HTTP, chunked) -> EncodedAudioOutput (Print bridge)
 * -> DemuxerAVI (demux)
 *      -> EncodedAudioStream (DecoderHelix: WAV/AAC/MP3) -> I2SStream (audio)
 *      \-> H264Decoder (H.264 decode -> RGB565) -> OutputTFT (draw) (video)
 *
 * On an ESP32-S3 board, swap H264Decoder for H264DecoderESP32S3
 * (AudioTools/Video/CodecH264ESP32S3.h) to use the hardware/esp_h264
 * backend (https://github.com/pschatzmann/ESP32S3-h264) instead - same
 * setOutput()/setPixelFormat() surface, no other change needed below.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/Bodmer/TFT_eSPI (configure your display's pins/driver
 *   in that library's User_Setup.h - not done in this sketch)
 * - https://github.com/pschatzmann/TinyH264
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTFT.h"

// ---- WiFi ----
const char *ssid = "ssid";
const char *password = "password";
const char *video_url = "http://192.168.1.100/";


TFT_eSPI tft = TFT_eSPI();
H264Decoder h264Decoder;
OutputTFT tftOutput(tft, h264Decoder);
I2SStream i2s;
DecoderHelix multiDecoder;  
EncodedAudioStream audioOut(&i2s, &multiDecoder);  // decodes PCM/AAC/MP3 -> I2S
DemuxerAVI aviDecoder;
EncodedAudioOutput aviInput(&aviDecoder);  // bridges raw bytes -> DemuxerAVI::write()
URLStream url(ssid, password);
StreamCopy copier(aviInput, url);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);
  audioOut.begin();
  multiDecoder.begin();

  h264Decoder.setOutput(tftOutput);  // RGB565 (the default) matches
                                      // pushImage()'s expected format
  h264Decoder.begin();

  aviDecoder.setOutputAudio(audioOut);
  aviDecoder.setOutputVideo(h264Decoder);
  aviInput.begin();

  if (!url.begin(video_url, "video/avi")) {
    Serial.println("Connection to video server failed");
    while (true) delay(1000);
  }

  copier.begin(aviInput, url);
}

void loop() {
  if (url) {
    // pumps bytes from the HTTP stream into DemuxerAVI, which routes
    // decoded audio to I2S and demuxed H.264 frames to H264Decoder, which
    // in turn pushes decoded RGB565 frames to the TFT via OutputTFT
    copier.copy();
  } else {
    Serial.println("Disconnected - reconnecting...");
    delay(2000);
    if (url.begin(video_url, "video/avi")) {
      copier.begin(aviInput, url);
    }
  }
}
