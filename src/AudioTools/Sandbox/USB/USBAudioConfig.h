#pragma once
#include "stdint.h"
#include "AudioTools/CoreAudio/AudioTypes.h"


namespace audio_tools {



#ifdef STANDALONE_USB
/**
 * @brief Configuration structure for USB Audio.
 *
 * This struct holds all configuration parameters for USB audio devices, including
 * sample rate, channel count, endpoint addresses, buffer sizes, and feature flags.
 *
 * Members are used to control the behavior and capabilities of the USB audio interface.
 */
struct USBAudioConfig  {
  uint32_t sample_rate = 44100;  // Default sample rate
  uint8_t channels = 2;          // Default number of channels
  uint8_t bits_per_sample = 16;  // Default bits per sample
#else
/**
 * @brief Configuration structure for USB Audio, inheriting from AudioInfo.
 *
 * This struct extends AudioInfo and adds USB-specific configuration parameters such as
 * endpoint addresses, buffer sizes, and feature flags for USB audio streaming.
 */
struct USBAudioConfig : public AudioInfo {
#endif
  uint8_t entity_id_input_terminal = 1;
  uint8_t entity_id_feature_unit = 2;
  uint8_t entity_id_output_terminal = 3;

  uint8_t ep_in = 0x81;   // IN endpoint address (default 0x81)
  uint8_t ep_out = 0x01;  // OUT endpoint address (default 0x01)
  uint16_t ep_in_size = 256;
  uint16_t ep_out_size = 256;
  //uint8_t interface_num = 1;  // Audio streaming interface number
  // Moved feature flags and buffer sizes:
  bool enable_ep_in = true;
  bool enable_ep_out = true;
  bool enable_feedback_ep = false;
  bool enable_ep_in_flow_control = false;
  bool enable_interrupt_ep = false;
  bool enable_fifo_mutex = false;
  bool use_linear_buffer_rx = false;
  bool use_linear_buffer_tx = false;
  int audio_count = 1;                 // Number of audio functions (ex CFG_TUD_AUDIO)
  int ctrl_buf_size_per_func = 64;     // Control buffer size per function
  int ep_in_buf_size_per_func = 256;   // IN endpoint buffer size per function
  int ep_out_buf_size_per_func = 256;  // OUT endpoint buffer size per function
  int lin_buf_in_size_per_func = 512;  // Linear buffer size for IN

  int audio_functions_count() const {
    int count = 0;
    if (enable_ep_in) count++;
    if (enable_ep_out) count++;
    return audio_count;
  }
};

}  // namespace audio_tools