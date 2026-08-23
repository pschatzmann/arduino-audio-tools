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
 * Audio/video sync: DemuxerMPG dispatches audio/video as fast as bytes can
 * be parsed - all real pacing happens in videoSync (VideoAudioSyncTask,
 * see Video/VideoAudioSyncTask.h), which sits between the demuxer and
 * mpgDecoder. It buffers each decoded picture and renders it (decode +
 * cv::imshow) from its own background task, timed against audioClock - an
 * AudioTimeSourceStream inserted between multiDecoder and PortAudioStream
 * that turns "how many decoded PCM bytes have reached the audio device so
 * far" into an elapsed-ms clock, so video is paced to how far audio has
 * actually played rather than a separate wall-clock schedule. OutputOpenCV
 * pulls its picture size from DemuxerMPG via setVideoInfoSource(), same as
 * decode-mp4-file.ino/decode-mp4.ino.
 *
 * Pipeline: File -> CodecCopy -> DemuxerMPG (demux)
 *   -> EncodedAudioStream (DecoderHelix) -> AudioTimeSourceStream (audio
 *   clock) -> PortAudioStream (audio)
 *   \-> VideoAudioSyncTask (buffer + schedule) -> MPGDecoder (MPEG-1
 *   decode -> RGB565) -> OutputOpenCV (draw) (video, own background task)
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
// 3rd arg: scheduling delay compensating for PortAudioStream's own output
// buffering - see VideoAudioSyncTask::setSchedulingDelayMs(); tune to
// match your actual audio config (this uses PortAudioStream's defaults).
VideoAudioSyncTask videoSync(mpgDecoder, 0, 50);
PortAudioStream out;
AudioTimeSourceStream audioClock(out);  // decoded-PCM-bytes-based playback clock
MP2Decoder mp2Decoder;
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&audioClock, &multiDecoder);  // decodes MP3/AAC/WAV -> audioClock -> PortAudio

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

  // mpgDecoder.setIgnorePFrames(true);  // skip P-pictures, only decode I/B for speed
  mpgDecoder.setOutput(videoOut);
  mpgDecoder.begin();

  // OutputOpenCV needs a picture size before it can display a raw
  // (non-MJPEG) frame - pulls width/height from DemuxerMPG automatically
  // on every write() instead of polling for it in loop().
  videoOut.setVideoFormat(VideoFormat::RGB565);
  videoOut.setVideoInfoSource(mpgDemuxer);

  videoSync.setAudioClock(audioClock);

  mpgDemuxer.setOutputAudio(audioOut);
  mpgDemuxer.setOutputVideo(videoSync);
  mpgDemuxer.begin();
}

void loop() {
  // fps is only known once the video ES's sequence_header has streamed in
  // (see DemuxerMPG::getVideoInfo()) - cheap to just keep checking until
  // it's non-zero, same as the picture-size polling OutputOpenCV already
  // relies on via setVideoInfoSource().
  float fps = mpgDemuxer.getVideoInfo().fps;
  if (fps > 0) videoSync.setFps(fps);

  if (file && copier.copy()) {
    // no-op - OutputOpenCV pulls its size from mpgDemuxer via
    // setVideoInfoSource(), no manual polling needed here anymore
  } else {
    Serial.println("Done");
    file.close();
    exit(0);
  }
}
