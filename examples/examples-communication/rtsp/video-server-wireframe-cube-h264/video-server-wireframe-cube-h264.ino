/*
 * ESP32-S3 RTSP H.264 Server streaming a synthetic TinyGPU wireframe scene
 * Author: Phil Schatzmann
 *
 * This example needs no camera at all: it renders a rotating 3D wireframe
 * cube into an RGB565 framebuffer using the TinyGPU library
 * (https://github.com/pschatzmann/TinyGPU), encodes it to H.264 with the
 * codec-h264-ESP32S3 library (https://github.com/pschatzmann/codec-h264-ESP32S3),
 * and streams it live over RTSP using H264RtpEncoder (RFC 6184).
 *
 * It is a variant of TinyGPU's own wireframe-cube-h264 example, which
 * streams the same scene as raw H.264/UDP - here the H.264 bytes go into
 * H264RtpEncoder instead, so any standard RTSP client (VLC, ffplay) can
 * connect without needing a raw UDP receiver.
 *
 * Like the RTSP audio sources, video is pulled rather than pushed:
 * RTSPMediaCallbackSource exposes H264RtpEncoder's already-packetized RTP
 * fragments through a read/packetSize callback pair.
 *
 * RTSP request handling runs on RTSPServer's own background tasks (see
 * RTSPServer.h), not in loop() - so loop() is free to just poll
 * rendering/encoding at its own pace without delaying PLAY/TEARDOWN/
 * keep-alive handling, and without needing a dedicated render task either.
 * Rendering is gated by rtspServer.clientCount(), checked directly in
 * loop() - no start/stop callback needed. This is a bit coarser than
 * PLAY/TEARDOWN (a client is "connected" from TCP accept through
 * DESCRIBE/SETUP, before it necessarily PLAYs), but harmless here:
 * RTSPMediaCallbackSource still gates actual RTP sending on its own
 * is_active flag (true only between PLAY/TEARDOWN), so any frame rendered a
 * moment early is simply dropped by H264RtpEncoder's 1-frame queue rather
 * than ever being sent. It also keeps h264Encoder.begin()/end() on the same
 * thread as rendering (loop()), so there's no cross-thread race against an
 * in-flight render/encode call.
 *
 * Additional libraries required (not part of arduino-audio-tools):
 * - TinyGPU: https://github.com/pschatzmann/TinyGPU
 * - codec-h264-ESP32S3: https://github.com/pschatzmann/codec-h264-ESP32S3
 *
 * Hardware Requirements:
 * - ESP32-S3 with PSRAM enabled (no camera needed)
 *
 * Usage:
 * 1. Update WiFi credentials below
 * 2. Compile with PSRAM enabled and a large partition scheme (the software
 *    H.264 encoder needs both flash space and a big loop-task stack)
 * 3. Build and run this example
 * 4. Connect with VLC or: ffplay rtsp://<device-ip>:8554/video
 */

#include <WiFi.h>

#include "AudioTools.h"
#include "AudioTools/Communication/RTSP.h"
#include "H264Encoder.h"
#include "TinyGPU.h"

using namespace audio_tools;

// The software H.264 encoder needs a bigger stack than the default loop
// task, since render/encode now runs directly on loop() (see renderLoop())
SET_LOOP_TASK_STACK_SIZE(64 * 1024);

// Network configuration
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";

// Framebuffer size (QVGA)
constexpr int WIDTH = 320;
constexpr int HEIGHT = 240;
const float VIDEO_FPS = 15.0f;
const unsigned long FRAME_DURATION_MS = (unsigned long)(1000.0f / VIDEO_FPS);

// TinyGPU framebuffer + wireframe renderer
FrameBufferRGB565 framebuffer(WIDTH, HEIGHT, FontRGB565);
WireFrame3D_RGB565 wireframe(framebuffer);
auto cubeMesh = WireFrame3D_RGB565::cube(2.0f);
float angle = 0.0f;

// Software H.264 encoder (no camera - fed with rendered RGB565 frames)
H264Encoder camEncoder;

// RTP payloader: does the RFC 6184 packetization and queues already
// RTP-ready fragments, retrieved via packetSize()/readBytes()
H264RtpEncoder h264Encoder;
RTSPFormatH264 h264Format(WIDTH, HEIGHT, VIDEO_FPS);

// Minimal Print adapter so H264Encoder::encodeRGB565(..., Print&) can feed
// its Annex-B output straight into H264RtpEncoder::write()
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
void renderLoop();

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

// Called from loop() while captureEnabled; paces itself to VIDEO_FPS since
// encodeRGB565() does not do this internally. Returns immediately (no
// delay/blocking) when not yet due.
void renderLoop() {
  static unsigned long lastFrameMs = 0;
  unsigned long now = millis();
  if (now - lastFrameMs < FRAME_DURATION_MS) return;
  lastFrameMs = now;

  // Render one frame of the rotating wireframe cube
  framebuffer.clear(RGB565(255, 255, 255));

  auto printer = framebuffer.linePrinter();
  printer.setColor(RGB565(0, 0, 255));
  printer.setScale(1);
  printer.print("RTSP Wireframe Cube Demo");

  auto model = WireFrame3D_RGB565::translation(0.0f, 0.0f, 0.0f) *
               WireFrame3D_RGB565::rotationY(angle) *
               WireFrame3D_RGB565::rotationX(angle * 0.7f);
  wireframe.renderWireframe(framebuffer, cubeMesh, model, RGB565(0, 0, 0));

  // Encode the rendered frame to H.264 and queue it for RTP: encodeRGB565()
  // writes the resulting Annex-B NAL units into h264Print, which forwards
  // them to H264RtpEncoder for RTP packetization.
  if (!camEncoder.encodeRGB565(framebuffer.data(), framebuffer.size(), h264Print)) {
    Serial.println("Frame encode failed");
  }

  angle += 0.03f;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32-S3 RTSP H.264 Server - TinyGPU Wireframe Cube");
  Serial.println("====================================================");

  // Configure and start the software H.264 encoder (no camera)
  auto camCfg = camEncoder.defaultConfig();
  camCfg.width = WIDTH;
  camCfg.height = HEIGHT;
  camCfg.fps = (int)VIDEO_FPS;
  camCfg.use_camera = false;
  if (!camEncoder.begin(camCfg)) {
    Serial.println("H.264 encoder initialization failed!");
    while (1) delay(1000);
  }
  h264Encoder.setFormat(h264Format);
  h264Encoder.setMaxFragmentSize(1400);  // Optimal for most networks

  videoSource.setPacketSizeCallback(videoPacketSize);

  // Set up the TinyGPU framebuffer and wireframe renderer
  framebuffer.begin();
  framebuffer.clear(RGB565(255, 255, 255));
  wireframe.begin();
  wireframe.setPerspective(60.0f, 0.1f, 100.0f);

  WireFrame3D_RGB565::Camera cam;
  cam.position = {0.0f, 0.0f, 5.0f};
  cam.target = {0.0f, 0.0f, 0.0f};
  cam.up = {0.0f, 1.0f, 0.0f};
  wireframe.setCamera(cam);

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
  Serial.printf("Stream URL: rtsp://%s:%d/video\n",
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

  // Enable/disable rendering based on client connectivity, read directly
  // from the server rather than a start/stop callback pair. Since
  // h264Encoder.begin()/end() now only ever run here (loop()), on the same
  // thread as rendering, there's no cross-thread race against an in-flight
  // renderLoop()/encode() call either.
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

  if (captureEnabled) renderLoop();
}
