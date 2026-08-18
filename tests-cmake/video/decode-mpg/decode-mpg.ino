/**
 * @file decode-mpg.ino
 * @brief Desktop counterpart of sd-mpg-audio-video.ino: plays a local MPEG-1
 * Program Stream (.mpg) file - video via DemuxerMPG -> MPGDecoder (TinyMPG)
 * -> OutputOpenCV, audio via DecoderHelix (auto-selects MP3/AAC/WAV from
 * DemuxerMPG's mime()) -> PortAudioStream. A dedicated MP2Decoder (TinyMP2)
 * is registered on top for Layer II specifically - see DemuxerMPG::mime(),
 * which reports Layer II with an explicit codecs parameter so it resolves
 * to this decoder instead of Helix's MP3 decoder. Same pipeline as the
 * embedded version, with OutputTinyGPU/I2SStream swapped for their desktop
 * equivalents.
 *
 * File feeding uses CodecCopy, like decode-avi.ino/decode-mp4-file.ino -
 * DemuxerMPG::write() may only accept part of a buffer per call, and
 * (unlike StreamCopy) CodecCopy retries the remainder instead of dropping
 * it, which would desync the demuxer from the real pack/PES byte stream.
 *
 * DemuxerMPG defaults to VideoAudioBufferedSync (same as DemuxerMP4) to
 * pace audio/video against each PES packet's PTS instead of dispatching as
 * fast as bytes can be parsed. OutputOpenCV pulls its picture size from
 * DemuxerMPG via setVideoInfoSource(), same as decode-mp4-file.ino/
 * decode-mp4.ino.
 *
 * Pipeline: File -> CodecCopy -> DemuxerMPG (demux, paced)
 *   -> EncodedAudioStream (DecoderHelix) -> PortAudioStream (audio)
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
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/AudioCodecs/CodecMP2.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputOpenCV.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144-mp3.mpg";

MPGDecoder mpgDecoder;
OutputOpenCV videoOut;
PortAudioStream out;
MP2Decoder mp2Decoder;
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes MP3/AAC/WAV -> PortAudio

DemuxerMPG mpgDemuxer;
File file;
CodecCopy copier(mpgDemuxer, file);

void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // add support for mp2 audio (dedicated TinyMP2 decoder, registered
  // under the specific codecs-parameterized mime DemuxerMPG reports for
  // Layer II - see DemuxerMPG::mime())
  multiDecoder.addDecoder(mp2Decoder, "audio/mpeg; codecs=\"mpeg1-layer2\"");

  file = SD.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    exit(1);
  }

  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);

  /// Define audio mime
  multiDecoder.setMimeSource(mpgDemuxer);
  audioOut.begin();

  mpgDecoder.setOutput(videoOut);
  mpgDecoder.begin();

  // OutputOpenCV needs a picture size before it can display a raw
  // (non-MJPEG) frame - pulls width/height from DemuxerMPG automatically
  // on every write() instead of polling for it in loop().
  videoOut.setVideoFormat(VideoFormat::RGB565);
  videoOut.setVideoInfoSource(mpgDemuxer);

  mpgDemuxer.setOutputAudio(audioOut);
  mpgDemuxer.setOutputVideo(mpgDecoder);
  mpgDemuxer.begin();
}

void loop() {
  if (file && copier.copy()) {
    // no-op - OutputOpenCV pulls its size from mpgDemuxer via
    // setVideoInfoSource(), no manual polling needed here anymore
  } else {
    Serial.println("Done");
    file.close();
    exit(0);
  }
}
