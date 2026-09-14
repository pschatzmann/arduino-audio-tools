/*
 * Desktop RTSP test for combined audio+video streaming via a single MPEG-TS
 * (.mts/.ts) file.
 *
 * The RTSP redesign's IMediaSource/RTSPFormat only ever carries one m= line
 * (one SDP media type) per RTSPServer instance - there is no multi-track
 * SDP support yet. Streaming a pre-muxed MPEG-TS container as MP2T (RFC
 * 2250) sidesteps that entirely: the interleaved, PTS-synced A/V stream
 * travels as a single "m=video ... RTP/AVP 33" track that any RTSP client
 * already knows how to demux into its two streams, no RTSP/SDP changes
 * needed.
 *
 * Pipeline:
 *   sample.ts (real H.264 + AAC, see below) -> LoopingFileStream (streamed
 *   directly off disk, not preloaded into memory) -> RTSPMediaSource
 *   (RTSPFormatMTS, RTP/AVP 33 - MP2T needs no extra per-fragment header,
 *   just raw bytes chunked to a whole number of 188-byte TS packets, so
 *   the plain Stream-backed RTSPMediaSource is enough; no read callback
 *   needed) -> RTSPServer
 *
 * RTSPServer runs its streaming loop on its own background thread (see
 * RTSPServer.h/Task), not the thread that calls loop() - a separate
 * File::seek() driven from loop() would race the streaming thread's
 * concurrent reads on the same File object. LoopingFileStream avoids that
 * by doing the rewind inside its own read() (the same thread that does
 * the actual read).
 *
 * (AudioTools/Disk/FileLoop.h's FileLoop looks like the obvious existing
 * class for this, but its setFile()/constructor take File by value, which
 * doesn't work with the desktop emulator's File - it wraps a std::fstream
 * directly, which is non-copyable - so this test rolls its own instead.)
 *
 * sample.ts is not checked into the repo - generate a small one next to
 * this file with:
 *
 *   ffmpeg -f lavfi -i testsrc=size=320x240:rate=25:duration=5 \
 *          -f lavfi -i "sine=frequency=1000:duration=5" \
 *          -c:v libx264 -profile:v baseline -pix_fmt yuv420p -g 25 \
 *          -c:a aac -b:a 128k \
 *          -f mpegts sample.ts
 *
 * (a 5s 320x240 H.264 baseline + 1kHz-tone AAC test pattern, muxed to
 * MPEG-TS - swap testsrc/sine for -i <any real video/audio file> to use
 * real content instead).
 *
 * Connect with: ffprobe -rtsp_transport tcp rtsp://127.0.0.1:8554/video
 * and expect two streams to be reported (Video: h264, Audio: aac) from
 * the single RTSP session/URL.
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMTS.h"
#include "AudioTools/Communication/RTSP/RTSPPlatformWiFi.h"
#include "AudioTools/Communication/RTSP.h"
#include "SD.h"

using namespace audio_tools;

const int port = 8554;
const float kVideoFps = 25.0f;
const char *kSampleFile = "sample.ts";
// RFC 2250: an RTP/MP2T payload must hold a whole number of 188-byte TS
// packets; 7 is the common choice that stays under a typical 1500-byte MTU.
const int kTsPacketsPerRtpPacket = 7;
const int kFragmentSize = kTsPacketsPerRtpPacket * MTS_PACKET_SIZE;

/// Streams a File, seeking back to 0 once exhausted (dropping any trailing
/// partial TS packet, since the file's size need not be a multiple of
/// MTS_PACKET_SIZE). Only read()/available()/peek() are actually virtual on
/// Stream - Stream::readBytes() is a plain loop over read() - so
/// overriding just read() (not readBytes()) is enough for the loop-at-EOF
/// to apply correctly however the bulk read is made, and it keeps the
/// rewind on whichever thread is actually reading, never racing a rewind
/// against a read from another thread (as a separate File::seek() driven
/// from Arduino's loop() would, since RTSPServer streams on its own
/// background thread).
class LoopingFileStream : public Stream {
 public:
  bool begin(const char *path) {
    file = SD.open(path);
    return file;
  }

  size_t size() { return file.size(); }

  int available() override { return file.available(); }

  int read() override {
    if (file.available() == 0) file.seek(0);
    return file.read();
  }

  int peek() override { return file.peek(); }

  // Read-only stream - Print::write() is pure virtual on the base but
  // unused here.
  size_t write(uint8_t) override { return 0; }

 protected:
  File file;
};

// RTSPFormatMTS (RTSPFormat.h) implements RTP/AVP 33 (MP2T) per RFC 2250.
RTSPFormatMTS mtsFormat(kVideoFps);
LoopingFileStream mtsFile;
RTSPMediaSource mtsSource(mtsFile, mtsFormat);
RTSPMediaStreamer<RTSPPlatformWiFi> rtspStreamer(mtsSource);
RTSPServer<RTSPPlatformWiFi> rtspServer(rtspStreamer, port);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!mtsFile.begin(kSampleFile)) {
    Serial.print("Could not open ");
    Serial.print(kSampleFile);
    Serial.println(
        " - generate it first (see the command in this file's header "
        "comment)");
    exit(1);
  }
  Serial.print("Streaming ");
  Serial.print(kSampleFile);
  Serial.print(": ");
  Serial.print((int)mtsFile.size());
  Serial.println(" bytes, looping");

  mtsSource.setFragmentSize(kFragmentSize);

  rtspServer.begin();
}

void loop() {}
