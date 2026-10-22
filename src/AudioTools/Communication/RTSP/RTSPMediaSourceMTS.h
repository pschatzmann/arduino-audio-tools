#pragma once

#include "AudioTools/Concurrency/LockFree.h"
#include "IMediaSource.h"
#include "RTSPFormat.h"

namespace audio_tools {

/**
 * @brief IMediaSource for an MPEG transport stream, e.g. live video and audio
 * muxed by MuxerMTS: the muxer writes its output to this object (it is a
 * Print), and an RTSPMediaStreamer sends it as one RTP track (MP2T, RFC
 * 2250 - see RTSPFormatMTS) to the RTSP client, which demuxes video and audio
 * itself.
 *
 * Unlike RTSPOutput's generic byte stream, this source is meant for a live
 * stream written from other tasks (e.g. by VideoMuxerWithTasks):
 *  - the data is kept as complete 188 byte TS packets in a fixed size,
 *    lock-free single producer/single consumer ring buffer
 *    (RingBufferSPSC), and every RTP packet contains whole TS packets (up
 *    to 7: 1316 bytes, below the usual MTU);
 *  - it is a packetized source (packetSize()), so the streamer sends all
 *    queued packets at each timer tick instead of a fixed amount: the send
 *    rate adapts to the bitrate of the stream;
 *  - while no client is streaming, the data is dropped, so a new client
 *    doesn't get old data;
 *  - if the network is slower than the stream, video is dropped rather
 *    than audio: when the buffer is filled to 3/4, the video packets are
 *    dropped from the start of the next video frame on, until the buffer is
 *    half empty again and the next keyframe starts (MuxerMTS writes the
 *    PAT before each keyframe). So the picture freezes for a moment while
 *    the sound continues. Audio and the other packets are only dropped if
 *    the buffer is completely full. Audio and video are recognized by the
 *    stream id of their PES packets, so this works with any transport
 *    stream. See droppedPackets() and droppedVideoPackets().
 * A client starts decoding at the next keyframe: MuxerMTS repeats the
 * stream description (PAT/PMT) before each one.
 *
 * The data can be written from any task, but not from several tasks at the
 * same time - e.g. by a MuxerMTS driven by VideoMuxerWithTasks, which
 * serializes the writes of its audio and video task with its mutex.
 *
 * @ingroup rtsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RTSPMediaSourceMTS : public IMediaSource, public Print {
 public:
  static constexpr int kPacketSize = 188;
  static constexpr int kPacketsPerFragment = 7;

  /// @param bufferPackets size of the buffer in TS packets
  /// @param sendRate number of times per second the queued packets are sent
  RTSPMediaSourceMTS(int bufferPackets = 128, float sendRate = 100)
      : buffer(bufferPackets * kPacketSize) {
    capacity = buffer.availableForWrite();
    format.setFramerate(sendRate);
    format.setFragmentSize(kPacketsPerFragment * kPacketSize);
  }

  RTSPFormat &getFormat() override { return format; }

  /// Called when a client starts streaming, before the first readBytes():
  /// discards the old data (on the consumer side - reset() would not be
  /// safe while the producer is writing)
  void start() override {
    uint8_t tmp[kPacketSize];
    while (buffer.readArray(tmp, kPacketSize) > 0) {
    }
    is_active = true;
  }

  /// Called when the client stops streaming
  void stop() override { is_active = false; }

  size_t write(uint8_t c) override { return write(&c, 1); }

  /// Accepts the transport stream, in any chunk sizes
  size_t write(const uint8_t *data, size_t len) override {
    size_t pos = 0;
    while (pos < len) {
      size_t n = kPacketSize - packet_len;
      if (n > len - pos) n = len - pos;
      memcpy(packet + packet_len, data + pos, n);
      packet_len += n;
      pos += n;
      if (packet_len == kPacketSize) {
        addPacket();
        packet_len = 0;
      }
    }
    return len;
  }

  /// Size of the next RTP payload: up to kPacketsPerFragment TS packets
  int packetSize() override {
    int packets = buffer.available() / kPacketSize;
    if (packets > kPacketsPerFragment) packets = kPacketsPerFragment;
    return packets * kPacketSize;
  }

  /// Provides whole TS packets only
  int readBytes(void *dest, int maxBytes) override {
    int len = buffer.available() < maxBytes ? buffer.available() : maxBytes;
    len = len / kPacketSize * kPacketSize;
    return buffer.readArray((uint8_t *)dest, len);
  }

  /// Number of TS packets dropped because the buffer was full
  uint32_t droppedPackets() { return dropped; }

  /// Number of video TS packets dropped to keep the audio going
  uint32_t droppedVideoPackets() { return dropped_video; }

 protected:
  RTSPFormatMTS format;
  RingBufferSPSC<uint8_t> buffer;
  uint8_t packet[kPacketSize];
  size_t packet_len = 0;
  volatile bool is_active = false;
  int capacity = 0;
  uint32_t dropped = 0;
  uint32_t dropped_video = 0;
  int video_pid = -1;  // learned from the PES stream ids
  bool dropping_video = false;

  void addPacket() {
    if (!is_active) return;  // no client is streaming
    int pid = ((packet[1] & 0x1F) << 8) | packet[2];
    bool pusi = packet[1] & 0x40;
    if (pusi && isVideoPes()) video_pid = pid;
    bool is_video = pid == video_pid;
    int free = buffer.availableForWrite();

    // too slow: drop video from the start of the next frame on, until the
    // buffer has recovered and the next keyframe (preceded by the PAT)
    if (dropping_video) {
      if (pid == 0 && free >= capacity / 2) {
        dropping_video = false;
        LOGI("RTSPMediaSourceMTS: resuming video at the next keyframe");
      }
    } else if (is_video && pusi && free < capacity / 4) {
      dropping_video = true;
      LOGW("RTSPMediaSourceMTS: network too slow - dropping video");
    }
    if (dropping_video && is_video) {
      dropped_video++;
      return;
    }

    // only whole packets: the free space can only grow while we write
    if (free < kPacketSize) {
      if (dropped++ % 100 == 0) {
        LOGW("RTSPMediaSourceMTS: buffer full, %u packets dropped",
             (unsigned)dropped);
      }
      return;
    }
    buffer.writeArray(packet, kPacketSize);
  }

  /// True if the current packet starts a video PES packet (stream id
  /// 0xE0-0xEF)
  bool isVideoPes() {
    int afc = (packet[3] >> 4) & 3;
    if (!(afc & 1)) return false;  // no payload
    int pos = (afc & 2) ? 5 + packet[4] : 4;
    if (pos + 4 > kPacketSize) return false;
    return packet[pos] == 0 && packet[pos + 1] == 0 && packet[pos + 2] == 1 &&
           (packet[pos + 3] & 0xF0) == 0xE0;
  }
};

}  // namespace audio_tools
