/**
 * @file container-mts.ino
 * @author Phil Schatzmann
 * @brief Self-contained round-trip test for ContainerMTS: MuxerMTS writes
 * synthetic video access units (H.264-shaped in one pass, MJPEG in
 * another) and ADTS-framed AAC audio frames into an in-memory MPEG
 * Transport Stream (PAT/PMT + 188-byte TS packets), DemuxerMTS parses
 * that stream back apart while being fed the bytes in small,
 * arbitrarily-sized chunks (exercising the sync-byte resync and
 * cross-packet PES reassembly logic), and the reassembled elementary
 * streams, frame counts and parsed VideoInfo/AudioInfo are compared
 * against what was originally written. No external files/hardware
 * required.
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./container-mts
 *
 * @copyright Copyright (c) 2025
 */
#include <assert.h>
#include <stdio.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMTS.h"
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

/// Runs one full mux -> demux -> verify round-trip. useJpegApi selects
/// MuxerMTS::addJpegFrame() (MJPEG, VideoFormat::MJPEG) instead of
/// addVideoFrame() (H.264-shaped, VideoFormat::H264) for the video track.
static bool runRoundTrip(bool useJpegApi) {
  const uint32_t kSampleRate = 44100;
  const uint8_t kChannels = 2;
  const int kVideoFrames = 5;
  const int kAudioFrames = 5;
  const VideoFormat kVideoFormat = useJpegApi ? VideoFormat::MJPEG : VideoFormat::H264;

  Vector<uint8_t> expected_video;
  Vector<uint8_t> expected_audio;

  DynamicMemoryStream mux_buffer;
  mux_buffer.begin();

  MuxerMTS muxer(mux_buffer);
  MuxerVideoConfig cfg;
  cfg.width = 64;
  cfg.height = 48;
  cfg.fps = 25.0f;
  cfg.format = kVideoFormat;
  muxer.setVideoInfo(cfg);
  muxer.setAudioInfo(AudioInfoFormat(kSampleRate, kChannels, 16, AudioFormat::AAC));
  assert(muxer.begin());

  for (int i = 0; i < kVideoFrames; i++) {
    Vector<uint8_t> frame;
    if (useJpegApi) {
      // fake JPEG SOI/EOI so the payload at least looks image-shaped
      frame.push_back(0xFF);
      frame.push_back(0xD8);
    } else {
      // fake Annex-B NAL unit (start code + a made-up header byte)
      uint8_t hdr[4] = {0x00, 0x00, 0x00, 0x01};
      for (int j = 0; j < 4; j++) frame.push_back(hdr[j]);
    }
    // frame 2 is oversized to force multi-TS-packet chunking
    size_t payload = (i == 2) ? 10000 : 40;
    appendPattern(frame, payload, (uint8_t)(0x10 * i));

    size_t written = useJpegApi ? muxer.addJpegFrame(frame.data(), frame.size())
                                : muxer.addVideoFrame(frame.data(), frame.size(), i == 0);
    assert(written == (size_t)frame.size());
    for (int j = 0; j < frame.size(); j++) expected_video.push_back(frame[j]);

    if (i < kAudioFrames) {
      Vector<uint8_t> aframe;
      // ADTS header: AAC LC, 44100 Hz (index 4), 2 channels
      uint8_t ahdr[4] = {0xFF, 0xF1, 0x50, 0x80};
      for (int j = 0; j < 4; j++) aframe.push_back(ahdr[j]);
      appendPattern(aframe, 60, (uint8_t)(0x80 + i));

      size_t awritten = muxer.addAudioFrame(aframe.data(), aframe.size());
      assert(awritten == (size_t)aframe.size());
      for (int j = 0; j < aframe.size(); j++) expected_audio.push_back(aframe[j]);
    }
  }
  muxer.end();

  printf("Muxed transport stream (%s): %d bytes\n", useJpegApi ? "MJPEG" : "H264",
         (int)mux_buffer.size());
  assert(mux_buffer.size() > 0);
  assert(mux_buffer.size() % MTS_PACKET_SIZE == 0);

  // demux, feeding bytes back in small, awkward-sized chunks
  mux_buffer.rewind();
  DemuxerMTS demuxer;
  VerifyPrint video_out;
  VerifyPrint audio_out;
  demuxer.setOutputVideo(video_out);
  demuxer.setOutputAudio(audio_out);
  assert(demuxer.begin());

  const size_t kChunk = 37;  // deliberately not aligned to 188
  uint8_t buf[kChunk];
  int total_fed = 0;
  // DynamicMemoryStream::available() only reflects the current internal
  // record's length (one per write() call), not the true remaining byte
  // count, so it can under-report once a record has been partially
  // consumed; track the known total size instead.
  size_t remaining = (size_t)mux_buffer.size();
  while (remaining > 0) {
    size_t want = remaining < kChunk ? remaining : kChunk;
    size_t n = mux_buffer.readBytes(buf, want);
    if (n == 0) break;
    size_t written = demuxer.write(buf, n);
    assert(written == n);
    total_fed += (int)n;
    remaining -= n;
  }
  demuxer.end();
  printf("Fed %d bytes into DemuxerMTS\n", total_fed);

  bool ok = true;

  printf("Checking video ES round-trip...\n");
  if (!sameBytes(expected_video, video_out.data)) ok = false;

  printf("Checking audio ES round-trip...\n");
  if (!sameBytes(expected_audio, audio_out.data)) ok = false;

  printf("video flush() count: %d (expected %d)\n", video_out.flush_count, kVideoFrames);
  if (video_out.flush_count != kVideoFrames) ok = false;

  VideoInfo vi = demuxer.getVideoInfo();
  printf("parsed VideoInfo format: %d (expected %d)\n", (int)vi.format, (int)kVideoFormat);
  if (vi.format != kVideoFormat) ok = false;

  AudioInfoFormat ai = demuxer.getAudioInfo();
  printf("parsed AudioInfo: %d Hz, %d ch, format %d\n", (int)ai.sample_rate, (int)ai.channels,
         (int)ai.format);
  if (ai.sample_rate != kSampleRate || ai.channels != kChannels || ai.format != AudioFormat::AAC)
    ok = false;

  return ok;
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  bool ok_h264 = runRoundTrip(false);
  printf(ok_h264 ? "PASS: ContainerMTS H264 round-trip OK\n" : "FAIL: ContainerMTS H264 round-trip mismatch\n");

  bool ok_mjpeg = runRoundTrip(true);
  printf(ok_mjpeg ? "PASS: ContainerMTS MJPEG round-trip OK\n" : "FAIL: ContainerMTS MJPEG round-trip mismatch\n");

  bool ok = ok_h264 && ok_mjpeg;
  assert(ok);
  exit(ok ? 0 : 1);
}

void loop() {}
