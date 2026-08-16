/**
 * @file decode-avi.ino
 * @brief Desktop counterpart of sd-avi-audio-video.ino: plays both the
 * audio and video tracks of a local .avi file, demuxing it live with
 * DemuxerAVI and displaying the result live in a window via OutputOpenCV,
 * while playing the audio track through PortAudio. Same pipeline shape as
 * the embedded version - only the two output classes (display, audio) are
 * swapped for their desktop equivalents: OutputTinyGPU -> OutputOpenCV,
 * I2SStream -> PortAudioStream. SD.h/File keep working unchanged - the
 * Arduino-Emulator's own SD.h shim maps them straight onto real filesystem
 * access.
 *
 * Unlike sd-avi-audio-video.ino this decodes MJPEG, not H.264: a plain
 * `ffmpeg -vf scale=... in.mkv out.avi` (no explicit -c:v) defaults to
 * MPEG-4 Part 2 for AVI, which neither H264Decoder nor any other codec in
 * this project decodes - `ffmpeg ... -c:v mjpeg out.avi` is the AVI+codec
 * pairing this project can actually decode without an extra decoder step:
 * OutputOpenCV's default mode already decodes MJPEG itself (cv::imdecode),
 * the same way the older container-avi-movie.cpp in this same tests-cmake
 * tree does. See sd-avi-video.ino's own comment for how to produce an
 * H.264-in-AVI file instead, if you want to exercise H264Decoder here too
 * (swap videoOut for an H264Decoder->OutputOpenCV(RGB565) pair, the same
 * way decode-mp4.ino does).
 *
 * AVI's audio format varies per file (PCM/WAV, AAC, MP3 are all valid
 * 'strf' choices), so this uses DecoderHelix - a multi-format decoder that
 * auto-detects which one it's looking at - rather than assuming a specific
 * codec. The test file's own 'strf' reports audio format tag 0x55 (MP3),
 * which DecoderHelix's bundled MP3DecoderHelix picks up automatically.
 *
 * Verified via a standalone DemuxerAVI parse of the real file: header
 * parses correctly (176x144, MJPG, MP3/48kHz/stereo) and both tracks
 * dispatch continuously once fed past the ~10KB header (17 video + 31
 * audio calls in the first ~20KB alone) - no stalls, no parse errors.
 *

 * Pipeline: File -> EncodedAudioOutput (Print bridge) -> DemuxerAVI (demux)
 *   -> EncodedAudioStream (DecoderHelix: WAV/AAC/MP3) -> PortAudioStream (audio)
 *   \-> OutputOpenCV (MJPEG decode + draw) (video)
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./decode-avi
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/Video/OutputOpenCV.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144-mjpeg.avi";

OutputOpenCV videoOut;  // default mode is MJPEG - decodes the JPEG itself
PortAudioStream out;
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes PCM/AAC/MP3 -> PortAudio

DemuxerAVI aviDemuxer;
EncodedAudioOutput aviInput(&aviDemuxer);  // bridges raw file bytes -> DemuxerAVI::write()

File file;
StreamCopy copier(aviInput, file);

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
  multiDecoder.begin();

  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setOutputVideo(videoOut);
  aviInput.begin();
}

void loop() {
  if (file && copier.copy()) {
    // MJPEG frames are self-describing (JPEG header carries width/height),
    // so - unlike decode-mp4.ino/decode-mpg.ino - there's no setSize() to
    // poll for here.
  } else {
    Serial.println("Done");
    file.close();
    exit(0);
  }
}
