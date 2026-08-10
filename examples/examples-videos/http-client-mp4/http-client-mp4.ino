/**
 * @file http-client-mp4.ino
 * @brief Connects to an HTTP MP4 stream (interleaved H.264 video + AAC
 * audio, "faststart" muxed so moov comes before mdat), demuxes it live with
 * DemuxerMP4, decodes the H.264 video track with H264Decoder (TinyH264,
 * https://github.com/pschatzmann/TinyH264 - pure software, works on any
 * board) and displays the result live on a TFT screen with TFT_eSPI. Also
 * plays the audio track through I2S.
 *
 * Pipeline: URLStream (HTTP) -> EncodedAudioOutput (Print bridge) ->
 * DemuxerMP4 (interleaved demux)
 *   -> EncodedAudioStream (AACDecoderHelix) -> I2SStream (audio)
 *   \-> H264Decoder (H.264 decode -> RGB565) -> OutputTFT (draw) (video)
 *
 * DemuxerMP4 only demuxes - it does not own or configure an audio decoder
 * itself; setOutputAudio() just points it at a plain Print, so any decoder
 * matching the track's codec can be wired in externally via an
 * EncodedAudioStream (as done here for AAC).
 *
 * DemuxerMP4 is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so unlike a typical "seek to the next chunk" MP4 reader
 * it works directly over a live HTTP download. It does require the file to
 * be faststart-muxed, e.g.:
 *   ffmpeg -i in.mp4 -c:v libx264 -c:a aac -movflags +faststart out.mp4
 *
 * On an ESP32-S3 board, swap H264Decoder for H264DecoderESP32S3
 * (AudioTools/Video/CodecH264ESP32S3.h) to use the hardware/esp_h264
 * backend (https://github.com/pschatzmann/ESP32S3-h264) instead - same
 * setOutput()/setVideoFormat() surface, no other change needed below.
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
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTFT.h"

// ---- WiFi ----
const char *ssid = "ssid";
const char *password = "password";
const char *video_url = "http://192.168.1.100/video.mp4";

TFT_eSPI tft = TFT_eSPI();
H264Decoder h264Decoder;
OutputTFT tftOutput(tft, h264Decoder);
I2SStream i2s;
AACDecoderHelix aacDecoder;
EncodedAudioStream audioOut(&i2s, &aacDecoder);  // decodes AAC -> I2S

// video-only-vs-audio-only decision is per track inside DemuxerMP4; both
// sinks are wired in and it dispatches each demuxed sample to the right one
DemuxerMP4 mp4Demuxer;
EncodedAudioOutput mp4Input(&mp4Demuxer);  // bridges raw HTTP bytes -> DemuxerMP4::write()

URLStream url(ssid, password);
StreamCopy copier(mp4Input, url);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);
  audioOut.begin();

  h264Decoder.setOutput(tftOutput);
  h264Decoder.setVideoFormat(VideoFormat::RGB565);  
  h264Decoder.begin();

  mp4Demuxer.setOutputAudio(audioOut);
  mp4Demuxer.setOutputVideo(h264Decoder);
  mp4Input.begin();

  if (!url.begin(video_url, "video/mp4")) {
    Serial.println("Connection to video server failed");
    while (true) delay(1000);
  }
}

void loop() {
  if (url) {
    // pumps bytes from the HTTP stream into DemuxerMP4, which routes
    // decoded AAC audio to I2S and demuxed H.264 frames to H264Decoder,
    // which in turn pushes decoded RGB565 frames to the TFT via OutputTFT
    copier.copy();
  } else {
    Serial.println("Disconnected - reconnecting...");
    delay(2000);
    if (url.begin(video_url, "video/mp4")) {
      copier.begin(mp4Input, url);
    }
  }
}
