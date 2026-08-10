/**
 * @file http-client-avi-tft.ino
 * @brief Companion client for http-server-avi.ino: connects to the MJPEG-in-AVI
 * HTTP stream published by that sketch, demuxes it with DemuxerAVI, and
 * displays the decoded JPEG frames live on a TFT screen with JPEGOutputTFT. Also
 * plays the audio track through I2S, if the stream has one - http-server-avi.ino
 * currently only sends video, but DemuxerAVI's audio format is only known
 * once the 'strf' header is parsed, so this client is written to handle
 * whichever of PCM/WAV, AAC or MP3 shows up.
 *
 * Pipeline: URLStream (HTTP, chunked) -> EncodedAudioOutput (Print bridge)
 * -> DemuxerAVI (demux)
 *      -> EncodedAudioStream (DecoderHelix: WAV/AAC/MP3) -> I2SStream (audio)
 *      \-> JPEGOutputTFT (JPEG decode + draw) (video)
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/Bodmer/TFT_eSPI (configure your display's pins/driver
 *   in that library's User_Setup.h - not done in this sketch)
 * - https://github.com/Bodmer/JPEGDecoder
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/Video/JPEGOutputTFT.h"

// ---- WiFi ----
const char *ssid = "ssid";
const char *password = "password";
const char *video_url = "http://192.168.1.100/";

TFT_eSPI tft = TFT_eSPI();
JPEGOutputTFT jpegOutput(tft);
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
  aviDecoder.setOutputAudio(audioOut);
  aviDecoder.setOutputVideo(jpegOutput);
  aviInput.begin();

  if (!url.begin(video_url, "video/avi")) {
    Serial.println("Connection to video server failed");
    while (true) delay(1000);
  }

}

void loop() {
  if (url) {
    // pumps bytes from the HTTP stream into DemuxerAVI, which routes
    // decoded audio to I2S and demuxed JPEG frames to JPEGOutputTFT for display
    copier.copy();
  } else {
    Serial.println("Disconnected - reconnecting...");
    delay(2000);
    if (url.begin(video_url, "video/avi")) {
      copier.begin(aviInput, url);
    }
  }
}
