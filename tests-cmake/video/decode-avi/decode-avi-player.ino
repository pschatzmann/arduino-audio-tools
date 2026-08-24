/**
 * @file decode-avi-player.ino
 * @brief Same pipeline/test file as decode-avi.ino, but driven through
 * AudioTools/Video/VideoPlayer.h instead of wiring CodecCopy,
 * MultiVideoDecoder, DecoderHelix and VideoAudioSyncTask together by hand -
 * see decode-avi.ino's own class comment for the pipeline this replaces
 * (identical end to end; VideoPlayer just owns the wiring).
 *
 * This AVI file has a real audio track, so setUseAudioClock(true) is used
 * to schedule video against actual playback progress instead of the
 * wall-clock default - see VideoPlayer's own class comment ("Audio
 * clock") for why that's opt-in rather than automatic.
 *
 * VideoPlayer's video AND audio multi-decoders both start empty (no
 * forced dependency on any one codec library - see its own class
 * comment), so the codecs this AVI file's tracks may use (H.264/MJPEG
 * video, MP3/AAC/WAV audio - the same trio DecoderHelix bundles by
 * default) are registered here explicitly via addVideoDecoder()/
 * addAudioDecoder(). VideoPlayerFull (see decode-avi-player-full.ino)
 * does this same registration internally, if you don't want to repeat it
 * per sketch.
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./decode-avi-player
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/CodecJPEG.h"
#include "AudioTools/Video/OutputOpenCV.h"
#include "AudioTools/Video/VideoPlayer.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144.avi";

OutputOpenCV videoOut;  // default mode is MJPEG - decodes the JPEG itself
PortAudioStream out;
DemuxerAVI aviDemuxer;
VideoPlayer player(aviDemuxer, videoOut, out);
H264Decoder h264Decoder;
MJPEGDecoder mjpegDecoder;
MP3DecoderHelix mp3Decoder;
AACDecoderHelix aacDecoder;
WAVDecoder wavDecoder;
File file;

void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  file = SD.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    exit(1);
  }

  auto audio_cfg = out.defaultConfig(TX_MODE);
  audio_cfg.buffer_size = 1024;
  audio_cfg.buffer_count = 10;
  out.begin(audio_cfg);

  // Both H264Decoder and MJPEGDecoder (the codecs MultiVideoDecoder, used
  // internally by VideoPlayer, auto-selects between) output already-
  // decoded RGB565 frames, not MJPEG bytes - OutputOpenCV defaults to
  // MJPEG mode (which just accumulates bytes waiting for flush(), never
  // called here), so without this it silently never displays anything.
  videoOut.setVideoFormat(VideoFormat::RGB565);
  videoOut.setVideoInfoSource(aviDemuxer);

  // Register the codecs this AVI file's tracks may use (see the comment
  // at the top of this file - VideoPlayer's multi-decoders start empty).
  player.addVideoDecoder(h264Decoder, VideoFormat::H264, isH264Video);
  player.addVideoDecoder(mjpegDecoder, VideoFormat::MJPEG, isMjpegVideo);
  player.addAudioDecoder(mp3Decoder, "audio/mpeg");
  player.addAudioDecoder(aacDecoder, "audio/aac");
  player.addAudioDecoder(wavDecoder, "audio/vnd.wave");

  // This file has a real audio track - schedule video against it instead
  // of wall-clock time (see the class comment's "Audio clock" section).
  player.setUseAudioClock(true);

  if (!player.begin(file)) {
    Serial.println("VideoPlayer begin() failed");
    exit(1);
  }
}

void loop() {
  static uint32_t diagLast = 0;
  if (millis() - diagLast > 1000) {
    Serial.print("avg render ms - I: ");
    Serial.print(player.videoSyncTask().avgIFrameMs());
    Serial.print(" / P: ");
    Serial.print(player.videoSyncTask().avgPFrameMs());
    Serial.print(" / overall: ");
    Serial.println(player.videoSyncTask().avgFrameMs());
    diagLast = millis();
  }

  if (player.copy() == 0) {
    Serial.println("Done");
    file.close();
    exit(0);
  }
}
