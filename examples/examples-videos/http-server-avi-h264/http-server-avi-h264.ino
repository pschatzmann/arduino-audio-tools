/**
 * @file http-server-avi-h264.ino
 * @brief ESP32-S3: captures raw YUV422 frames from the camera, encodes them
 * to H.264 with H264Encoder (TinyH264,
 * https://github.com/pschatzmann/TinyH264 - pure software, works on any
 * board), muxes the result into a (video-only) H.264-in-AVI stream with
 * MuxerAVI, and publishes it via a plain AudioServer (WiFi) HTTP server so
 * it can be watched live with e.g. VLC or ffplay:
 * `vlc http://<esp32-ip-address>/`
 *
 * On an ESP32-S3 board, swap H264Encoder for H264EncoderESP32S3
 * (AudioTools/Video/CodecH264ESP32S3.h) to use the hardware/esp_h264
 * backend (https://github.com/pschatzmann/ESP32S3-h264) instead - it also
 * offers a built-in camera-capture loop (see that library's own
 * CameraEncoderUDP example) if you don't need MuxerAVI/HTTP.
 *
 * Hardware: pin mapping below matches the ESP32-S3 mic+camera board in
 * examples/examples-custom-boards/esp32s3-mic-cam (adjust CAMERA_PIN_* for
 * your own board) with the Arduino-ESP32 "esp32-camera" component (ships
 * with the ESP32 board package). Requires PSRAM (set "PSRAM: QSPI PSRAM" in
 * Tools) for the frame buffers, and "USB CDC on Boot: Enabled" to see
 * Serial.print output.
 *
 * @note AudioServer serves a single client at a time and the streaming
 * callback below blocks (in a simple while loop) for as long as that
 * client stays connected, so loop() - and any other client trying to
 * connect - is stalled while a video is playing. That keeps this example
 * simple; split video capture and network I/O across two cores/tasks if
 * you need it non-blocking.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/Video/CodecH264.h"
#include "esp_camera.h"

// ---- WiFi ----
const char *ssid = "ssid";
const char *password = "password";

// ---- Camera pins: ESP32-S3 mic+camera board (examples-custom-boards/esp32s3-mic-cam) ----
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 10
#define CAMERA_PIN_SIOD 21
#define CAMERA_PIN_SIOC 14
#define CAMERA_PIN_Y9 11
#define CAMERA_PIN_Y8 9
#define CAMERA_PIN_Y7 8
#define CAMERA_PIN_Y6 6
#define CAMERA_PIN_Y5 4
#define CAMERA_PIN_Y4 2
#define CAMERA_PIN_Y3 3
#define CAMERA_PIN_Y2 5
#define CAMERA_PIN_VSYNC 13
#define CAMERA_PIN_HREF 12
#define CAMERA_PIN_PCLK 7

// ---- Video / stream settings ----
// QVGA - H264Encoder (TinyH264) targets microcontrollers without PSRAM by
// default (see TinyH264/src/h264_config.h H264_MAX_WIDTH/HEIGHT); raise
// those #defines before including CodecH264.h if you need VGA and have
// PSRAM to spare.
const framesize_t camera_frame_size = FRAMESIZE_QVGA;
const uint16_t video_width = 320;
const uint16_t video_height = 240;
const float video_fps = 10;

AudioServer server(ssid, password);  // AudioServerT<WiFiClient, WiFiServer>
MuxerAVI muxer;
H264Encoder<> h264Encoder;

bool setupCamera() {
  camera_config_t config = {};
  config.pin_pwdn = CAMERA_PIN_PWDN;
  config.pin_reset = CAMERA_PIN_RESET;
  config.pin_xclk = CAMERA_PIN_XCLK;
  config.pin_sccb_sda = CAMERA_PIN_SIOD;
  config.pin_sccb_scl = CAMERA_PIN_SIOC;
  config.pin_d7 = CAMERA_PIN_Y9;
  config.pin_d6 = CAMERA_PIN_Y8;
  config.pin_d5 = CAMERA_PIN_Y7;
  config.pin_d4 = CAMERA_PIN_Y6;
  config.pin_d3 = CAMERA_PIN_Y5;
  config.pin_d2 = CAMERA_PIN_Y4;
  config.pin_d1 = CAMERA_PIN_Y3;
  config.pin_d0 = CAMERA_PIN_Y2;
  config.pin_vsync = CAMERA_PIN_VSYNC;
  config.pin_href = CAMERA_PIN_HREF;
  config.pin_pclk = CAMERA_PIN_PCLK;
  config.xclk_freq_hz = 20000000;
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.pixel_format = PIXFORMAT_YUV422;  // matches h264Encoder.setVideoFormat(VideoFormat::YUV422) below
  config.frame_size = camera_frame_size;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  return esp_camera_init(&config) == ESP_OK;
}

// Called once per connected client; keeps writing frames until it
// disconnects
void sendVideo(Print *out) {
  Serial.println("Client connected - streaming video...");

  muxer.setOutput(*out);
  MuxerVideoConfig cfg;
  cfg.width = video_width;
  cfg.height = video_height;
  cfg.fps = video_fps;
  cfg.format = VideoFormat::H264;
  muxer.setVideoInfo(cfg);
  if (!muxer.begin()) {
    Serial.println("MuxerAVI begin failed");
    return;
  }

  h264Encoder.setOutput(muxer);  // encoded NAL units go straight into the
                                 // 'video/avi' container as they're produced
  h264Encoder.setVideoFormat(VideoFormat::YUV422);  // matches the camera's raw output
  h264Encoder.setSize(video_width, video_height);
  h264Encoder.setKeyframeInterval((int)video_fps);  // one keyframe/sec
  h264Encoder.setQp(30);
  h264Encoder.begin();

  const uint32_t frame_interval_ms = 1000 / (uint32_t)video_fps;
  while (server.isClientConnected()) {
    uint32_t start = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == nullptr) {
      Serial.println("Camera capture failed");
      continue;
    }
    if (fb->format == PIXFORMAT_YUV422) {
      h264Encoder.write(fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);

    uint32_t elapsed = millis() - start;
    if (elapsed < frame_interval_ms) delay(frame_interval_ms - elapsed);
  }

  h264Encoder.end();
  muxer.end();
  Serial.println("Client disconnected");
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!setupCamera()) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }

  // serves the result of sendVideo() as "video/avi" to each connecting
  // client; AudioServer takes care of WiFi connection, HTTP handshake and
  // chunked transfer encoding
  server.begin(sendVideo, "video/avi");
}

void loop() { server.copy(); }
