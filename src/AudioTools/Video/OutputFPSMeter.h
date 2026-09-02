#pragma once
#include "AudioTools/Video/VideoOutput.h"

namespace audio_tools {

/**
 * @brief Wraps a VideoOutput (typically a codec decoder, e.g. H264Decoder/
 * MJPEGDecoder/MPGDecoder or a MultiVideoDecoder) and measures how long
 * decoding+outputting each frame actually takes - forwards every write()/
 * flush()/setSkipRender()/isKeyFrame() call unmodified to the wrapped
 * target, but times write()+flush() together and classifies the result
 * I- vs P-frame via the target's own isKeyFrame(), the same way
 * PacedVideoOutput classifies frames for its own frameCountI()/
 * frameCountP() stats.
 *
 * Timed and finalized at flush() time, not write() - deliberately, per
 * the VideoOutput contract's own "one or more write() calls, then
 * finalized with flush()" pattern: H264Decoder/MPGDecoder do decode+
 * output entirely within write() (their own flush() is a no-op, so
 * timing/tallying at flush() still captures the real cost correctly),
 * but MJPEGDecoder only *accumulates* bytes in write() (a cheap memcpy)
 * and does the actual decode+output on flush(). A version of this class
 * that finalized at write() time (an earlier bug here) would time
 * MJPEGDecoder's cheap accumulate call and never see its real decode
 * cost at all - reporting a near-zero, meaningless "avg ms" and a wildly
 * inflated frame count/fps, since VideoOutput::hadOutput() defaults true
 * for every call regardless of whether real work happened.
 *
 * Important: this measures the *whole* time inside the wrapped target's
 * write()+flush() calls, not decode alone. Every current decoder
 * (H264Decoder/MJPEGDecoder/MPGDecoder) pushes each decoded picture to
 * its own configured output synchronously, from within write() or
 * flush() (see above) - and every current VideoOutput display driver
 * (e.g. OutputTinyGPU, whose TinyGPU bus drivers busy-wait for their DMA
 * transfer to finish before returning) blocks until the pixels have
 * actually reached the panel. So if the decoder you wrap here has a real
 * display wired as its own output, the numbers below already include
 * that display's render/SPI cost - they are NOT decode-only unless the
 * decoder's own output is a no-op sink (see NullVideoOutput in
 * sd-measure-fps.ino) instead of a real display.
 *
 * Only tallies a write()+flush() pair where the target's hadOutput() is
 * true after flush() (a decoder like MPGDecoder can legitimately swallow
 * a call into B-picture reordering without emitting a picture that same
 * call - see VideoOutput::hadOutput()'s own comment), so the stats
 * reflect actually-decoded pictures, not raw access-unit count.
 *
 * Drop this in front of a decoder with no pacing above it (i.e. no
 * PacedVideoOutput - feed the demuxer's setOutputVideo() straight into a
 * OutputFPSMeter wrapping the decoder) and drive it with plain CodecCopy
 * to answer "how fast could this hardware actually play this content if
 * nothing were pacing it" - exactly the ceiling PacedVideoOutput itself
 * needs to stay under (see its own inputFPS()/outputFPS()/avgFrameMs()
 * diagnostics, and the "Audio/Video Synchronization" wiki chapter) to
 * avoid ever falling behind and dropping/resyncing during real playback.
 * See sd-measure-fps.ino for the full pattern - it wires a real display
 * as the default target (so the measurement reflects real achievable
 * playback fps), with a no-op VideoOutput sink as a drop-in alternative
 * to isolate pure codec throughput from display cost instead.
 *
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OutputFPSMeter : public VideoOutput {
 public:
  /// @param target every write()/flush() call is forwarded here,
  /// unmodified - its own isKeyFrame()/hadOutput() answers drive this
  /// meter's I/P classification and which write()+flush() pairs actually
  /// count. Must outlive this object.
  OutputFPSMeter(VideoOutput &target) : p_target(&target) {}

  size_t write(const uint8_t *data, size_t len) override {
    if (len == 0) return 0;
    // Cached for flush() to finalize with - some decoders (MJPEGDecoder)
    // only decode on flush(), by which point the raw encoded bytes
    // isKeyFrame() needs are already gone. OR'd across however many
    // write() calls precede the next flush(), in case a keyframe marker
    // isn't in the very first chunk.
    pending_is_key = pending_is_key || p_target->isKeyFrame(data, len);
    have_pending = true;
    uint32_t t0 = millis();
    size_t result = p_target->write(data, len);
    call_ms += millis() - t0;
    return result;
  }

  void flush() override {
    uint32_t t0 = millis();
    p_target->flush();
    call_ms += millis() - t0;
    if (have_pending && p_target->hadOutput()) {
      if (frame_count == 0) start_ms = t0;
      last_ms = millis();
      frame_count++;
      total_ms += call_ms;
      if (pending_is_key) {
        i_count++;
        i_ms += call_ms;
      } else {
        p_count++;
        p_ms += call_ms;
      }
    }
    call_ms = 0;
    have_pending = false;
    pending_is_key = false;
  }

  void setSkipRender(bool skip) override { p_target->setSkipRender(skip); }
  bool isKeyFrame(const uint8_t *data, size_t len) override {
    return p_target->isKeyFrame(data, len);
  }

  /// Total number of frames actually decoded so far - handy for a
  /// progress line while a long run is still going.
  uint32_t frameCount() const { return frame_count; }
  /// Number of those classified as keyframes (I-frames).
  uint32_t frameCountI() const { return i_count; }
  /// Number of those classified as non-keyframes (P-frames).
  uint32_t frameCountP() const { return p_count; }

  /// Prints the measurement so far: overall fps actually achieved
  /// (frames decoded / wall-clock time spent inside the wrapped target's
  /// write()+flush() calls since the first one), plus the avg-ms/
  /// implied-fps breakdown per frame type, mirroring PacedVideoOutput::
  /// logTo()'s own style. Safe to call mid-run (e.g. for a periodic
  /// progress line) or once at the end.
  ///
  /// Labeled "write ms", not "decode ms" - see this class's own comment:
  /// unless the wrapped target's own output is a no-op sink, these times
  /// already include that target's real render/display cost, not just
  /// decode.
  void logTo(Print &out) {
    uint32_t elapsed = last_ms - start_ms;
    out.print("frames decoded: ");
    out.print((int)frame_count);
    out.print(" (I: ");
    out.print((int)i_count);
    out.print(" / P: ");
    out.print((int)p_count);
    out.println(")");
    out.print("elapsed: ");
    out.print((int)elapsed);
    out.println(" ms");
    out.print("avg fps (measured, full speed): ");
    out.println(elapsed > 0 ? 1000.0f * frame_count / elapsed : 0.0f);
    out.print("avg write ms (decode + whatever the target's own output "
               "does) - I: ");
    out.print(i_count > 0 ? (float)i_ms / i_count : 0.0f);
    out.print(" / P: ");
    out.print(p_count > 0 ? (float)p_ms / p_count : 0.0f);
    out.print(" / overall: ");
    out.println(frame_count > 0 ? (float)total_ms / frame_count : 0.0f);
    out.print("implied max fps - I: ");
    out.print(i_ms > 0 ? 1000.0f * i_count / i_ms : 0.0f);
    out.print(" / P: ");
    out.print(p_ms > 0 ? 1000.0f * p_count / p_ms : 0.0f);
    out.print(" / overall: ");
    out.println(total_ms > 0 ? 1000.0f * frame_count / total_ms : 0.0f);
    // Only nonzero for a target that tracks decode time separately from
    // everything after it (currently H264Decoder) - see VideoOutput::
    // totalDecodeMs()'s own comment. This is the one number here that IS
    // decode-only, regardless of what the target's own output does.
    uint64_t decodeMs = p_target->totalDecodeMs();
    if (decodeMs > 0 && frame_count > 0) {
      float avgDecodeMs = (float)decodeMs / frame_count;
      out.print("of which pure decode ms - avg: ");
      out.print(avgDecodeMs);
      out.print(" / implied max fps: ");
      out.println(1000.0f * frame_count / decodeMs);
      // Everything write() did minus the decode-only share above -
      // convert/render/SPI, i.e. exactly the display cost this class's
      // own comment warns is otherwise baked into "avg write ms" above.
      out.print("of which convert+render ms - avg: ");
      out.println(((float)total_ms / frame_count) - avgDecodeMs);
    }
  }

 protected:
  VideoOutput *p_target;
  // write()-then-flush() pending state - see flush()'s own comment for
  // why finalization happens there instead of in write().
  bool have_pending = false;
  bool pending_is_key = false;
  uint32_t call_ms = 0;
  uint32_t frame_count = 0, i_count = 0, p_count = 0;
  uint32_t total_ms = 0, i_ms = 0, p_ms = 0;
  uint32_t start_ms = 0, last_ms = 0;
};

}  // namespace audio_tools
