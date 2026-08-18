/**
 * @file decode-avi.ino
 * @brief Desktop counterpart of sd-avi-audio-video.ino: plays a local .avi
 * file's audio + H.264 video tracks, demuxed live with DemuxerAVI and
 * decoded with H264Decoder (TinyH264). Display/audio are swapped for their
 * desktop equivalents: OutputTinyGPU -> OutputOpenCV, I2SStream ->
 * PortAudioStream.
 *
 * H264Decoder outputs already-decoded RGB565 frames (its default pixel
 * format) - videoOut.setVideoFormat(VideoFormat::RGB565) tells OutputOpenCV
 * to display those directly instead of its default MJPEG-accumulate mode
 * (which just buffers bytes waiting for a flush() that raw decoders never
 * call). Audio codec is auto-detected via DecoderHelix (multi-format:
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
 * Audio/video sync: DemuxerAVI defaults to VideoAudioClockSync (mirrors
 * DemuxerMP4's wall-clock approach) - it waits only the remaining time
 * until each frame's scheduled instant, instead of a blind delay() that
 * underruns audio once a frame (H.264 decode + cv::imshow) takes longer
 * than expected. Audio is written straight through; PortAudioStream's
 * blocking write sets the real-time pace.
 *
 * Pipeline: File -> CodecCopy -> DemuxerAVI (demux)
 *   -> EncodedAudioStream (DecoderHelix: WAV/AAC/MP3) -> PortAudioStream (audio)
 *   \-> H264Decoder (H.264 decode -> RGB565) -> OutputOpenCV (draw) (video)
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
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputOpenCV.h"
#include "AudioTools/Video/OutputTest.h"
#include "SD.h"

// ---- File to play ----
const char *file_path = "/media/pschatzmann/External/Videos/output176x144.avi";

OutputOpenCV videoOut;  // default mode is MJPEG - decodes the JPEG itself
H264Decoder h264Decoder(videoOut);
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
  // H264Decoder outputs already-decoded RGB565 frames (its default pixel
  // format), not MJPEG bytes - OutputOpenCV defaults to MJPEG mode (which
  // just accumulates bytes waiting for flush(), never called here), so
  // without this it silently never displays anything.
  videoOut.setVideoFormat(VideoFormat::RGB565);
  videoOut.setVideoInfoSource(aviDemuxer);
  audioOut.begin();

  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setOutputVideo(h264Decoder);
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
