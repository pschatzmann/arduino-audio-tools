/**
 * @file decode-mpg.ino
 * @brief Desktop counterpart of sd-mpg-audio-video.ino: plays both the
 * audio and video (MPEG-1) tracks of a local MPEG-1 Program Stream (.mpg)
 * file, demuxing it live with DemuxerMPG, decoding the video track with
 * MPGDecoder (TinyMPG, https://github.com/pschatzmann/TinyMPG - pure
 * software) and displaying the result live in a window via OutputOpenCV,
 * while playing the audio track (a raw MPEG-1 Layer I/II/III elementary
 * stream) through PortAudio via MP3DecoderHelix - the same choice
 * ContainerMPG.h's own file comment uses. Same pipeline as the embedded
 * version - only the two output classes (display, audio) are swapped for
 * their desktop equivalents: OutputTinyGPU -> OutputOpenCV, I2SStream ->
 * PortAudioStream. SD.h/File keep working unchanged - the Arduino-
 * Emulator's own SD.h shim maps them straight onto real filesystem access.
 *
 * The test file's own audio track is MPEG-1 Layer II (ffprobe: codec_name
 * mp2, 48kHz stereo), which MP3DecoderHelix's bundled libhelix MP3 backend
 * covers along with Layer III.
 *
 * Verified via a standalone DemuxerMPG parse of the real file: parsed
 * VideoInfo comes back as 176x144@23.98fps (matching the source), and
 * video sample dispatch grows steadily and without stalling across 80+ MB
 * fed (6788 samples, ~6.9MB of video ES).
 *
 * Pipeline: File -> EncodedAudioOutput (Print bridge) -> DemuxerMPG (demux)
 *   -> EncodedAudioStream (MP3DecoderHelix) -> PortAudioStream (audio)
 *   \-> MPGDecoder (MPEG-1 decode -> RGB565) -> OutputOpenCV (draw) (video)
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./decode-mpg
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputOpenCV.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144.mpg";

MPGDecoder mpgDecoder;
OutputOpenCV videoOut;
PortAudioStream out;
MP3DecoderHelix mp3Decoder;
EncodedAudioStream audioOut(&out, &mp3Decoder);  // decodes MP3/MP2 -> PortAudio

DemuxerMPG mpgDemuxer;
EncodedAudioOutput mpgInput(&mpgDemuxer);  // bridges raw file bytes -> DemuxerMPG::write()

File file;
StreamCopy copier(mpgInput, file);

// OutputOpenCV needs setSize() before it can display a raw (non-MJPEG)
// frame - unlike OutputTinyGPU/OutputTFT_eSPI it has no setVideoInfoSource()
// dynamic-binding, so this polls DemuxerMPG's parsed VideoInfo once per
// loop() and configures it as soon as the sequence_header has been seen.
bool video_size_set = false;
void configureVideoSizeOnceKnown() {
  if (video_size_set) return;
  VideoInfo vi = mpgDemuxer.getVideoInfo();
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

  mpgDecoder.setOutput(videoOut);
  mpgDecoder.begin();

  mpgDemuxer.setOutputAudio(audioOut);
  mpgDemuxer.setOutputVideo(mpgDecoder);
  mpgInput.begin();
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
