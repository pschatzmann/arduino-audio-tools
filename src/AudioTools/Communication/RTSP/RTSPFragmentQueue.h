/*
 * Author: Phil Schatzmann
 */

#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"

namespace audio_tools {

/**
 * @brief Self-delimited fragment queue shared by packetized RTP video
 * encoders (JPEGRtpEncoder, H264RtpEncoder, ...).
 *
 * Fragments are appended one at a time via appendFragment(), already fully
 * built (payload-format header + data). They are consumed through
 * queuePacketSize()/queueReadBytes(), which subclasses expose as the
 * IMediaSource packetSize()/readBytes() overrides RTSPMediaStreamer expects
 * - see IMediaSource::packetSize() for the exact contract (0 = nothing
 * queued, RTSPMediaStreamer treats that as "the last fragment sent was the
 * last one of its frame").
 *
 * The queue is a flat byte buffer of messages laid out as
 * [uint32 payloadLen][uint8 isLast][payloadLen bytes]. The isLast byte is
 * only used internally, to know when a whole frame has been enqueued (see
 * m_queuedFrames / dropOldestFrame) so a slow consumer can be capped to a
 * bounded backlog; it is not exposed through packetSize().
 *
 * Framing the queue this way (rather than a Vector of per-fragment Vectors)
 * avoids relying on element-wise move/copy semantics that this codebase's
 * Vector<T> does not provide for non-trivial T (it grows via memmove).
 *
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFragmentQueue {
 protected:
  // RTSPMediaStreamer infers the RTP marker bit from "nothing left queued
  // after this fragment", which is only exact when at most one frame's
  // fragments are queued at a time - so keep the backlog capped at 1 frame
  // (older, stale frames are dropped rather than queued behind a newer one,
  // which also keeps real-time video latency low).
  size_t m_maxQueuedFrames = 1;

  Vector<uint8_t> m_queue;
  size_t m_queueHead = 0;
  size_t m_queuedFrames = 0;

  void clearQueue() {
    m_queue.clear();
    m_queueHead = 0;
    m_queuedFrames = 0;
  }

  uint32_t readQueueU32(size_t pos) {
    return ((uint32_t)m_queue[pos] << 24) | ((uint32_t)m_queue[pos + 1] << 16) |
           ((uint32_t)m_queue[pos + 2] << 8) | (uint32_t)m_queue[pos + 3];
  }

  void appendFragment(const uint8_t *payload, size_t len, bool last) {
    size_t pos = m_queue.size();
    m_queue.resize(pos + 5 + len);
    uint8_t *p = m_queue.data() + pos;
    p[0] = (uint8_t)((len >> 24) & 0xFF);
    p[1] = (uint8_t)((len >> 16) & 0xFF);
    p[2] = (uint8_t)((len >> 8) & 0xFF);
    p[3] = (uint8_t)(len & 0xFF);
    p[4] = last ? 1 : 0;
    memcpy(p + 5, payload, len);

    if (last) {
      ++m_queuedFrames;
      if (m_queuedFrames > m_maxQueuedFrames) {
        dropOldestFrame();
      }
    }
  }

  void dropOldestFrame() {
    size_t pos = m_queueHead;
    while (pos < m_queue.size()) {
      uint32_t len = readQueueU32(pos);
      bool last = m_queue[pos + 4] != 0;
      pos += 5 + len;
      if (last) break;
    }
    m_queueHead = pos;
    if (m_queuedFrames > 0) --m_queuedFrames;
    LOGW("RTSPFragmentQueue: consumer too slow, dropped oldest queued video frame");
  }

  int queuePacketSize() {
    if (m_queueHead >= m_queue.size()) {
      if (m_queue.size() > 0) clearQueue();
      return 0;
    }
    return (int)readQueueU32(m_queueHead);
  }

  /// Retrieves the next queued fragment. Call queuePacketSize() first and
  /// pass its result as maxBytes so exactly one fragment is read.
  int queueReadBytes(void *dest, int maxBytes) {
    if (m_queueHead >= m_queue.size()) return 0;

    uint32_t len = readQueueU32(m_queueHead);
    bool last = m_queue[m_queueHead + 4] != 0;
    size_t payloadStart = m_queueHead + 5;
    if (payloadStart + len > m_queue.size()) {
      LOGE("RTSPFragmentQueue: corrupt fragment queue; resetting");
      clearQueue();
      return 0;
    }

    m_queueHead = payloadStart + len;
    if (last && m_queuedFrames > 0) --m_queuedFrames;

    if ((int)len > maxBytes) {
      LOGE(
          "RTSPFragmentQueue: fragment (%u bytes) exceeds destination "
          "buffer (%d bytes); dropping",
          (unsigned)len, maxBytes);
      return 0;
    }
    memcpy(dest, m_queue.data() + payloadStart, len);
    return (int)len;
  }
};

}  // namespace audio_tools
