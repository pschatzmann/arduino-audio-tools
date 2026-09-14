/**
 * @file decode-mp4-spooled.ino
 * @brief Same pipeline as decode-mp4.ino, but backs DemuxerMP4's sample
 * tables (stsz/stco/stsc/stts) with SpoolFileSampleTableStore instead of
 * the RAM-backed default - trades the ~4.5MB of RAM a feature-length
 * movie's tables would otherwise need for real disk I/O against small
 * scratch files written as 'moov' streams past, then read back on demand
 * during 'mdat' playback.
 *
 * Unlike SourceSeekSampleTableStore (which re-reads entries directly from
 * the original MP4 file and therefore needs it to stay seekable), spool
 * storage writes its own private copy, so it also works with sources that
 * aren't seekable at all (e.g. a live HTTP-streamed MP4) - not exercised
 * here since this example still reads from a local File, but the demuxer
 * side doesn't care either way.
 *
 * SpoolStorageFactory exists because DemuxerMP4 doesn't know up front how
 * many tracks a file has, or which are video vs. audio - up to two tracks
 * (video + audio) times four tables means up to eight distinct spool
 * files are needed, each opened lazily as DemuxerMP4 discovers a track's
 * kind (once 'hdlr' has been parsed, not at 'trak' time).
 *
 * Verified against the real test file via a standalone diagnostic run:
 * every spooled table's byte count matched the known-correct entry
 * counts exactly (147559 video samples/chunks, 288495 audio samples,
 * 147560 audio chunks, 13259 stsc entries) - see SampleTableStore.h for
 * the storage strategies themselves.
 *
 * Pacing: DemuxerMP4 dispatches video as fast as bytes can be parsed - with
 * no decoder in between to pace it, frames would decode/display as fast as
 * the CPU allows instead of at the file's real frame rate. videoSync
 * (PacedVideoOutput, see Video/PacedVideoOutput.h) sits between the
 * demuxer and h264Decoder to fix that. It's never given an audio clock
 * (setAudioClock() not called) since this test file has no audio track to
 * derive one from - see PacedVideoOutput::clockMs(), it falls back to
 * wall-clock millis() in that case, which is exactly what's needed here.
 *
 * Pipeline: File -> EncodedAudioOutput (Print bridge) -> DemuxerMP4 (demux,
 *   spool-backed tables) -> EncodedAudioStream (AACDecoderHelix) ->
 *   PortAudioStream (audio)
 *   \-> PacedVideoOutput (buffer + schedule against wall clock) ->
 *   H264Decoder (H.264 decode -> RGB565) -> OutputOpenCV (draw) (video, own
 *   background task)
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./decode-mp4-spooled
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
File file;
CodecCopy copier(mp4Demuxer, file);

// ---- Spool storage: one real scratch file per table per track kind,
// opened lazily as DemuxerMP4 discovers each track's kind. Up to 8 files
// total (2 tracks x 4 tables) for this file (1 video + 1 audio track). ----
class LocalSpoolFactory : public SpoolStorageFactory {
 public:
  SpoolStorage *createSpoolStorage(TrackKind trackKind,
                                    SampleTableKind tableKind) override {
    char path[64];
    const char *kindStr = trackKind == TrackKind::Video ? "video" : "audio";
    const char *tableStr = "unknown";
    switch (tableKind) {
      case SampleTableKind::SampleSizes: tableStr = "sizes"; break;
      case SampleTableKind::ChunkOffsets: tableStr = "offsets"; break;
      case SampleTableKind::Stsc: tableStr = "stsc"; break;
      case SampleTableKind::Stts: tableStr = "stts"; break;
    }
    snprintf(path, sizeof(path), "mp4spool-%s-%s.bin", kindStr, tableStr);
    File &f = spool_files[count++];
    f = SD.open(path, O_RDWR | O_CREAT);
    if (!f) {
      Serial.print("Could not open spool file ");
      Serial.println(path);
      exit(1);
    }
    Serial.print("Spool file opened: ");
    Serial.println(path);
    stores[count - 1] = new FileSpoolStorage<File>(f);
    return stores[count - 1];
  }

 protected:
  File spool_files[8];
  FileSpoolStorage<File> *stores[8];
  int count = 0;
};

LocalSpoolFactory spoolFactory;

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

  // DemuxerMP4 - spool-backed sample tables instead of the RAM default
  mp4Demuxer.setSpoolStorageFactory(spoolFactory);
  mp4Demuxer.setOutputAudio(audioOut);
  mp4Demuxer.setOutputVideo(videoSync);
  mp4Demuxer.begin();
}

void loop() {
  // fps is only known once 'moov' has been parsed (strictly before any
  // 'mdat' sample is dispatched) - see DemuxerMP4::getVideoInfo().
  float fps = mp4Demuxer.getVideoInfo().fps;
  if (fps > 0) videoSync.setFps(fps);

  if (file && copier.copy()) {
    // no-op - OutputOpenCV pulls its size from mp4Demuxer via
    // setVideoInfoSource(), no manual polling needed here anymore
  } else {
    Serial.println("Done");
    file.close();
    exit(0);
  }
}
