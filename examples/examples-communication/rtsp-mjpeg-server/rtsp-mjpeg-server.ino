/*
 * ESP32 Camera RTSP MJPEG Video Server Example
 * Author: Phil Schatzmann
 *
 * This example demonstrates how to use the JPEGRtpEncoder for proper
 * JPEG frame fragmentation and RTSP streaming from ESP32 camera.
 *
 * Features:
 * - RFC 2435 compliant JPEG over RTP fragmentation
 * - Proper fragment offset handling for large frames
 * - Optimized bandwidth with optional header stripping
 * - Enhanced compatibility with RTSP clients
 *
 * Hardware Requirements:
 * - ESP32-CAM board or ESP32 with OV2640/OV3660 camera module
 * - Proper camera pin configuration (see camera_config below)
 *
 * Components demonstrated:
 * - ESP32 Camera API for JPEG frame capture
 * - JPEGRtpEncoder for RFC 2435 compliant frame processing
 * - RTSPOutput with encoder pattern for clean architecture
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

// Camera and streaming state
static int frameCounter = 0;
static unsigned long lastFrameTime = 0;
static unsigned long frameDuration = 1000 / VIDEO_FPS;  // ms between frames
static bool streamingActive = false;
static bool cameraInitialized = false;

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

// RTSP Server and components with JPEG fragmentation encoder
RTSPFormatMJPEG mjpegFormat(VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);
JPEGRtpEncoder jpegEncoder;
RTSPOutput<RTSPPlatformWiFi> rtspOutput(mjpegFormat, jpegEncoder);
RTSPServerTaskless<RTSPPlatformWiFi> rtspServer(rtspOutput.streamer());

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 Camera RTSP MJPEG Video Server");
  Serial.println("====================================");
  Serial.println("Features: JPEG Fragmentation, RFC 2435 Compliance");
  Serial.println();

  // Configure JPEG encoder for optimal video streaming
  jpegEncoder.setMaxFragmentSize(1400);    // Optimal for most networks
  jpegEncoder.setStripJpegHeaders(false);  // Keep headers for compatibility
  Serial.println("✓ JPEG encoder configured for fragmentation");

  // Initialize camera first
  if (!beginCamera(65536)) {
    Serial.println("Camera initialization failed!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("✓ Camera initialized successfully");

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
  Serial.println("✓ WiFi connected successfully");

  // Initialize RTSP output with JPEG encoder
  if (!rtspOutput.begin()) {
    Serial.println("Failed to start RTSP output!");
    while (1) delay(1000);
  }
  Serial.println("✓ RTSP output initialized");

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
  Serial.println("✓ JPEG frame fragmentation (RFC 2435)");
  Serial.println("✓ Proper fragment offset handling");
  Serial.println("✓ RTP marker bit on last fragment");
  Serial.println("✓ Optimized for large frame streaming");
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
  // Handle RTSP server requests
  rtspServer.doLoop();

  // Capture and stream camera frames using JPEG encoder
  static unsigned long lastFrameCapture = 0;
  if (millis() - lastFrameCapture >= frameDuration) {
    // Check if output is ready for more data
    if (rtspOutput.availableForWrite() > 0) {
      // Capture frame from camera
      camera_fb_t* fb = esp_camera_fb_get();
      if (fb && fb->len > 0) {
        // Stream JPEG frame through fragmentation encoder
        size_t processed = rtspOutput.write(fb->buf, fb->len);

        if (processed > 0) {
          frameCounter++;
          Serial.printf("Frame %d: %d bytes → %d processed (fragmented)\n",
                        frameCounter, (int)fb->len, (int)processed);
        }

        // Return frame buffer to camera driver
        esp_camera_fb_return(fb);
        lastFrameCapture = millis();
      } else {
        if (fb) esp_camera_fb_return(fb);
      }
    }
  }

  // Optional: Print status every 10 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    Serial.printf("RTSP Camera Server running - Streamed %d frames\n",
                  frameCounter);
    Serial.printf("JPEG Encoder: fragmentation active, max size: %d bytes\n",
                  1400);

    // Optional: Print memory usage
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    lastStatus = millis();
  }

  delay(1);
}
