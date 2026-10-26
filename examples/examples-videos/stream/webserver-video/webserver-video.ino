/**
 * @file webserver-video.ino
 * @brief Web server that streams live camera video as H.264 in an MPEG
 * transport stream (video/mp2t): open http://<ip-address>/ in VLC, ffplay
 * or mpv. Each client gets its own stream, starting with a keyframe.
 *
 * Pipeline: TinyCamera (RGB565 frames) -> H264Encoder (TinyH264) ->
 * MuxerMTS -> HTTP client, driven by VideoMuxer and served by AudioServer.
 *
 * Tested on a Waveshare ESP32-P4-WIFI6 with its OV5647 MIPI-CSI camera:
 * the software encoder reaches about 9 fps at 320x240 there. Build with
 * esp32:esp32:esp32p4:ChipVariant=postv3,PSRAM=enabled,PartitionScheme=huge_app
 * For other cameras replace setPinsWaveshareEsp32P4Wifi6() with the
 * TinyCameraPins.h preset (or pins) of your board.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/TinyCamera
 * - https://github.com/pschatzmann/TinyH264
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMTS.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/Video/Camera/CameraFrameSourceTinyCamera.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/VideoMuxer.h"
#include "TinyCameraPins.h"


// Frame rate the camera is captured at: what the encoder can sustain
constexpr float kFps = 8;
const char *ssid = "ssid";
const char *password = "password";
AudioServer server(ssid, password);
CameraFrameSourceTinyCamera camera;
H264Encoder encoder;
MuxerMTS mts;
VideoMuxer video(mts);

// Streams to one client until it disconnects
void streamVideo(Print *out) {
  Serial0.println("client connected");
  mts.setOutput(*out);
  video.begin();  // restarts the encoder: the stream starts with a keyframe
  while (server.isClientConnected()) {
    if (video.copy() == 0) break;
  }
  video.end();
  Serial0.println("client disconnected");
}

void setup() {
  Serial0.begin(115200);
  AudioToolsLogger.begin(Serial0, AudioToolsLogLevel::Warning);

  // print start message
  while (!Serial0);
  Serial0.println("starting...");

  // camera: 320x240 RGB565
  camera_config_t config = tiny_camera::TinyCamera::defaultConfig();
  tiny_camera::setPinsWaveshareEsp32P4Wifi6(config);
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  if (!camera.begin(config, kFps)) {
    Serial0.println("camera begin failed");
    stop();
  }

  // H.264 encoder: fixed quality, a keyframe every 2 seconds
  VideoInfo info = camera.videoInfo();
  encoder.setSize(info.width, info.height);
  encoder.setVideoFormat(VideoFormat::RGB565);
  encoder.setQp(28);
  encoder.setKeyframeInterval(2 * kFps);
  encoder.driver().setAllOptimizationsActive(true);

  // live stream: timestamps from the capture time, so the player's latency
  // stays constant even if fewer than kFps frames/s get through
  mts.setRealTime(true);
  video.setVideoSource(camera);
  video.setVideoEncoder(encoder);

  // connects to WiFi (with power save switched off)
  server.begin(streamVideo, mts.mimeVideo());

  // print local ip address
  Serial0.print("Open http://");
  Serial0.print(WiFi.localIP());
  Serial0.println("/ in VLC or ffplay");
}

void loop() { server.copy(); }
