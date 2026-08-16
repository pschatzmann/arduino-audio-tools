/**
 * @file http-server-mpg.ino
 * @brief ESP32-S3: captures raw YUV422 frames from the camera, encodes them
 * to MPEG-1 (ISO/IEC 11172-2) with MPGEncoder (TinyMPG,
 * https://github.com/pschatzmann/TinyMPG - pure software, works on any
 * board), muxes the result into an MPEG-1 Program Stream with MuxerMPG, and
 * publishes it via a plain AudioServer (WiFi) HTTP server so it can be
 * watched live with e.g. VLC or ffplay: `vlc http://<esp32-ip-address>/`
 *
 * Unlike MuxerAVI/MuxerMP4, MuxerMPG needs no external wrapper format -
 * ISO/IEC 11172-1's own pack_header/system_header/PES_packet framing *is*
 * the container - so this is the MPEG-1 analog of http-server-avi-h264.ino
 * (see that file for the same pipeline built on AVI+H.264 instead; the two
 * are otherwise line-for-line the same shape).
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
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/Video/CodecMPG.h"
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
// QVGA - comfortably inside TinyMPG's default MPG_MAX_WIDTH/MPG_MAX_HEIGHT
// (CIF, 352x288 - see TinyMPG/src/mpg_config.h, tuned for a plain ESP32
// without PSRAM on the decode side); raise those #defines before including
// CodecMPG.h if you need a bigger picture and have PSRAM to spare.
const framesize_t camera_frame_size = FRAMESIZE_QVGA;
const uint16_t video_width = 320;
const uint16_t video_height = 240;
const float video_fps = 10;

AudioServer server(ssid, password);  // AudioServerT<WiFiClient, WiFiServer>
MuxerMPG muxer;
MPGEncoder mpgEncoder(video_width, video_height);

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
  config.pixel_format = PIXFORMAT_YUV422;  // matches mpgEncoder.setVideoFormat(VideoFormat::YUV422) below
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
  cfg.format = VideoFormat::MPEG1;
  muxer.setVideoInfo(cfg);
  if (!muxer.begin()) {
    Serial.println("MuxerMPG begin failed");
    return;
  }

  mpgEncoder.setOutput(muxer);  // encoded pictures go straight into the
                                // 'video/mpeg' Program Stream as they're produced
  mpgEncoder.setVideoFormat(VideoFormat::YUV422);  // matches the camera's raw output
  mpgEncoder.setFrameRate((int)video_fps);
  mpgEncoder.setQp(8);  // 1..31, lower = higher quality/larger output
  mpgEncoder.begin();

  const uint32_t frame_interval_ms = 1000 / (uint32_t)video_fps;
  while (server.isClientConnected()) {
    uint32_t start = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == nullptr) {
      Serial.println("Camera capture failed");
      continue;
    }
    if (fb->format == PIXFORMAT_YUV422) {
      mpgEncoder.write(fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);

    uint32_t elapsed = millis() - start;
    if (elapsed < frame_interval_ms) delay(frame_interval_ms - elapsed);
  }

  mpgEncoder.end();
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

  // serves the result of sendVideo() as "video/mpeg" to each connecting
  // client; AudioServer takes care of WiFi connection, HTTP handshake and
  // chunked transfer encoding
  server.begin(sendVideo, "video/mpeg");
}

void loop() { server.copy(); }
