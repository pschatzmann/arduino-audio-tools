/**
 * @file decode-mp4-file.ino
 * @brief Same pipeline as decode-mp4.ino, but backs DemuxerMP4's sample
 * tables (stsz/stco/stsc/stts) with SourceSeekSampleTableStore instead of
 * the RAM-backed default - discards each parsed table entry right after
 * appending it, keeping only the file offset of the first one, and seeks
 * back into the original MP4 file to re-read entries on demand during
 * 'mdat' playback. Trades the ~4.5MB of RAM a feature-length movie's
 * tables would otherwise need for small repeated seeks - no separate
 * scratch storage needed at all (unlike decode-mp4-spooled.ino), since it
 * reads straight from the file that's already there.
 *
 * The tradeoff for that "no scratch storage" simplicity: the source has
 * to actually support seek() (a local File - this won't work against a
 * live, non-seekable stream like an HTTP response body, unlike spool
 * storage) and has to stay open for the entire lifetime of playback.
 *
 * Only one File is needed - FileSeekableSource<File, DemuxerMP4> owns it
 * and does double duty: its copy() method reads the file forward
 * sequentially and feeds DemuxerMP4::write() (replacing the
 * CodecCopy/StreamCopy other decode-*.ino examples use), while its
 * seek()/readBytes() (called by SourceSeekSampleTableStore during 'mdat'
 * playback) save the sequential position before jumping away and restore
 * it right after, so the two access patterns never disturb each other
 * despite sharing one file. Its constructor also wires itself into the
 * demuxer via writer.setSeekSource(*this), so no separate setSeekSource()
 * call is needed in setup().
 *
 * Pacing: DemuxerMP4 dispatches audio/video as fast as bytes can be parsed
 * - all real pacing happens in videoSync (PacedVideoOutput, see
 * Video/PacedVideoOutput.h), which sits between the demuxer and
 * h264Decoder. It buffers each decoded frame and renders it (decode +
 * cv::imshow) from its own background task. This test file has no audio
 * track (see decode-mp4.ino), so setAudioClock() is deliberately never
 * called; PacedVideoOutput falls back to wall-clock millis() in that
 * case (see its clockMs()), which is exactly what's needed here. (A real
 * audio track would instead be wired via an AudioTimeSourceStream between
 * multiDecoder and PortAudioStream, turning "how many decoded PCM bytes
 * have reached the audio device so far" into an elapsed-ms clock passed to
 * videoSync.setAudioClock() - see decode-avi.ino for that pattern.)
 *
 * Pipeline: File -> FileSeekableSource::copy() -> DemuxerMP4 (demux,
 *   seek-backed tables, reading from the same File) ->
 *   EncodedAudioStream (AACDecoderHelix) -> PortAudioStream (audio)
 *   \-> PacedVideoOutput (buffer + schedule against wall clock) ->
 *   H264Decoder (H.264 decode -> RGB565) -> OutputOpenCV (draw) (video, own
 *   background task)
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./decode-mp4-file
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputOpenCV.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144.mp4";

OutputOpenCV videoOut;
H264Decoder h264Decoder(videoOut);
// No audio clock (see the pacing note above) - scheduling delay stays 0.
PacedVideoOutput videoSync(h264Decoder);
PortAudioStream out;
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes AAC -> PortAudio
DemuxerMP4 mp4Demuxer;

// Single File, doing double duty: sequential forward feed (copy()) and
// the out-of-band seeks SourceSeekSampleTableStore performs (seek()/
// readBytes()) - see the class comment on FileSeekableSource for why one
// handle is safe here. Its constructor wires itself into mp4Demuxer via
// setSeekSource() automatically - no separate call needed in setup().
File file;
FileSeekableSource<File, DemuxerMP4> seekSource(file, mp4Demuxer);

void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  file = SD.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    exit(1);
  }

  /// Define audio mime
  multiDecoder.setMimeSource(mp4Demuxer);

  // OutputOpenCV needs a picture size before it can display a raw
  // (non-MJPEG) frame - pulls width/height from DemuxerMP4 automatically
  // on every write() instead of polling for it in loop().
  videoOut.setVideoFormat(VideoFormat::RGB565);
  videoOut.setVideoInfoSource(mp4Demuxer);

  // PortAudioStream
  auto audio_cfg = out.defaultConfig(TX_MODE);
  audio_cfg.buffer_size = 1024;
  audio_cfg.buffer_count = 10;
  out.begin(audio_cfg);

  // EncodedAudioStream
  audioOut.begin();

  // DemuxerMP4 - seek-backed sample tables instead of the RAM default
  mp4Demuxer.setOutputAudio(audioOut);
  mp4Demuxer.setOutputVideo(videoSync);
  mp4Demuxer.begin();
}

void loop() {
  // fps is only known once 'stts' has been parsed (part of 'moov', so
  // strictly before any 'mdat' sample is dispatched) - see
  // DemuxerMP4::getVideoInfo().
  float fps = mp4Demuxer.getVideoInfo().fps;
  if (fps > 0) videoSync.setFps(fps);

  if (file && seekSource.copy()) {
  } else {
    Serial.println("Done");
    file.close();
    exit(0);
  }
}
