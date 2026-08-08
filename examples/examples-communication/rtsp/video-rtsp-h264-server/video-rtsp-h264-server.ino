/*
 * ESP32-S3 Camera RTSP H.264 Video Server Example
 * Author: Phil Schatzmann
 *
 * This example demonstrates how to stream H.264 video over RTSP from an
 * ESP32-S3 camera, using the codec-h264-ESP32S3 library
 * (https://github.com/pschatzmann/codec-h264-ESP32S3) for camera capture and
 * software H.264 encoding, and H264RtpEncoder for RFC 6184 RTP
 * packetization.
 *
 * Unlike a push-style video sketch (where loop() explicitly writes each
 * captured frame into the RTSP pipeline), this example pulls video the same
 * way RTSP audio sources do: RTSPMediaCallbackSource exposes H264RtpEncoder's
 * already-packetized RTP fragments through a read/packetSize callback pair.
 *
 * RTSP request handling runs on RTSPServer's own background tasks (see
 * RTSPServer.h), not in loop() - so loop() is free to just poll capture at
 * its own pace without delaying PLAY/TEARDOWN/keep-alive handling, and
 * without needing a dedicated capture task either. Capture is gated by
 * rtspServer.clientCount(), checked directly in loop() - no start/stop
 * callback needed. This is a bit coarser than PLAY/TEARDOWN (a client is
 * "connected" from TCP accept through DESCRIBE/SETUP, before it necessarily
 * PLAYs), but harmless here: RTSPMediaCallbackSource still gates actual RTP
 * sending on its own is_active flag (true only between PLAY/TEARDOWN), so
 * any frame captured a moment early is simply dropped by H264RtpEncoder's
 * 1-frame queue rather than ever being sent. It also keeps
 * h264Encoder.begin()/end() on the same thread as capture (loop()), so
 * there's no cross-thread race against an in-flight capture/encode call.
 *
 * Note: this uses H264Encoder::captureFrame()/encode() rather than the
 * higher-level captureH264(), because captureH264() enforces VIDEO_FPS with
 * its own internal blocking delay(), which would stall loop() (and,
 * previously, RTSP request handling that shared its thread) for the same
 * amount of time. captureLoop() below does its own non-blocking pacing
 * check instead (like the MJPEG/wireframe-cube examples).
 *
 * Additional library required (not part of arduino-audio-tools):
 * - codec-h264-ESP32S3: https://github.com/pschatzmann/codec-h264-ESP32S3
 *
 * Features:
 * - Software H.264 encoding (openh264-based) via H264Encoder
 * - RFC 6184 packetization-mode=1 (Single NAL Unit + FU-A fragmentation)
 * - SPS/PPS captured automatically and advertised in the SDP as soon as the
 *   first frame has been encoded (sprop-parameter-sets/profile-level-id),
 *   and also re-sent in-band before every IDR frame for clients that join
 *   mid-stream
 * - Camera capture only runs while a client is connected
 *
 * Hardware Requirements:
 * - ESP32-S3 board with camera module and PSRAM (e.g. ESP32-S3-EYE,
 *   ESP32-S3-CAM); H264Encoder::defaultConfig(name) has built-in pin
 *   mappings for several common boards - adjust the preset name below to
 *   match yours, or fall back to defaultConfig() and set the pins manually.
 *
 * Usage:
 * 1. Update WiFi credentials below
 * 2. Compile with PSRAM enabled and a large partition scheme (the software
 *    encoder needs both flash space and a big loop-task stack)
 * 3. Build and run this example
 * 4. Connect with VLC or: ffplay rtsp://<device-ip>:8554/video
 */

#include <WiFi.h>

#include "AudioTools.h"
#include "AudioTools/Communication/RTSP.h"
#include "H264Encoder.h"

using namespace audio_tools;

// The software H.264 encoder needs a bigger stack than the default loop
// task, since capture/encode now runs directly on loop() (see captureLoop())
SET_LOOP_TASK_STACK_SIZE(64 * 1024);

// Network configuration
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";

// Video configuration
const int VIDEO_WIDTH = 320;
const int VIDEO_HEIGHT = 240;
const float VIDEO_FPS = 10.0f;
const unsigned long FRAME_DURATION_MS = (unsigned long)(1000.0f / VIDEO_FPS);

// Camera + H.264 encoder (writes Annex-B NAL units to any Print target)
H264EncoderPSRAM camEncoder;

// I420 scratch buffer for captureFrame()/encode(), PSRAM-backed like
// H264Encoder's own internal buffers; sized in setup() once dimensions are
// known (I420 = width * height * 1.5 bytes)
PSVec<uint8_t> i420Buf;

// RTP payloader: does the RFC 6184 packetization and queues already
// RTP-ready fragments, retrieved via packetSize()/readBytes()
H264RtpEncoder h264Encoder;
RTSPFormatH264 h264Format(VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);

// Minimal Print adapter so H264Encoder::captureH264(Print&) can feed its
// Annex-B output straight into H264RtpEncoder::write()
class EncoderPrintAdapter : public Print {
 public:
  EncoderPrintAdapter(AudioEncoder& enc) : p_enc(&enc) {}
  size_t write(uint8_t b) override { return p_enc->write(&b, 1); }
  size_t write(const uint8_t* data, size_t len) override {
    return p_enc->write(data, len);
  }

 private:
  AudioEncoder* p_enc;
};
EncoderPrintAdapter h264Print(h264Encoder);

int readVideoPacket(uint8_t* buffer, int maxBytes, void* userData);
int videoPacketSize(void* userData);
void captureLoop();

// RTSPMediaCallbackSource: pull-based MediaSource wired to H264RtpEncoder's
// queue - same shape as a callback-based audio source, just packetized
RTSPMediaCallbackSource videoSource(h264Format, readVideoPacket, &h264Encoder);

RTSPMediaStreamer<RTSPPlatformWiFi> rtspStreamer(videoSource);
RTSPServer<RTSPPlatformWiFi> rtspServer(rtspStreamer);

// -- RTSPMediaCallbackSource callbacks ------------------------------------

int readVideoPacket(uint8_t* buffer, int maxBytes, void* userData) {
  return ((H264RtpEncoder*)userData)->readBytes(buffer, maxBytes);
}

int videoPacketSize(void* userData) {
  return ((H264RtpEncoder*)userData)->packetSize();
}

// Called from loop() while captureEnabled; paces itself to VIDEO_FPS using
// the same non-blocking check as the other video examples (captureFrame()/
// encode() do not delay internally, unlike captureH264()).
void captureLoop() {
  static unsigned long lastFrameMs = 0;
  unsigned long now = millis();
  if (now - lastFrameMs < FRAME_DURATION_MS) return;
  lastFrameMs = now;

  if (!camEncoder.captureFrame(i420Buf.data(), i420Buf.size())) {
    Serial.println("Frame capture failed");
    return;
  }
  if (!camEncoder.encode(i420Buf.data(), i420Buf.size(), h264Print)) {
    Serial.println("Frame encode failed");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32-S3 Camera RTSP H.264 Video Server");
  Serial.println("========================================");

  // Configure the camera + software H.264 encoder. Replace
  // "ESP32-S3-N16R8-CAM" with a preset matching your board (see
  // H264Encoder::defaultConfig() in codec-h264-ESP32S3 for the full list),
  // or use defaultConfig() and set the pin fields yourself.
  auto camCfg = camEncoder.defaultConfig("ESP32-S3-N16R8-CAM");
  camCfg.width = VIDEO_WIDTH;
  camCfg.height = VIDEO_HEIGHT;
  camCfg.fps = (int)VIDEO_FPS;
  camCfg.use_camera = true;
  if (!camEncoder.begin(camCfg)) {
    Serial.println("Camera/encoder initialization failed!");
    while (1) delay(1000);
  }
  Serial.println("Camera and H.264 encoder initialized successfully");

  i420Buf.resize((size_t)VIDEO_WIDTH * VIDEO_HEIGHT * 3 / 2);

  h264Encoder.setFormat(h264Format);
  h264Encoder.setMaxFragmentSize(1400);  // Optimal for most networks

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

  rtspServer.begin();

  Serial.println();
  Serial.printf("RTSP Server started on port %d\n", rtspServer.getPort());
  Serial.printf("Camera stream URL: rtsp://%s:%d/video\n",
                WiFi.localIP().toString().c_str(), rtspServer.getPort());
  Serial.println();
  Serial.println("You can now connect with:");
  Serial.println("- VLC Media Player");
  Serial.printf("- FFplay: ffplay rtsp://%s:%d/video\n",
                WiFi.localIP().toString().c_str(), rtspServer.getPort());
}

void loop() {
  // RTSPServer handles accept/session/RTSP-request processing on its own
  // background tasks - nothing to do here for that.

  // Enable/disable capture based on client connectivity, read directly from
  // the server rather than a start/stop callback pair. Since
  // h264Encoder.begin()/end() now only ever run here (loop()), on the same
  // thread as capture, there's no cross-thread race against an in-flight
  // captureLoop()/encode() call either.
  static bool captureEnabled = false;
  bool clientConnected = rtspServer.clientCount() > 0;
  if (clientConnected != captureEnabled) {
    captureEnabled = clientConnected;
    if (captureEnabled) {
      h264Encoder.begin();
    } else {
      h264Encoder.end();
    }
  }

  if (captureEnabled) captureLoop();
}
