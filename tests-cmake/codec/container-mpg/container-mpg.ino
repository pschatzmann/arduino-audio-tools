/**
 * @file container-mpg.ino
 * @author Phil Schatzmann
 * @brief Self-contained round-trip test for ContainerMPG: MuxerMPG writes
 * synthetic MPEG-1 video/audio elementary-stream frames into an in-memory
 * MPEG-1 Program Stream, DemuxerMPG parses that stream back apart while
 * being fed the bytes in small, arbitrarily-sized chunks (exercising the
 * partial-write/resync logic), and the reassembled elementary streams,
 * frame counts and parsed VideoInfo/AudioInfo are compared against what was
 * originally written. No external files/hardware required.
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./container-mpg
 *
 * @copyright Copyright (c) 2025
 */
#include <assert.h>
#include <stdio.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/CoreAudio/BaseStream.h"

using namespace audio_tools;

/// Collects everything written to it (for byte-exact comparison) and counts
/// flush() calls (one expected per demuxed video access unit).
class VerifyPrint : public Print {
 public:
  Vector<uint8_t> data;
  int flush_count = 0;
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t *buf, size_t len) override {
    for (size_t i = 0; i < len; i++) {
      uint8_t b = buf[i];
      data.push_back(b);
    }
    return len;
  }
  void flush() override { flush_count++; }
};

static void appendPattern(Vector<uint8_t> &v, size_t n, uint8_t seed) {
  for (size_t i = 0; i < n; i++) v.push_back((uint8_t)(seed + i));
}

static bool sameBytes(Vector<uint8_t> &a, Vector<uint8_t> &b) {
  if (a.size() != b.size()) {
    printf("  size mismatch: %d vs %d\n", (int)a.size(), (int)b.size());
    return false;
  }
  for (int i = 0; i < a.size(); i++) {
    if (a[i] != b[i]) {
      printf("  byte %d mismatch: 0x%02X vs 0x%02X\n", i, a[i], b[i]);
      return false;
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  const uint16_t kWidth = 64;
  const uint16_t kHeight = 48;
  const float kFps = 25.0f;  // must be one of the ISO 11172-2 rate table values
  const uint32_t kSampleRate = 44100;
  const uint8_t kChannels = 2;
  const int kVideoFrames = 5;
  const int kAudioFrames = 5;

  // ---- 1) mux synthetic frames into an in-memory Program Stream, ------
  //         keeping a copy of everything written for later comparison.
  // (frames are built and muxed one at a time in the same loop rather
  // than stored in a Vector<Vector<uint8_t>> - this codebase's Vector
  // does not support non-POD/nested element types.)
  Vector<uint8_t> expected_video;
  Vector<uint8_t> expected_audio;

  DynamicMemoryStream mux_buffer;
  mux_buffer.begin();

  MuxerMPG muxer(mux_buffer);
  MuxerVideoConfig cfg;
  cfg.width = kWidth;
  cfg.height = kHeight;
  cfg.fps = kFps;
  cfg.format = VideoFormat::MPEG1;
  muxer.setVideoInfo(cfg);
  muxer.setAudioInfo(AudioInfoFormat(kSampleRate, kChannels, 16, AudioFormat::MP3));
  assert(muxer.begin());

  for (int i = 0; i < kVideoFrames; i++) {
    Vector<uint8_t> frame;
    if (i == 0) {
      // sequence_header: width=64, height=48, aspect=1, rate_index=3 (25fps)
      uint8_t hdr[8] = {0x00, 0x00, 0x01, 0xB3, 0x04, 0x00, 0x30, 0x13};
      for (int j = 0; j < 8; j++) frame.push_back(hdr[j]);
    } else {
      // fake picture_start_code so it at least looks like an access unit
      uint8_t hdr[4] = {0x00, 0x00, 0x01, 0x00};
      for (int j = 0; j < 4; j++) frame.push_back(hdr[j]);
    }
    // frame 2 is oversized to force MuxerMPG's multi-pack chunking
    size_t payload = (i == 2) ? 10000 : 40;
    appendPattern(frame, payload, (uint8_t)(0x10 * i));

    size_t written = muxer.addVideoFrame(frame.data(), frame.size());
    assert(written == (size_t)frame.size());
    for (int j = 0; j < frame.size(); j++) expected_video.push_back(frame[j]);

    if (i < kAudioFrames) {
      Vector<uint8_t> aframe;
      // MPEG-1 Layer III, 44100 Hz, stereo sync/header (a real MP3 header)
      uint8_t ahdr[4] = {0xFF, 0xFB, 0x50, 0x04};
      for (int j = 0; j < 4; j++) aframe.push_back(ahdr[j]);
      appendPattern(aframe, 60, (uint8_t)(0x80 + i));

      size_t awritten = muxer.addAudioFrame(aframe.data(), aframe.size());
      assert(awritten == (size_t)aframe.size());
      for (int j = 0; j < aframe.size(); j++)
        expected_audio.push_back(aframe[j]);
    }
  }
  muxer.end();

  printf("Muxed program stream: %d bytes\n", (int)mux_buffer.size());
  assert(mux_buffer.size() > 0);

  // ---- 3) demux, feeding bytes back in small, awkward-sized chunks ---
  mux_buffer.rewind();
  DemuxerMPG demuxer;
  VerifyPrint video_out;
  VerifyPrint audio_out;
  demuxer.setOutputVideo(video_out);
  demuxer.setOutputAudio(audio_out);
  assert(demuxer.begin());

  const size_t kChunk = 23;  // deliberately not aligned to any header size
  uint8_t buf[kChunk];
  int total_fed = 0;
  while (mux_buffer.available() > 0) {
    size_t n = mux_buffer.readBytes(buf, kChunk);
    if (n == 0) break;
    size_t written = demuxer.write(buf, n);
    assert(written == n);
    total_fed += (int)n;
  }
  demuxer.end();
  printf("Fed %d bytes into DemuxerMPG\n", total_fed);

  // ---- 4) verify ------------------------------------------------------
  bool ok = true;

  printf("Checking video ES round-trip...\n");
  if (!sameBytes(expected_video, video_out.data)) ok = false;

  printf("Checking audio ES round-trip...\n");
  if (!sameBytes(expected_audio, audio_out.data)) ok = false;

  printf("video flush() count: %d (expected %d)\n", video_out.flush_count,
         kVideoFrames);
  if (video_out.flush_count != kVideoFrames) ok = false;

  VideoInfo vi = demuxer.getVideoInfo();
  printf("parsed VideoInfo: %d x %d @ %.3f fps\n", (int)vi.width,
         (int)vi.height, vi.fps);
  if (vi.width != kWidth || vi.height != kHeight || vi.fps != kFps) ok = false;

  AudioInfoFormat ai = demuxer.getAudioInfo();
  printf("parsed AudioInfo: %d Hz, %d ch\n", (int)ai.sample_rate,
         (int)ai.channels);
  if (ai.sample_rate != kSampleRate || ai.channels != kChannels) ok = false;

  if (ok) {
    printf("PASS: ContainerMPG round-trip OK\n");
  } else {
    printf("FAIL: ContainerMPG round-trip mismatch\n");
  }
  assert(ok);
  exit(ok ? 0 : 1);
}

void loop() {}
