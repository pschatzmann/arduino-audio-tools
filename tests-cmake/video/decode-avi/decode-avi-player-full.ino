/**
 * @file decode-avi-player-full.ino
 * @brief Same pipeline/test file as decode-avi-player.ino, but via
 * AudioTools/Video/VideoPlayerFull.h (a VideoPlayer subclass that
 * pre-registers H264Decoder/MJPEGDecoder/MPGDecoder and
 * MP3DecoderHelix/AACDecoderHelix/MP2Decoder itself - see its own class
 * comment) instead of registering the codecs by hand. VideoPlayerFull
 * pre-registers no container demuxer of its own, so this still supplies
 * one explicitly - a MultiVideoDemuxerFull (Video/MultiVideoDemuxerFull.h)
 * rather than a plain DemuxerAVI, to demonstrate the "any container
 * format just works too" pattern VideoPlayerFull's own class comment
 * recommends (this file happens to only ever feed it an AVI, but the same
 * player would equally auto-detect an MP4/MPG file).
 *
 * To build & run:
 * - mkdir build && cd build && cmake .. && make
 * - ./decode-avi-player-full
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/Video/MultiVideoDemuxerFull.h"
#include "AudioTools/Video/OutputOpenCV.h"
#include "AudioTools/Video/VideoPlayerFull.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144.avi";

OutputOpenCV videoOut;  // default mode is MJPEG - decodes the JPEG itself
PortAudioStream out;
MultiVideoDemuxerFull demuxer;  // auto-detects AVI/MP4/MPG
VideoPlayerFull player(demuxer, videoOut, out);
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
  videoOut.setVideoInfoSource(player.demuxer());

  // This file has a real audio track - schedule video against it instead
  // of wall-clock time (see VideoPlayer's class comment's "Audio clock"
  // section).
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
