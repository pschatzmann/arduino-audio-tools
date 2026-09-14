#pragma once
#include "stdint.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"

namespace audio_tools {

class I2SDriverBase {
public:
    virtual I2SConfig defaultConfig(RxTxMode mode) = 0;
    virtual I2SConfig config() = 0;
    virtual bool begin(I2SConfig cfg) = 0;
    virtual void end() = 0;
    virtual size_t writeBytes(const void *src, size_t size_bytes) = 0;
    virtual size_t readBytes(void *dest, size_t size_bytes) = 0;
    virtual int available() = 0;
    virtual int availableForWrite() = 0;
    virtual bool setAudioInfo(AudioInfo info) = 0;
    /**
     * Discards any audio already queued in the driver's own output buffer
     * (e.g. the I2S DMA buffer) but not yet physically played - useful when
     * switching to unrelated audio content (e.g. AudioPlayer moving to the
     * next track) to stop stale samples from continuing to play out during
     * the switch. Default no-op, since not every backend can support this;
     * override where the underlying driver allows it.
     */
    virtual void flush() {}
};


}