/**
 * @file decode-avi.ino
 * @brief Desktop counterpart of sd-avi-audio-video.ino: plays a local .avi
 * file's audio and video tracks, demuxed live with DemuxerAVI. Display/
 * audio are swapped for their desktop equivalents: OutputTinyGPU ->
 * OutputOpenCV, I2SStream -> PortAudioStream.
 *
 * Decodes MJPEG, not H.264 - OutputOpenCV's default mode decodes MJPEG
 * itself (cv::imdecode); see sd-avi-video.ino for producing an H.264-in-AVI
 * file instead. Audio codec is auto-detected via DecoderHelix (multi-format:
 * WAV/AAC/MP3), fed DemuxerAVI's parsed mime type directly via
 * multiDecoder.setMimeSource(aviDemuxer).
 *
 * File feeding uses CodecCopy rather than StreamCopy; its write() must
 * retry on a partial accept (DemuxerAVI's parse buffer only has so much
 * room per call) - dropping the unwritten remainder desyncs the demuxer
 * from the real chunk boundaries. ContainerAVI.h itself needs enough bytes
 * buffered before reading a chunk header, else it can read stale/garbage
 * data past what's been written. Both are fixed at the library level
 * (CodecCopy.h, ContainerAVI.h) - not sketch-specific workarounds.
 *
 * Audio/video sync: DemuxerAVI now defaults to VideoAudioClockSync (mirrors
 * DemuxerMP4's wall-clock approach) - it waits only the remaining time
 * until each frame's scheduled instant, instead of a blind delay() that
 * underruns audio once a frame (JPEG decode + cv::imshow) takes longer
 * than expected. Audio is written straight through; PortAudioStream's
 * blocking write sets the real-time pace.
 *
 * Pipeline: File -> CodecCopy -> DemuxerAVI (demux)
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
#include "AudioTools/Video/OutputTest.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144-mjpeg.avi";

OutputOpenCV videoOut;  // default mode is MJPEG - decodes the JPEG itself
PortAudioStream out;
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes PCM/AAC/MP3 -> PortAudio

DemuxerAVI aviDemuxer;
File file;
CodecCopy copier(aviDemuxer, file);

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

  multiDecoder.setMimeSource(aviDemuxer);
  audioOut.begin();

  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setOutputVideo(videoOut);
  aviDemuxer.begin();

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
