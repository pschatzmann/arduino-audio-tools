/**
 * @file rtspserver-video-ext.ino
 * @brief RTSP server that streams live camera video with sound from the
 * microphone: open rtsp://<ip-address>:8554/ in VLC, ffplay or mpv.
 *
 * The RTSP server provides one track per session, so video and audio are
 * first muxed into an MPEG transport stream, which is sent as one RTP track
 * (MP2T, RFC 2250): the player demuxes and synchronizes video and audio
 * itself.
 *
 * Pipeline:
 * - video: TinyCamera (RGB565 frames) -> H264EncoderESP32P4 (the P4's
 *   hardware H.264 encoder) -> MuxerMTS
 * - audio: microphone (ES8311 codec via I2SCodecStream, 16 kHz mono) ->
 *   AACEncoderVO (AAC-LC in ADTS frames) -> MuxerMTS
 * - MuxerMTS -> RTSPMediaSourceMTS -> RTSPMediaStreamer -> RTSP client
 * VideoMuxerWithTasks runs one task per track; the RTSP server runs in its
 * own tasks. A client starts with the next keyframe, which is sent every
 * second. One client is served at a time.
 *
 * Over UDP (the default of most players) packets that get lost on a weak
 * WiFi connection are gone, which breaks up the picture and the sound. Ask
 * the player for TCP instead: ffplay -rtsp_transport tcp rtsp://<ip>:8554/
 * (VLC: Preferences > Input/Codecs > RTP over RTSP (TCP)). If the network
 * is still too slow, RTSPMediaSourceMTS drops video frames rather than
 * audio, so the sound continues while the picture freezes; a lower kFps
 * reduces the bitrate.
 *
 * See webserver-video-ext.ino for the same stream via HTTP, which can also
 * be played in a web browser.
 *
 * Written for a Waveshare ESP32-P4-WIFI6 with its OV5647 MIPI-CSI camera.
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
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <WiFi.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACVO.h"
#include "AudioTools/AudioCodecs/ContainerMTS.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "AudioTools/Communication/RTSP.h"
#include "AudioTools/Video/Camera/CameraFrameSourceTinyCamera.h"
#include "AudioTools/Video/CodecH264ESP32P4.h"
#include "AudioTools/Video/VideoMuxerWithTasks.h"
#include "TinyCameraPins.h"

// Frame rate the camera is captured at: what the encoder can sustain
constexpr float kFps = 8;
// Microphone: 16 kHz mono
AudioInfo audio_info(16000, 1, 16);
const char *ssid = "ssid";
const char *password = "password";

// RTSP server: sends the MPEG transport stream as one RTP track
RTSPMediaSourceMTS source;
RTSPMediaStreamer<RTSPPlatformWiFi> streamer(source);
RTSPServer<RTSPPlatformWiFi> rtsp(streamer);  // port 8554

// Muxer
MuxerMTS mts(source);
VideoMuxerWithTasks video(mts);

// video pipeline: camera -> H.264 encoder -> MuxerMTS
CameraFrameSourceTinyCamera camera;
H264EncoderESP32P4 encoder;

// audio pipeline: microphone -> AAC encoder -> MuxerMTS
I2SCodecStream mic(WaveshareESP32P4WIFI6);
AACEncoderVO aac;

void setup() {
  Serial0.begin(115200);
  AudioToolsLogger.begin(Serial0, AudioToolsLogLevel::Warning);

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

  // live stream: timestamps from the capture time
  mts.setRealTime(true);

  // connects to WiFi (with power save switched off) and starts the server
  rtsp.begin(ssid, password);
  Serial0.print("Open rtsp://");
  Serial0.print(WiFi.localIP());
  Serial0.println(":8554/ in VLC or ffplay");

  // starts the tasks: video at kFps, audio as the microphone delivers it
  video.setVideoSource(camera);
  video.setVideoEncoder(encoder);
  video.setAudioSource(mic);
  video.setAudioEncoder(aac);
  video.setTaskParameters(16 * 1024, 2);  // stack in bytes on ESP32
  video.begin();
}

void loop() { delay(1000); }
