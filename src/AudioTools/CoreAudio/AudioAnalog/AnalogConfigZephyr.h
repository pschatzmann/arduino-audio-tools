#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

#ifndef ANALOG_BUFFERS
#  define ANALOG_BUFFERS 10
#endif

#ifndef ANALOG_MAX_OUT_CHANNELS
#  define ANALOG_MAX_OUT_CHANNELS 10
#endif

#include "AudioTools/CoreAudio/AudioTypes.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/drivers/adc.h>
#include <vector>

namespace audio_tools {

class AnalogAudioZephyr;

/**
 * @brief Generic ADC and DAC configuration
 *
 * @author Phil Schatzmann
 * @ingroup platform
 * @copyright GPLv3
 */
class AnalogConfigZephyr : public AudioInfo {
  friend class AnalogAudioArduino;
  public:
    int buffer_count = ANALOG_BUFFERS;
    int buffer_size = ANALOG_BUFFER_SIZE;
    RxTxMode rx_tx_mode = RX_MODE;
    bool is_blocking_write = true;
    bool is_auto_center_read = true;
    int max_sample_rate = ANALOG_MAX_SAMPLE_RATE;

    AnalogConfigZephyr() = default;
    AnalogConfigZephyr(RxTxMode rxtxMode) : AudioInfo() {
      rx_tx_mode = rxtxMode;
    }

    // assign dac via DAC_DT_SPEC_GET(DT_NODELABEL(audio_dac0));
    std::vector<dac_dt_spec> dac;
    std::vector<adc_dt_spec> adc;

};

#ifndef ANALOG_CONFIG
#define ANALOG_CONFIG
using AnalogConfig = AnalogConfigZephyr;
#endif

} // ns
#//endif
