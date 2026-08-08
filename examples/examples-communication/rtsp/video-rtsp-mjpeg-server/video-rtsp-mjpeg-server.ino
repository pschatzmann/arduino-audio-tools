/*
 * ESP32 Camera RTSP MJPEG Video Server Example
 * Author: Phil Schatzmann
 *
 * This example demonstrates how to use the JPEGRtpEncoder for proper
 * JPEG frame fragmentation and RTSP streaming from ESP32 camera.
 *
 * Like the RTSP audio sources, video is pulled rather than pushed:
 * RTSPMediaCallbackSource exposes JPEGRtpEncoder's already-packetized RTP
 * fragments through a read/packetSize callback pair.
 *
 * RTSP request handling runs on RTSPServer's own background tasks (see
 * RTSPServer.h), not in loop() - so loop() is free to just poll capture at
 * its own pace, no dedicated capture task needed either since
 * esp_camera_fb_get() is cheap. Capture is gated by rtspServer.clientCount(),
 * checked directly in loop() - no start/stop callback needed. This is a bit
 * coarser than PLAY/TEARDOWN (a client is "connected" from TCP accept
 * through DESCRIBE/SETUP, before it necessarily PLAYs), but harmless here:
 * RTSPMediaCallbackSource still gates actual RTP sending on its own
 * is_active flag (true only between PLAY/TEARDOWN), so any frame captured a
 * moment early is simply dropped by JPEGRtpEncoder's 1-frame queue rather
 * than ever being sent. It also keeps jpegEncoder.begin()/end() on the same
 * thread as capture (loop()), so there's no cross-thread race against an
 * in-flight capture call.
 *
 * Features:
 * - RFC 2435 compliant JPEG over RTP fragmentation
 * - Proper fragment offset handling for large frames
 * - Optimized bandwidth with optional header stripping
 * - Enhanced compatibility with RTSP clients
 * - Camera capture only runs while a client is connected
 *
 * Hardware Requirements:
 * - ESP32-CAM board or ESP32 with OV2640/OV3660 camera module
 * - Proper camera pin configuration (see camera_config below)
 *
 * Components demonstrated:
 * - ESP32 Camera API for JPEG frame capture
 * - JPEGRtpEncoder for RFC 2435 compliant frame processing
 * - RTSPMediaCallbackSource as a pull-based MediaSource for video
 * - RTSPFormatMJPEG for MJPEG format handling
 * - Real-time video streaming with proper frame fragmentation
 *
 * Usage:
 * 1. Update WiFi credentials below
 * 2. Configure camera pins for your board (ESP32-CAM pins provided)
 * 3. Build and run this example
 * 4. Connect with VLC: rtsp://192.168.1.100:8554/camera
 * 5. Video stream should display live camera feed with improved quality
 *
 * Performance Notes:
 * - JPEG fragmentation enables higher quality streaming
 * - Automatic frame splitting for optimal network transmission
 * - Monitor serial output for frame timing and fragmentation info
 */

#include <WiFi.h>

#include "AudioTools.h"
#include "AudioTools/Communication/RTSP.h"
#include "esp_camera.h"

using namespace audio_tools;

// Camera pin definitions for ESP32-CAM (AI-Thinker)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Network configuration
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";

// Video configuration
const int VIDEO_WIDTH = 800;   // SVGA width
const int VIDEO_HEIGHT = 600;  // SVGA height
const float VIDEO_FPS = 10.0;  // Conservative frame rate for stability
const unsigned long FRAME_DURATION_MS = 1000 / VIDEO_FPS;

// Camera and streaming state
static int frameCounter = 0;

// Camera initialization function
bool beginCamera(size_t bufferSize = 65536) {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Frame size and quality settings
  config.frame_size = FRAMESIZE_SVGA;  // 800x600
  config.jpeg_quality = 12;  // Lower number = higher quality (range: 0-63)
  config.fb_count = 2;       // Number of frame buffers

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  // Get camera sensor and configure settings
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    // Adjust camera settings for better video streaming
    sensor->set_brightness(sensor, 0);      // -2 to 2
    sensor->set_contrast(sensor, 0);        // -2 to 2
    sensor->set_saturation(sensor, 0);      // -2 to 2
    sensor->set_special_effect(sensor, 0);  // 0 to 6 (0=No Effect)
    sensor->set_whitebal(sensor, 1);        // 0 = disable , 1 = enable
    sensor->set_awb_gain(sensor, 1);        // 0 = disable , 1 = enable
    sensor->set_wb_mode(sensor, 0);         // 0 to 4 - if awb_gain enabled
    sensor->set_exposure_ctrl(sensor, 1);   // 0 = disable , 1 = enable
    sensor->set_aec2(sensor, 0);            // 0 = disable , 1 = enable
    sensor->set_ae_level(sensor, 0);        // -2 to 2
    sensor->set_aec_value(sensor, 300);     // 0 to 1200
    sensor->set_gain_ctrl(sensor, 1);       // 0 = disable , 1 = enable
    sensor->set_agc_gain(sensor, 0);        // 0 to 30
    sensor->set_gainceiling(sensor, (gainceiling_t)0);  // 0 to 6
    sensor->set_bpc(sensor, 0);       // 0 = disable , 1 = enable
    sensor->set_wpc(sensor, 1);       // 0 = disable , 1 = enable
    sensor->set_raw_gma(sensor, 1);   // 0 = disable , 1 = enable
    sensor->set_lenc(sensor, 1);      // 0 = disable , 1 = enable
    sensor->set_hmirror(sensor, 0);   // 0 = disable , 1 = enable
    sensor->set_vflip(sensor, 0);     // 0 = disable , 1 = enable
    sensor->set_dcw(sensor, 1);       // 0 = disable , 1 = enable
    sensor->set_colorbar(sensor, 0);  // 0 = disable , 1 = enable
  }

  Serial.println("Camera initialized successfully");
  return true;
}

// RTP payloader: does the RFC 2435 fragmentation and queues already
// RTP-ready fragments, retrieved via packetSize()/readBytes()
RTSPFormatMJPEG mjpegFormat(VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);
JPEGRtpEncoder jpegEncoder;

int readVideoPacket(uint8_t* buffer, int maxBytes, void* userData);
int videoPacketSize(void* userData);
void captureLoop();

// RTSPMediaCallbackSource: pull-based MediaSource wired to JPEGRtpEncoder's
// queue - same shape as a callback-based audio source, just packetized
RTSPMediaCallbackSource videoSource(mjpegFormat, readVideoPacket, &jpegEncoder);
RTSPMediaStreamer<RTSPPlatformWiFi> rtspStreamer(videoSource);
RTSPServer<RTSPPlatformWiFi> rtspServer(rtspStreamer);

// -- RTSPMediaCallbackSource callbacks ------------------------------------

int readVideoPacket(uint8_t* buffer, int maxBytes, void* userData) {
  return ((JPEGRtpEncoder*)userData)->readBytes(buffer, maxBytes);
}

int videoPacketSize(void* userData) {
  return ((JPEGRtpEncoder*)userData)->packetSize();
}

// Called from loop() while captureEnabled; paces itself to VIDEO_FPS since
// esp_camera_fb_get() (unlike H264Encoder::captureH264()) does not do this
// internally. Returns immediately (no delay/blocking) when not yet due.
void captureLoop() {
  static unsigned long lastFrameCapture = 0;
  unsigned long now = millis();
  if (now - lastFrameCapture < FRAME_DURATION_MS) return;
  lastFrameCapture = now;

  camera_fb_t* fb = esp_camera_fb_get();
  if (fb && fb->len > 0) {
    jpegEncoder.write(fb->buf, fb->len);
    frameCounter++;
  }
  if (fb) esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 Camera RTSP MJPEG Video Server");
  Serial.println("====================================");
  Serial.println("Features: JPEG Fragmentation, RFC 2435 Compliance");
  Serial.println();

  // Configure JPEG encoder for optimal video streaming
  jpegEncoder.setFormat(mjpegFormat);
  jpegEncoder.setMaxFragmentSize(1400);  // Optimal for most networks
  Serial.println("JPEG encoder configured for fragmentation");

  // Initialize camera first
  if (!beginCamera(65536)) {
    Serial.println("Camera initialization failed!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("Camera initialized successfully");

  videoSource.setPacketSizeCallback(videoPacketSize);

  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("WiFi connected successfully");

  // Start RTSP server
  rtspServer.begin();

  char ip_str[16];
  sprintf(ip_str, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1],
          WiFi.localIP()[2], WiFi.localIP()[3]);

  Serial.println();
  Serial.printf("RTSP Server started on port %d\n", rtspServer.getPort());
  Serial.printf("Camera stream URL: rtsp://%s:%d/camera\n", ip_str,
                rtspServer.getPort());
  Serial.println();
  Serial.println("Enhanced Features Enabled:");
  Serial.println("- JPEG frame fragmentation (RFC 2435)");
  Serial.println("- Proper fragment offset handling");
  Serial.println("- RTP marker bit on last fragment");
  Serial.println("- Optimized for large frame streaming");
  Serial.println();
  Serial.println("You can now connect with:");
  Serial.println("- VLC Media Player");
  Serial.printf("- FFplay: ffplay rtsp://%s:%d/camera\n", ip_str,
                rtspServer.getPort());
  Serial.println("- OpenCV VideoCapture");
  Serial.println();
  Serial.println("Camera is ready to stream!");
}

void loop() {
  // RTSPServer handles accept/session/RTSP-request processing on its own
  // background tasks - nothing to do here for that.

  // Enable/disable capture based on client connectivity, read directly from
  // the server rather than a start/stop callback pair. Since
  // jpegEncoder.begin()/end() now only ever run here (loop()), on the same
  // thread as capture, there's no cross-thread race against an in-flight
  // captureLoop() call either.
  static bool captureEnabled = false;
  bool clientConnected = rtspServer.clientCount() > 0;
  if (clientConnected != captureEnabled) {
    captureEnabled = clientConnected;
    if (captureEnabled) {
      jpegEncoder.begin();
    } else {
      jpegEncoder.end();
    }
  }

  if (captureEnabled) captureLoop();

  // Optional: Print status every 10 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    Serial.printf("RTSP Camera Server running - Streamed %d frames\n",
                  frameCounter);
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    lastStatus = millis();
  }
}
