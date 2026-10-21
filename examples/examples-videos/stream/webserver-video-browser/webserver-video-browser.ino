/**
 * @file webserver-video-browser.ino
 * @brief Web server that streams live camera video to a web browser: open
 * http://<ip-address>/ in Chrome, Edge or Firefox. Browsers can't play an
 * MPEG transport stream natively, so the page served at / plays the stream
 * at /stream.ts with mpegts.js (https://github.com/xqq/mpegts.js), loaded
 * from a CDN: the browser needs internet access. VLC, ffplay or mpv can
 * open http://<ip-address>/stream.ts directly.
 *
 * Pipeline: TinyCamera (RGB565 frames) -> H264Encoder (TinyH264) ->
 * MuxerMTS -> HTTP clients, driven by VideoMuxer and served by TinyHttp.
 * Each frame is encoded once and sent to all connected viewers; a viewer
 * starts with the next keyframe, which is sent every second.
 *
 * Tested on a Waveshare ESP32-P4-WIFI6 with its OV5647 MIPI-CSI camera.
 * Build with
 * esp32:esp32:esp32p4:ChipVariant=postv3,PSRAM=enabled,PartitionScheme=huge_app
 * For other cameras replace setPinsWaveshareEsp32P4Wifi6() with the
 * TinyCameraPins.h preset (or pins) of your board.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/TinyCamera
 * - https://github.com/pschatzmann/TinyH264
 * - https://github.com/pschatzmann/TinyHttp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <WiFi.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMTS.h"
#include "AudioTools/Video/Camera/CameraFrameSourceTinyCamera.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/VideoMuxer.h"
#include "HttpExtensions.h"  // TinyHttp
#include "HttpServer.h"
#include "TinyCameraPins.h"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// Frame rate the camera is captured at: what the encoder can sustain
constexpr float kFps = 8;
const char *ssid = "ssid";
const char *password = "password";

// Player page: plays /stream.ts with mpegts.js, in low latency live mode
const char *kPage = R"(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Camera</title>
<script src="https://cdn.jsdelivr.net/npm/mpegts.js@1.7.3/dist/mpegts.js"></script>
</head><body style="background:#111;margin:0">
<video id="video" muted autoplay playsinline controls style="width:100%;max-height:100vh"></video>
<script>
if (mpegts.getFeatureList().mseLivePlayback) {
  const player = mpegts.createPlayer(
      {type: 'mpegts', isLive: true, url: location.origin + '/stream.ts'},
      {enableStashBuffer: false, liveBufferLatencyChasing: true});
  player.attachMediaElement(document.getElementById('video'));
  player.load();
  player.play();
} else {
  document.body.innerHTML = '<p style="color:white">This browser has no MSE live playback</p>';
}
</script></body></html>)";

WiFiServer wifi;
tinyhttp::HttpServer server(wifi);
CameraFrameSourceTinyCamera camera;
H264Encoder encoder;
MuxerMTS mts;
VideoMuxer video(mts);

// /stream.ts: everything written to it is sent to all connected viewers
tinyhttp::ExtensionStream viewers("/stream.ts", tinyhttp::T_GET, "video/mp2t");

void setup() {
  Serial0.begin(115200);
  AudioToolsLogger.begin(Serial0, AudioToolsLogLevel::Warning);

  // camera: 320x240 RGB565
  camera_config_t config = tiny_camera::TinyCamera::defaultConfig();
  tiny_camera::setPinsWaveshareEsp32P4Wifi6(config);
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  if (!camera.begin(config, kFps)) {
    Serial0.println("camera begin failed");
    stop();
  }

  // H.264 encoder: fixed quality, a keyframe every second
  VideoInfo info = camera.videoInfo();
  encoder.setSize(info.width, info.height);
  encoder.setVideoFormat(VideoFormat::RGB565);
  encoder.setQp(28);
  encoder.setKeyframeInterval(kFps);
  encoder.driver().setAllOptimizationsActive(true);

  // live stream: timestamps from the capture time, so the player's latency
  // stays constant even if fewer than kFps frames/s get through
  mts.setRealTime(true);
  mts.setOutput(viewers);
  video.setVideoSource(camera); 
  video.setVideoEncoder(encoder);
  video.begin();

  // web server: player page and stream
  server.on("/", tinyhttp::T_GET, "text/html", kPage);
  server.addExtension(viewers);
  server.setNoConnectDelay(0);  // loop() is paced by the video instead
  server.begin(80, ssid, password);  // connects to WiFi
  WiFi.setSleep(false);              // no power save: it throttles the stream

  Serial0.print("Open http://");
  Serial0.print(WiFi.localIP());
  Serial0.println("/ in a web browser");
}

void loop() {
  server.copy();  // accepts new requests
  if (viewers.isOpen()) {
    video.copy();  // captures, encodes and sends one frame to all viewers
  } else {
    delay(20);  // no viewers: nothing to encode
  }
}
