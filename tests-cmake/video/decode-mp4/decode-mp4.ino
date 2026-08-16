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
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputOpenCV.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144-remuxed.mp4";

H264Decoder h264Decoder;
OutputOpenCV videoOut;
PortAudioStream out;
AACDecoderHelix aacDecoder;
EncodedAudioStream audioOut(&out, &aacDecoder);  // decodes AAC -> PortAudio

DemuxerMP4 mp4Demuxer;
EncodedAudioOutput mp4Input(&mp4Demuxer);  // bridges raw file bytes -> DemuxerMP4::write()

File file;
StreamCopy copier(mp4Input, file);

// OutputOpenCV needs setSize() before it can display a raw (non-MJPEG)
// frame - unlike OutputTinyGPU/OutputTFT_eSPI it has no setVideoInfoSource()
// dynamic-binding, so this polls DemuxerMP4's parsed VideoInfo once per
// loop() and configures it as soon as the video track's sample entry has
// been parsed.
bool video_size_set = false;
void configureVideoSizeOnceKnown() {
  if (video_size_set) return;
  VideoInfo vi = mp4Demuxer.getVideoInfo();
  if (vi.width > 0 && vi.height > 0) {
    videoOut.setVideoFormat(VideoFormat::RGB565);
    videoOut.setSize(vi.width, vi.height);
    video_size_set = true;
  }
}

void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  file = SD.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    exit(1);
  }

  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);
  audioOut.begin();

  h264Decoder.setOutput(videoOut);
  h264Decoder.setVideoFormat(VideoFormat::RGB565);
  h264Decoder.begin();

  mp4Demuxer.setOutputAudio(audioOut);
  mp4Demuxer.setOutputVideo(h264Decoder);
  mp4Input.begin();
}

void loop() {
  if (file && copier.copy()) {
    configureVideoSizeOnceKnown();
  } else {
    Serial.println("Done");
    file.close();
    exit(0);
  }
}
