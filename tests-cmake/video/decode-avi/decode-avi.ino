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
 * Audio/video sync: DemuxerAVI itself just writes audio straight through
 * and does nothing for video - all real sync work happens in videoSync
 * (VideoAudioSyncTask, see Video/VideoAudioSyncTask.h), which sits between
 * the demuxer and h264Decoder.
 * It buffers each decoded frame and renders it (decode + cv::imshow) from
 * its own background task, timed against audioClock - an
 * AudioTimeSourceStream inserted between multiDecoder and PortAudioStream
 * that turns "how many decoded PCM bytes have reached the audio device so
 * far" into an elapsed-ms clock. Since PortAudioStream's write() blocks at
 * the real playback rate, that clock tracks actual audio playback time -
 * so video is paced to how far audio has actually played, not to a
 * separate, independent wall-clock schedule. Because this all happens off
 * the demuxer's own thread, a slow decode/display frame never delays the
 * next audio chunk the way an inline wait used to.
 *
 * Pipeline: File -> CodecCopy -> DemuxerAVI (demux)
 *   -> EncodedAudioStream (DecoderHelix: WAV/AAC/MP3) -> AudioTimeSourceStream
 *   (audio clock) -> PortAudioStream (audio)
 *   \-> VideoAudioSyncTask (buffer + schedule) -> H264Decoder (H.264 decode
 *   -> RGB565) -> OutputOpenCV (draw) (video, own background task)
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
// 3rd arg: scheduling delay compensating for PortAudioStream's own output
// buffering below (buffer_size*buffer_count bytes) - see
// VideoAudioSyncTask::setSchedulingDelayMs(); ~50ms is a rough match for
// 1024*10 bytes of 44.1kHz/16-bit/stereo PCM, tune for your own config.
VideoAudioSyncTask videoSync(h264Decoder, 0, 50);
PortAudioStream out;
AudioTimeSourceStream audioClock(out);  // decoded-PCM-bytes-based playback clock
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&audioClock, &multiDecoder);  // decodes PCM/AAC/MP3 -> audioClock -> PortAudio

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

  videoSync.setAudioClock(audioClock);

  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setOutputVideo(videoSync);
  aviDemuxer.begin();
}

void loop() {
  // AVI's header (which carries the nominal frame rate) always precedes
  // the movi data - so this becomes available strictly before the first
  // video frame ever reaches videoSync, no polling/gating needed beyond
  // "call it every time, cheaply, until it's non-zero".
  float fps = aviDemuxer.getVideoInfo().fps;
  if (fps > 0) videoSync.setFps(fps);

  static uint32_t diagLast = 0;
  if (millis() - diagLast > 1000) {
    Serial.print("avg render ms - I: ");
    Serial.print(videoSync.avgIFrameMs());
    Serial.print(" / P: ");
    Serial.print(videoSync.avgPFrameMs());
    Serial.print(" / overall: ");
    Serial.println(videoSync.avgFrameMs());
    diagLast = millis();
  }

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
