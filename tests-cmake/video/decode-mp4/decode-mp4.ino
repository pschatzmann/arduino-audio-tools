/**
 * @file decode-mp4.ino
 * @brief Desktop counterpart of sd-mp4-audio-video.ino: plays the video
 * (H.264) track of a local .mp4 file, demuxing it live with DemuxerMP4,
 * decoding it with H264Decoder (TinyH264,
 * https://github.com/pschatzmann/TinyH264 - pure software) and displaying
 * the result live in a window via OutputOpenCV, while playing any audio
 * track through PortAudio (AACDecoderHelix - the source file used below
 * has no audio track, so that side just stays idle; point file_path at a
 * file with an AAC track to exercise it). Same pipeline as the embedded
 * version - only the two output classes (display, audio) are swapped for
 * their desktop equivalents: OutputTinyGPU -> OutputOpenCV, I2SStream ->
 * PortAudioStream. SD.h/File keep working unchanged - the Arduino-
 * Emulator's own SD.h shim maps them straight onto real filesystem access.
 *
 * Same "faststart" MP4 (moov before mdat) DemuxerMP4 requires everywhere
 * else, e.g.:
 *   ffmpeg -i in.mp4 -c:v libx264 -c:a aac -movflags +faststart out.mp4
 *
 * The file this points at (output176x144.mp4, from the same directory as
 * decode-avi.ino/decode-mpg.ino's test files) is actually a *raw* H.264
 * Annex-B elementary stream, not a real MP4 container - its own generating
 * command uses `-f h264`, which overrides the container despite the .mp4
 * name (confirmed via `ffprobe`: format_name=h264, "raw H.264 video"), so
 * DemuxerMP4 can't parse it directly. It was remuxed once into a real MP4
 * to produce output176x144-remuxed.mp4 (verified via a standalone
 * DemuxerMP4 parse: 9660 video samples dispatched, correct 176x144, no
 * stalls):
 *   ffmpeg -i output176x144.mp4 -c copy -movflags +faststart output176x144-remuxed.mp4
 *
 * Pipeline: File -> EncodedAudioOutput (Print bridge) -> DemuxerMP4 (demux)
 *   -> EncodedAudioStream (AACDecoderHelix) -> PortAudioStream (audio)
 *   \-> H264Decoder (H.264 decode -> RGB565) -> OutputOpenCV (draw) (video)
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./decode-mp4
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
PortAudioStream out;
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes AAC -> PortAudio
DemuxerMP4 mp4Demuxer;
File file;
CodecCopy copier(mp4Demuxer, file);

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

  // DemuxerMP4
  mp4Demuxer.setOutputAudio(audioOut);
  mp4Demuxer.setOutputVideo(h264Decoder);
  mp4Demuxer.begin();
}

void loop() {
  if (file && copier.copy()) {
    // no-op - OutputOpenCV pulls its size from mp4Demuxer via
    // setVideoInfoSource(), no manual polling needed here anymore
  } else {
    Serial.println("Done");
    file.close();
    exit(0);
  }
}
