/*
 * Desktop RTSP MJPEG video streaming test.
 *
 * Mirrors examples/examples-communication/rtsp/video-server-mjpeg, but runs
 * on the desktop (via the Arduino-Emulator's WiFiServer/WiFiClient/WiFiUDP)
 * instead of an ESP32-CAM, so it can be built and exercised without any
 * hardware. Loops a single synthetic JPEG test frame (frame_jpeg.h) through
 * JPEGRtpEncoder's RFC 2435 fragmentation and RTSPMediaCallbackSource.
 *
 * Connect with: ffplay rtsp://127.0.0.1:8554/video
 */
#include "AudioTools.h"
#include "AudioTools/Communication/RTSP/RTSPPlatformWiFi.h"
#include "AudioTools/Communication/RTSP.h"
#include "frame_jpeg.h"

using namespace audio_tools;

const int port = 8554;
const int VIDEO_WIDTH = 320;
const int VIDEO_HEIGHT = 240;
const float VIDEO_FPS = 5.0f;
const unsigned long FRAME_DURATION_MS = 1000 / VIDEO_FPS;

RTSPFormatMJPEG mjpegFormat(VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);
// JPEGRtpEncoder is itself an IMediaSource (RTSPVideoEncoder : AudioEncoder,
// IMediaSource), so it can go straight into RTSPMediaStreamer - no
// RTSPMediaCallbackSource/callback wrapper needed.
JPEGRtpEncoder jpegEncoder;

RTSPMediaStreamer<RTSPPlatformWiFi> rtspStreamer(jpegEncoder);
RTSPServer<RTSPPlatformWiFi> rtspServer(rtspStreamer, port);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  jpegEncoder.setFormat(mjpegFormat);
  jpegEncoder.setMaxFragmentSize(1400);

  rtspServer.begin();
}

void loop() {
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

  if (captureEnabled) {
    static unsigned long lastFrame = 0;
    unsigned long now = millis();
    if (now - lastFrame >= FRAME_DURATION_MS) {
      lastFrame = now;
      jpegEncoder.write(frame_jpeg, frame_jpeg_len);
    }
  }
}
