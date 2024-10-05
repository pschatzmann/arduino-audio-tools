#pragma once

#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief We can build a input or an output chain: an input chain starts with
 * setInput(); followed by add() an output chain consinsts of add() and ends
 * with setOutput();
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Pipeline : public AudioStream {
  friend MultiOutput;

 public:
  Pipeline() = default;
  ///  adds a component
  bool add(ModifyingStream& io) {
    if (has_output) {
      LOGE("Output already defined");
      is_ok = false;
      return false;
    }
    if (has_input) {
      if (p_stream == nullptr) {
        LOGE("Input not defined");
        is_ok = false;
        return false;
      }

      // predecessor notifies successor
      if (p_ai_source != nullptr) {
        p_ai_source->addNotifyAudioChange(io);
      }

      // we set up an input chain
      components.push_back(&io);
      io.setStream(*p_stream);
      p_stream = &io;
      p_ai_source = &io;
    } else {
      // we assume an output chain
      if (size() > 0) {
        auto& last_c = last();
        last_c.setOutput(io);
        last_c.addNotifyAudioChange(io);
      }
      components.push_back(&io);
    }
    return true;
  }

  ///  adds a component
  bool add(ModifyingOutput& out) {
    if (has_output) {
      LOGE("Output already defined");
      is_ok = false;
      return false;
    }
    if (has_input) {
      LOGE("Input not supported");
      is_ok = false;
      return false;
    }
    ModifyingStream* io = new ModifyingStreamAdapter(out);
    cleanup.push_back(io);
    return add(*io);
  }

  /// Defines the output for an output pipeline: must be last call after add()
  bool setOutput(AudioOutput& out) {
    p_out_print = &out;
    if (size() > 0) {
      last().addNotifyAudioChange(out);
    }
    return setOutput((Print&)out);
  }

  /// Defines the output for an output pipeline: must be last call after add()
  bool setOutput(AudioStream& out) {
    p_out_stream = &out;
    if (size() > 0) {
      last().addNotifyAudioChange(out);
    }
    return setOutput((Print&)out);
  }

  /// Defines the output for an output pipeline: must be last call after add()
  bool setOutput(Print& out) {
    if (has_output) {
      LOGE("Output already defined");
      is_ok = false;
      return false;
    }
    p_print = &out;
    if (size() > 0) {
      last().setOutput(out);
    }
    // must be last element
    has_output = true;
    return true;
  }

  /// Defines the input for an input pipeline: must be first call before add()
  bool setInput(AudioStream& in) {
    p_ai_source = &in;
    p_ai_input = &in;
    return setInput((Stream&)in);
  }

  /// Defines the input for an input pipeline: must be first call before add()
  bool setInput(Stream& in) {
    if (has_output) {
      LOGE("Defined as output");
      is_ok = false;
      return false;
    }
    if (has_input) {
      LOGE("Input already defined");
      is_ok = false;
      return false;
    }
    // must be first
    has_input = true;
    p_stream = &in;
    return true;
  }

  int availableForWrite() override {
    if (!is_active) return 0;
    if (size() == 0) {
      if (p_print != nullptr) return p_print->availableForWrite();
      return 0;
    }
    return components[0]->availableForWrite();
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!is_active) return 0;
    if (size() == 0) {
      if (p_print != nullptr) return p_print->write(data, len);
      return 0;
    }
    LOGD("write: %u", (unsigned)len);
    return components[0]->write(data, len);
  }

  int available() override {
    if (!is_active) return 0;
    Stream* in = getInput();
    if (in == nullptr) return 0;
    return in->available();
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    if (!is_active) return 0;
    Stream* in = getInput();
    if (in == nullptr) return 0;
    return in->readBytes(data, len);
  }

  /// Optional method: Calls begin on all components and setAudioInfo on first
  /// coponent to update the full chain
  bool begin(AudioInfo info) {
    LOGI("begin");
    bool rc = begin();
    setAudioInfo(info);
    audioInfoOut().logInfo("pipeline out:");
    return rc;
  }

  /// Optional method: Calls begin on all components
  bool begin() override {
    has_output = false;
    bool ok = true;

    // avoid too much noise
    setNotifyActive(false);

    // start components
    for (auto c : components) {
      ok = ok && c->begin();
    }
    // start output
    if (p_out_stream != nullptr) {
      ok = ok && p_out_stream->begin();
    } else if (p_out_print != nullptr) {
      ok = ok && p_out_print->begin();
    }
    // start input
    if (p_ai_input != nullptr) {
      ok = ok && p_ai_input->begin();
    }

    setNotifyActive(true);
    is_active = ok;
    is_ok = ok;
    return ok;
  }

  /// Calls end on all components
  void end() override {
    for (auto c : components) {
      c->end();
    }
    components.clear();
    for (auto& c : cleanup) {
      delete c;
    }
    cleanup.clear();

    has_output = false;
    has_input = false;
    p_out_print = nullptr;
    p_out_stream = nullptr;
    p_print = nullptr;
    p_stream = nullptr;
    p_ai_source = nullptr;
    p_ai_input = nullptr;
    is_ok = false;
    is_active = true;
  }

  /// Defines the AudioInfo for the first node
  void setAudioInfo(AudioInfo newInfo) override {
    this->info = newInfo;
    if (has_input && p_ai_input != nullptr) {
      p_ai_input->setAudioInfo(info);
    } else if (has_output) {
      components[0]->setAudioInfo(info);
    }
  }

  /// Provides the resulting AudioInfo from the last node
  AudioInfo audioInfoOut() override {
    AudioInfo info = audioInfo();
    if (size() > 0) {
      info = last().audioInfoOut();
    }
    return info;
  }

  /// Returns true if we have at least 1 component
  bool hasComponents() { return size() > 0; }

  /// Provides the number of components
  int size() { return components.size(); }

  /// Provides the last component
  ModifyingStream& last() { return *components[size() - 1]; }

  /// Access to the components by index
  ModifyingStream& operator[](int idx) { return *components[idx]; }

  /// Subscribes to notifications on last component of the chain
  void addNotifyAudioChange(AudioInfoSupport& bi) override {
    if (size() > 0) last().addNotifyAudioChange(bi);
  }

  /// Activates/deactivates notifications
  void setNotifyActive(bool flag) {
    is_notify_active = flag;
    for (auto c : components) {
      c->setNotifyActive(flag);
    }
  }

  /// Activates/deactivates the pipeline (default is active)
  void setActive(bool flag){
    is_active = flag;
  }

  /// Determines if the pipeline is active
  bool isActive(){
    return is_active;
  }

  /// Returns true if pipeline is correctly set up 
  bool isOK(){
    return is_ok;
  }

  /// Returns true if pipeline is correctly set up and is active
  operator bool() { return is_ok && is_active; }

 protected:
  Vector<ModifyingStream*> components{0};
  Vector<ModifyingStream*> cleanup{0};
  bool has_output = false;
  bool has_input = false;
  bool is_ok = true;
  bool is_active = true;
  // prior input for input pipline
  Stream* p_stream = nullptr;
  AudioInfoSource* p_ai_source = nullptr;
  AudioStream* p_ai_input = nullptr;
  // output for notifications, begin and end calls
  AudioOutput* p_out_print = nullptr;
  AudioStream* p_out_stream = nullptr;
  Print* p_print = nullptr;

  /// Support for ModifyingOutput
  struct ModifyingStreamAdapter : public ModifyingStream {
    ModifyingStreamAdapter(ModifyingOutput& out) { p_out = &out; }
    /// Defines/Changes the input & output
    void setStream(Stream& in) override { p_out->setOutput(in); }
    /// Defines/Changes the output target
    void setOutput(Print& out) override { p_out->setOutput(out); }
    /// Read not supported
    int available() override { return 0; }

    size_t write(const uint8_t* data, size_t len) override {
      return p_out->write(data, len);
    }

    bool begin() override { return p_out->begin(); }

    void end() override { p_out->end(); }

    void setAudioInfo(AudioInfo info) override { p_out->setAudioInfo(info); }

    void addNotifyAudioChange(AudioInfoSupport& bi) override {
      p_out->addNotifyAudioChange(bi);
    }

   protected:
    ModifyingOutput* p_out = nullptr;
  };

  /// we read from the last node or the defined input: null if no input is
  /// available
  Stream* getInput() {
    Stream* in = p_stream;
    if (size() > 0) {
      in = &last();
    }
    return in;
  }
};

}  // namespace audio_tools
