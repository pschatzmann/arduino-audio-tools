/**
 * @file webserver-video-ext.ino
 * @brief Web server that streams live camera video with sound from the
 * microphone to a web browser: open
 * http://<ip-address>/ in Chrome, Edge or Firefox. Browsers can't play an
 * MPEG transport stream natively, so the page served at / plays the stream
 * at /stream.ts with mpegts.js (https://github.com/xqq/mpegts.js), loaded
 * from a CDN: the browser needs internet access. VLC, ffplay or mpv can
 * open http://<ip-address>/stream.ts directly.
 *
 * Pipeline:
 * - video: TinyCamera (RGB565 frames) -> H264EncoderESP32P4 (the P4's
 *   hardware H.264 encoder) -> MuxerMTS
 * - audio: microphone (ES8311 codec via I2SCodecStream, 16 kHz mono) ->
 *   AACEncoderVO (AAC-LC in ADTS frames) -> MuxerMTS
 * - MuxerMTS -> HTTP clients
 * VideoMuxerWithTasks runs one task per track; TinyHttp serves the clients
 * in loop(). They all use the muxer and the client list, so they share a
 * mutex. The camera and the audio codec share the board's I2C bus: the
 * camera uses the bus of Wire, which the audio driver starts.
 * Each frame is encoded once and sent to all connected out_streams; a viewer
 * starts with the next keyframe, which is sent every second. Browsers only
 * autoplay muted videos: unmute it with the player controls.
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
 * - https://github.com/pschatzmann/codec-h264-ESP32P4
 * - https://github.com/pschatzmann/codec-vo-aacenc
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/TinyHttp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <WiFi.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACVO.h"
#include "AudioTools/AudioCodecs/ContainerMTS.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "AudioTools/Video/Camera/CameraFrameSourceTinyCamera.h"
#include "AudioTools/Video/CodecH264ESP32P4.h"
#include "AudioTools/Video/VideoMuxerWithTasks.h"
#include "HttpExtensions.h"  // TinyHttp
#include "HttpServer.h"
#include "TinyCameraPins.h"

//SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// Frame rate the camera is captured at: what the encoder can sustain
constexpr float kFps = 8;
// Microphone: 16 kHz mono
AudioInfo audio_info(16000, 1, 16);
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

// web server: serves the player page and the MPEG transport stream
WiFiServer wifi;
HttpServer server(wifi);
ExtensionStream out_streams("/stream.ts", tinyhttp::T_GET, "video/mp2t");

// Muxer
MuxerMTS mts(out_streams);
VideoMuxerWithTasks video(mts);
Mutex mutex;  // shared by the video task and the web server

// video pipeline: camera -> H.264 encoder -> MuxerMTS
CameraFrameSourceTinyCamera camera;
H264EncoderESP32P4 encoder;

// audio pipeline: microphone -> AAC encoder -> MuxerMTS
I2SCodecStream mic(WaveshareESP32P4WIFI6);
AACEncoderVO aac;


void setup() {
  Serial0.begin(115200);
  AudioToolsLogger.begin(Serial0, AudioToolsLogLevel::Warning);

  while(!Serial0);
  Serial0.println("starting...");

  // microphone: also starts the I2C bus (Wire) for the audio codec
  auto mic_cfg = mic.defaultConfig(RX_MODE);
  mic_cfg.copyFrom(audio_info);
  if (!mic.begin(mic_cfg)) {
    Serial0.println("microphone begin failed");
    stop();
  }

  // camera: 320x240 RGB565, on the I2C bus of Wire
  camera_config_t config = tiny_camera::TinyCamera::defaultConfig();
  tiny_camera::setPinsWaveshareEsp32P4Wifi6(config);
  config.sccb_i2c_bus = (i2c_master_bus_handle_t)i2cBusHandle(0);
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  if (!camera.begin(config, kFps)) {
    Serial0.println("camera begin failed");
    stop();
  }

  // H.264 hardware encoder: a keyframe every second
  VideoInfo info = camera.videoInfo();
  encoder.setSize(info.width, info.height);
  encoder.setVideoFormat(VideoFormat::RGB565);
  encoder.setFrameRate(kFps);
  encoder.setGop(kFps);

  // live stream: timestamps from the capture time, so the player's latency
  // stays constant even if fewer than kFps frames/s get through
  mts.setRealTime(true);

  // web server: player page and stream
  server.on("/", tinyhttp::T_GET, "text/html", kPage);
  server.addExtension(out_streams);
  server.setNoConnectDelay(0);  // loop() waits outside of the mutex instead
  // connects to WiFi (with power save switched off)
  server.begin(80, ssid, password);

  Serial0.print("Open http://");
  Serial0.print(WiFi.localIP());
  Serial0.println("/ in a web browser");

  // starts the tasks: video at kFps, audio as the microphone delivers it
  // config muxer
  video.setVideoSource(camera);
  video.setVideoEncoder(encoder);
  video.setAudioSource(mic);
  video.setAudioEncoder(aac);
  video.setMutex(mutex);
  video.setTaskParameters(16 * 1024, 2);  // stack in bytes on ESP32
  video.begin();
}

void loop() {
  {
    // needed because the video task and the web server share the muxer and the client list
    LockGuard guard(mutex);
    server.copy();  // accepts new requests
  }
  delay(10);
}
