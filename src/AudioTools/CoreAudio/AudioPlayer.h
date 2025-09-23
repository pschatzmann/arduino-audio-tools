#pragma once

#include "AudioTools/AudioCodecs/AudioCodecs.h"
#include "AudioTools/CoreAudio/AudioBasic/Debouncer.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioMetaData/MetaData.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/BaseConverter.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/Fade.h"
#include "AudioTools/CoreAudio/StreamCopy.h"
#include "AudioTools/CoreAudio/VolumeStream.h"
#include "AudioTools/Disk/AudioSource.h"
#include "AudioToolsConfig.h"

/**
 * @defgroup player Player
 * @ingroup main
 * @brief Audio Player
 */

namespace audio_tools {

/**
 * @brief High-level audio playback pipeline and controller.
 *
 * Provides pull-driven playback from an AudioSource through optional decoding,
 * volume control and click-free fades to an AudioOutput/AudioStream/Print.
 *
 * Features:
 * - Playback control: begin, play, stop, next, previous, setIndex
 * - PCM and encoded formats via AudioDecoder with dynamic audio info updates
 * - Volume management (0.0–1.0) with pluggable VolumeControl
 * - Auto-fade in/out to avoid pops; optional silence while inactive
 * - Auto-advance on timeout with forward/backward navigation
 * - Metadata: ICY (via source) or ID3 (internal MetaDataID3)
 * - Callbacks: metadata updates and stream-change notification
 * - Flow control: adjustable copy buffer and optional delay when output is full
 *
 * Pipeline: AudioSource → StreamCopy → EncodedAudioOutput → VolumeStream →
 * FadeStream → Output.
 *
 * Operation model: call copy() regularly (non-blocking) or copyAll() for
 * blocking end-to-end playback.
 *
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioPlayer : public AudioInfoSupport, public VolumeSupport {
 public:
  /// Default constructor
  AudioPlayer() { TRACED(); }

  /**
   * @brief Construct a new Audio Player object. The processing chain is
   * AudioSource -> Stream-copy -> EncodedAudioStream -> VolumeStream ->
   * FadeStream -> Print
   *
   * @param source
   * @param output
   * @param decoder
   */
  AudioPlayer(AudioSource &source, AudioOutput &output, AudioDecoder &decoder) {
    TRACED();
    this->p_source = &source;
    this->p_decoder = &decoder;
    setOutput(output);
    // notification for audio configuration
    decoder.addNotifyAudioChange(*this);
  }

  /**
   * @brief Construct a new Audio Player object. The processing chain is
   * AudioSource -> Stream-copy -> EncodedAudioStream -> VolumeStream ->
   * FadeStream -> Print
   *
   * @param source
   * @param output
   * @param decoder
   * @param notify
   */
  AudioPlayer(AudioSource &source, Print &output, AudioDecoder &decoder,
              AudioInfoSupport *notify = nullptr) {
    TRACED();
    this->p_source = &source;
    this->p_decoder = &decoder;
    setOutput(output);
    addNotifyAudioChange(notify);
  }

  /**
   * @brief Construct a new Audio Player object. The processing chain is
   * AudioSource -> Stream-copy -> EncodedAudioStream -> VolumeStream ->
   * FadeStream -> Print
   *
   * @param source
   * @param output
   * @param decoder
   */
  AudioPlayer(AudioSource &source, AudioStream &output, AudioDecoder &decoder) {
    TRACED();
    this->p_source = &source;
    this->p_decoder = &decoder;
    setOutput(output);
    // notification for audio configuration
    decoder.addNotifyAudioChange(*this);
  }

  /// Non-copyable: copy constructor is deleted
  AudioPlayer(AudioPlayer const &) = delete;

  /// Non-assignable: assignment operator is deleted
  AudioPlayer &operator=(AudioPlayer const &) = delete;

  /// Sets the final output to an AudioOutput (adds Volume/Fade for PCM)
  void setOutput(AudioOutput &output) {
    if (p_decoder->isResultPCM()) {
      this->fade.setOutput(output);
      this->volume_out.setOutput(fade);
      out_decoding.setOutput(&volume_out);
      out_decoding.setDecoder(p_decoder);
    } else {
      out_decoding.setOutput(&output);
      out_decoding.setDecoder(p_decoder);
    }
    this->p_final_print = &output;
    this->p_final_stream = nullptr;
  }

  /// Sets the final output to a Print (adds Volume/Fade for PCM)
  void setOutput(Print &output) {
    if (p_decoder->isResultPCM()) {
      this->fade.setOutput(output);
      this->volume_out.setOutput(fade);
      out_decoding.setOutput(&volume_out);
      out_decoding.setDecoder(p_decoder);
    } else {
      out_decoding.setOutput(&output);
      out_decoding.setDecoder(p_decoder);
    }
    this->p_final_print = nullptr;
    this->p_final_stream = nullptr;
  }

  /// Sets the final output to an AudioStream (adds Volume/Fade for PCM)
  void setOutput(AudioStream &output) {
    if (p_decoder->isResultPCM()) {
      this->fade.setOutput(output);
      this->volume_out.setOutput(fade);
      out_decoding.setOutput(&volume_out);
      out_decoding.setDecoder(p_decoder);
    } else {
      out_decoding.setOutput(&output);
      out_decoding.setDecoder(p_decoder);
    }
    this->p_final_print = nullptr;
    this->p_final_stream = &output;
  }

  /// Sets the internal copy buffer size (bytes)
  void setBufferSize(int size) { copier.resize(size); }

  /// Starts or restarts playback from the first or given stream index
  bool begin(int index = 0, bool isActive = true) {
    TRACED();
    bool result = false;
    // initilaize volume
    if (current_volume == -1.0f) {
      setVolume(1.0f);
    } else {
      setVolume(current_volume);
    }

    // take definition from source
    autonext = p_source->isAutoNext();

    // initial audio info for fade from output when not defined yet
    setupFade();

    // start dependent objects
    out_decoding.begin();

    if (!p_source->begin()){
      LOGE("Could not start audio source");
      return false;
    }
    if (!meta_out.begin()){
      LOGE("Could not start metadata output");
      return false;
    }
    if (!volume_out.begin()){
      LOGE("Could not start volume control");
      return false;
    }

    if (index >= 0) {
      setStream(p_source->selectStream(index));
      if (p_input_stream != nullptr) {
        if (meta_active) {
          copier.setCallbackOnWrite(decodeMetaData, this);
        }
        copier.begin(out_decoding, *p_input_stream);
        timeout = millis() + p_source->timeoutAutoNext();
        active = isActive;
        result = true;
      } else {
        LOGW("-> begin: no data found");
        active = false;
        result = false;
      }
    } else {
      LOGW("-> begin: no stream selected");
      active = isActive;
      result = false;
    }
    return result;
  }

  /// Ends playback and resets decoder/intermediate stages
  void end() {
    TRACED();
    active = false;
    out_decoding.end();
    meta_out.end();
    // remove any data in the decoder
    if (p_decoder != nullptr) {
      LOGI("reset codec");
      p_decoder->end();
      p_decoder->begin();
    }
  }

  /// Returns the active AudioSource
  AudioSource &audioSource() { return *p_source; }

  /// Sets or replaces the AudioSource
  void setAudioSource(AudioSource &source) { this->p_source = &source; }

  /// Sets or replaces the AudioDecoder
  void setDecoder(AudioDecoder &decoder) {
    this->p_decoder = &decoder;
    out_decoding.setDecoder(p_decoder);
  }

  /// Adds/updates a listener notified on audio info changes
  void addNotifyAudioChange(AudioInfoSupport *notify) {
    this->p_final_notify = notify;
    // notification for audio configuration
    if (p_decoder != nullptr) {
      p_decoder->addNotifyAudioChange(*this);
    }
  }

  /// Receives and forwards updated AudioInfo to the chain
  void setAudioInfo(AudioInfo info) override {
    TRACED();
    LOGI("sample_rate: %d", (int)info.sample_rate);
    LOGI("bits_per_sample: %d", (int)info.bits_per_sample);
    LOGI("channels: %d", (int)info.channels);
    this->info = info;
    // notifiy volume
    volume_out.setAudioInfo(info);
    fade.setAudioInfo(info);
    // notifiy final ouput: e.g. i2s
    if (p_final_print != nullptr) p_final_print->setAudioInfo(info);
    if (p_final_stream != nullptr) p_final_stream->setAudioInfo(info);
    if (p_final_notify != nullptr) p_final_notify->setAudioInfo(info);
  };

  /// Returns the current AudioInfo of the playback chain
  AudioInfo audioInfo() override { return info; }

  /// starts / resumes the playing after calling stop(): same as setActive(true)
  /// Resumes playback after stop(); equivalent to setActive(true)
  void play() {
    TRACED();
    setActive(true);
  }

  /// plays a complete audio file or url from start to finish (blocking call)
  /// @param path path to the audio file or url to play (depending on source)
  /// @return true if file was found and played successfully, false otherwise
  bool playPath(const char *path) {
    TRACED();
    if (!setPath(path)) {
      LOGW("Could not open file: %s", path);
      return false;
    }

    LOGI("Playing %s", path);
    // start if inactive
    play();
    // process all data
    copyAll();

    LOGI("%s has finished playing", path);
    return true;
  }

  /// Obsolete: use PlayPath!
  bool playFile(const char *path) { return playPath(path); }


  /// Halts playback; equivalent to setActive(false)
  void stop() {
    TRACED();
    setActive(false);
  }

  /// Moves to the next/previous stream by offset (negative supported)
  bool next(int offset = 1) {
    TRACED();
    writeEnd();
    stream_increment = offset >= 0 ? 1 : -1;
    active = setStream(p_source->nextStream(offset));
    return active;
  }

  /// Selects stream by absolute index in the source
  bool setIndex(int idx) {
    TRACED();
    writeEnd();
    stream_increment = 1;
    active = setStream(p_source->selectStream(idx));
    return active;
  }

  /// Selects stream by path without changing the source iterator
  bool setPath(const char *path) {
    TRACED();
    writeEnd();
    stream_increment = 1;
    active = setStream(p_source->selectStream(path));
    return active;
  }

  /// Moves back by offset streams (defaults to 1)
  bool previous(int offset = 1) {
    TRACED();
    writeEnd();
    stream_increment = -1;
    active = setStream(p_source->previousStream(abs(offset)));
    return active;
  }

  /// Activates the provided Stream as current input
  bool setStream(Stream *input) {
    end();
    out_decoding.begin();
    p_input_stream = input;
    if (p_input_stream != nullptr) {
      LOGD("open selected stream");
      meta_out.begin();
      copier.begin(out_decoding, *p_input_stream);
    }
    // execute callback if defined
    if (on_stream_change_callback != nullptr)
      on_stream_change_callback(p_input_stream, p_reference);
    return p_input_stream != nullptr;
  }

  /// Returns the currently active input Stream (e.g., file)
  Stream *getStream() { return p_input_stream; }

  /// Checks whether playback is active
  bool isActive() { return active; }

  /// Boolean conversion returns isActive()
  operator bool() { return isActive(); }

  /// Toggles playback activity; triggers fade and optional silence
  void setActive(bool isActive) {
    if (is_auto_fade) {
      if (isActive) {
        fade.setFadeInActive(true);
      } else {
        fade.setFadeOutActive(true);
        copier.copy();
        writeSilence(2048);
      }
    }
    active = isActive;
  }

  /// Sets volume in range [0.0, 1.0]; updates VolumeStream
  bool setVolume(float volume) override {
    bool result = true;
    if (volume >= 0.0f && volume <= 1.0f) {
      if (abs(volume - current_volume) > 0.01f) {
        LOGI("setVolume(%f)", volume);
        volume_out.setVolume(volume);
        current_volume = volume;
      }
    } else {
      LOGE("setVolume value '%f' out of range (0.0 -1.0)", volume);
      result = false;
    }
    return result;
  }

  /// Returns the current volume [0.0, 1.0]
  float volume() override { return current_volume; }

  /// Enables/disables auto-advance at end/timeout (overrides AudioSource)
  void setAutoNext(bool next) { autonext = next; }

  /// Sets delay (ms) to wait when output is full
  void setDelayIfOutputFull(int delayMs) { delay_if_full = delayMs; }

  /// Copies DEFAULT_BUFFER_SIZE (=1024 bytes) from the source to the decoder:
  /// Call this method in the loop.
  size_t copy() { return copy(copier.bufferSize()); }

  /// Copies until source is exhausted (blocking)
  size_t copyAll() {
    size_t result = 0;
    size_t step = copy();
    while (step > 0) {
      result += step;
      step = copy();
    }
    return result;
  }

  /// Copies the requested bytes from source to decoder (call in loop)
  size_t copy(size_t bytes) {
    size_t result = 0;
    if (active) {
      if (delay_if_full != 0 && ((p_final_print != nullptr &&
                                  p_final_print->availableForWrite() == 0) ||
                                 (p_final_stream != nullptr &&
                                  p_final_stream->availableForWrite() == 0))) {
        // not ready to do anything - so we wait a bit
        delay(delay_if_full);
        LOGD("copy: %d -> 0", (int)bytes);
        return 0;
      }
      // handle sound
      result = copier.copyBytes(bytes);
      if (result > 0 || timeout == 0) {
        // reset timeout if we had any data
        timeout = millis() + p_source->timeoutAutoNext();
      }
      // move to next stream after timeout
      moveToNextFileOnTimeout();

      // return silence when there was no data
      if (result < bytes && silence_on_inactive) {
        writeSilence(bytes - result);
      }

    } else {
      // e.g. A2DP should still receive data to keep the connection open
      if (silence_on_inactive) {
        writeSilence(bytes);
      }
    }
    LOGD("copy: %d -> %d", (int)bytes, (int)result);
    return result;
  }

  /// Sets a custom VolumeControl implementation
  void setVolumeControl(VolumeControl &vc) { volume_out.setVolumeControl(vc); }

  /// Provides access to StreamCopy to register additional callbacks
  /// callbacks
  StreamCopy &getStreamCopy() { return copier; }

  /// When enabled, writes zeros while inactive to keep sinks alive
  void setSilenceOnInactive(bool active) { silence_on_inactive = active; }

  /// Returns whether silence-on-inactive is enabled
  bool isSilenceOnInactive() { return silence_on_inactive; }

  /// Writes the requested number of zero bytes to the output
  void writeSilence(size_t bytes) {
    TRACEI();
    if (p_final_print != nullptr) {
      p_final_print->writeSilence(bytes);
    } else if (p_final_stream != nullptr) {
      p_final_stream->writeSilence(bytes);
    }
  }

  /// Returns the VolumeStream used by the player
  VolumeStream &getVolumeStream() { return volume_out; }

  /// Enables/disables automatic fade in/out to prevent pops
  void setAutoFade(bool active) { is_auto_fade = active; }

  /// Checks whether automatic fade in/out is enabled
  bool isAutoFade() { return is_auto_fade; }

  /// Sets the maximum ID3 metadata buffer size (default 256)
  void setMetaDataSize(int size) { meta_out.resize(size); }

  /// Sets a user reference passed to the stream-change callback
  void setReference(void *ref) { p_reference = ref; }

  /// Defines the metadata callback
  void setMetadataCallback(void (*callback)(MetaDataType type, const char *str,
                                            int len),
                           ID3TypeSelection sel = SELECT_ID3) {
    TRACEI();
    // setup metadata.
    if (p_source->setMetadataCallback(callback)) {
      // metadata is handled by source
      LOGI("Using ICY Metadata");
      meta_active = false;
    } else {
      // metadata is handled here
      meta_out.setCallback(callback);
      meta_out.setFilter(sel);
      meta_active = true;
    }
  }

  /// Defines a callback that is called when the stream is changed
  void setOnStreamChangeCallback(void (*callback)(Stream *stream_ptr,
                                                  void *reference)) {
    on_stream_change_callback = callback;
    if (p_input_stream!=nullptr) callback(p_input_stream, p_reference);
  }

 protected:
  bool active = false;
  bool autonext = true;
  bool silence_on_inactive = false;
  AudioSource *p_source = nullptr;
  VolumeStream volume_out;  // Volume control
  FadeStream fade;          // Phase in / Phase Out to avoid popping noise
  MetaDataID3 meta_out;     // Metadata parser
  EncodedAudioOutput out_decoding;  // Decoding stream
  CopyDecoder no_decoder{true};
  AudioDecoder *p_decoder = &no_decoder;
  Stream *p_input_stream = nullptr;
  AudioOutput *p_final_print = nullptr;
  AudioStream *p_final_stream = nullptr;
  AudioInfoSupport *p_final_notify = nullptr;
  StreamCopy copier;  // copies sound into i2s
  AudioInfo info;
  bool meta_active = false;
  uint32_t timeout = 0;
  int stream_increment = 1;      // +1 moves forward; -1 moves backward
  float current_volume = -1.0f;  // illegal value which will trigger an update
  int delay_if_full = 100;
  bool is_auto_fade = true;
  void *p_reference = nullptr;
  void (*on_stream_change_callback)(Stream *stream_ptr,
                                    void *reference) = nullptr;

  void setupFade() {
    if (p_final_print != nullptr) {
      fade.setAudioInfo(p_final_print->audioInfo());
    } else if (p_final_stream != nullptr) {
      fade.setAudioInfo(p_final_stream->audioInfo());
    }
  }

  void moveToNextFileOnTimeout() {
    if (p_final_stream != nullptr && p_final_stream->availableForWrite() == 0)
      return;
    if (p_input_stream == nullptr || millis() > timeout) {
      if (is_auto_fade) fade.setFadeInActive(true);
      if (autonext) {
        LOGI("-> timeout - moving by %d", stream_increment);
        // open next stream
        if (!next(stream_increment)) {
          LOGD("stream is null");
        }
      } else {
        active = false;
      }
      timeout = millis() + p_source->timeoutAutoNext();
    }
  }

  void writeEnd() {
    // end silently
    TRACEI();
    if (is_auto_fade) {
      fade.setFadeOutActive(true);
      copier.copy();
      // start by fading in
      fade.setFadeInActive(true);
    }
    // restart the decoder to make sure it does not contain any audio when we
    // continue
    p_decoder->begin();
  }

  /// Callback implementation which writes to metadata
  static void decodeMetaData(void *obj, void *data, size_t len) {
    LOGD("%s, %zu", LOG_METHOD, len);
    AudioPlayer *p = (AudioPlayer *)obj;
    if (p->meta_active) {
      p->meta_out.write((const uint8_t *)data, len);
    }
  }
};

}  // namespace audio_tools
