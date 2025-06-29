#include <cstdint>
#include <cstring>
#include <functional>

#include "tusb.h"

namespace tinyusb {

// Audio 2.0 Descriptor Generator
class USBAudio2DescriptorBuilder {
 public:
  USBAudio2DescriptorBuilder(uint8_t epIn, uint8_t epOut)
      : sampleRate_(44100),
        channels_(2),
        bitsPerSample_(16),
        entityIdInputTerminal_(1),
        entityIdFeatureUnit_(2),
        entityIdOutputTerminal_(3) {
    this->epIn = epIn;
    this->epOut = epOut;
  }

  void setSampleRate(uint32_t rate) { sampleRate_ = rate; }
  void setNumChannels(uint8_t ch) { channels_ = ch; }
  void setBitsPerSample(uint8_t bits) { bitsPerSample_ = bits; }

  uint16_t calcMaxPacketSize() const {
    uint16_t bytesPerFrame = (bitsPerSample_ / 8) * channels_;
    uint16_t samplesPerMs = (sampleRate_ + 999) / 1000;
    return bytesPerFrame * samplesPerMs;
  }

  const uint8_t* buildDescriptor(uint8_t itf, uint8_t alt, uint16_t* outLen) {
    static uint8_t desc[256];
    uint8_t* p = desc;

    if (alt == 0) {
      p = addStandardInterfaceDesc(p, itf, alt, 0);
    } else {
      p = addStandardInterfaceDesc(p, itf, alt, 1);
      p = addCsInterfaceHeader(p);
      p = addInputTerminalDesc(p);
      p = addFeatureUnitDesc(p);
      p = addOutputTerminalDesc(p);
      p = addFormatTypeDesc(p);
      p = addIsoDataEndpointDesc(p);
      p = addCsIsoEndpointDesc(p);
    }

    *outLen = static_cast<uint16_t>(p - desc);
    return desc;
  }

 private:
  uint8_t* addStandardInterfaceDesc(uint8_t* p, uint8_t itf, uint8_t alt,
                                    uint8_t numEps) {
    *p++ = 9;       // bLength
    *p++ = 0x04;    // bDescriptorType = Interface
    *p++ = itf;     // bInterfaceNumber
    *p++ = alt;     // bAlternateSetting
    *p++ = numEps;  // bNumEndpoints
    *p++ = 0x01;    // bInterfaceClass (AUDIO)
    *p++ = 0x02;    // bInterfaceSubClass (Streaming)
    *p++ = 0x20;    // bInterfaceProtocol (2.0)
    *p++ = 0;       // iInterface
    return p;
  }

  uint8_t* addCsInterfaceHeader(uint8_t* p) {
    *p++ = 7;     // bLength
    *p++ = 0x24;  // CS_INTERFACE
    *p++ = 0x01;  // HEADER subtype
    *p++ = 0x00;
    *p++ = 0x02;  // bcdADC = 2.00
    *p++ = 0x01;  // bCategory = 1 (Speaker)
    *p++ = 0x00;  // wTotalLength (placeholder)
    return p;
  }

  uint8_t* addInputTerminalDesc(uint8_t* p) {
    *p++ = 17;                      // bLength
    *p++ = 0x24;                    // CS_INTERFACE
    *p++ = 0x02;                    // INPUT_TERMINAL subtype
    *p++ = entityIdInputTerminal_;  // bTerminalID
    *p++ = 0x01;
    *p++ = 0x01;       // wTerminalType = USB Streaming
    *p++ = 0x00;       // bAssocTerminal
    *p++ = channels_;  // bNrChannels
    *p++ = 0x03;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;  // wChannelConfig
    *p++ = 0;     // iChannelNames
    *p++ = 0;     // iTerminal
    return p;
  }

  uint8_t* addFeatureUnitDesc(uint8_t* p) {
    uint8_t length = 7 + (channels_ + 1) * 2;
    *p++ = length;                  // bLength
    *p++ = 0x24;                    // CS_INTERFACE
    *p++ = 0x06;                    // FEATURE_UNIT subtype
    *p++ = entityIdFeatureUnit_;    // bUnitID
    *p++ = entityIdInputTerminal_;  // bSourceID
    *p++ = 0x01;                    // bControlSize
    *p++ = 0x01;                    // bmaControls[0] (Master Mute)
    for (uint8_t i = 1; i <= channels_; ++i) {
      *p++ = 0x03;  // bmaControls[i] (Mute + Volume)
    }
    *p++ = 0x00;  // iFeature
    return p;
  }

  uint8_t* addOutputTerminalDesc(uint8_t* p) {
    *p++ = 12;                       // bLength
    *p++ = 0x24;                     // CS_INTERFACE
    *p++ = 0x03;                     // OUTPUT_TERMINAL subtype
    *p++ = entityIdOutputTerminal_;  // bTerminalID
    *p++ = 0x01;
    *p++ = 0x03;                  // wTerminalType = Speaker
    *p++ = 0x00;                  // bAssocTerminal
    *p++ = entityIdFeatureUnit_;  // bSourceID
    *p++ = 0x00;                  // iTerminal
    return p;
  }

  uint8_t* addFormatTypeDesc(uint8_t* p) {
    *p++ = 14;                  // bLength
    *p++ = 0x24;                // CS_INTERFACE
    *p++ = 0x02;                // FORMAT_TYPE subtype
    *p++ = 0x01;                // FORMAT_TYPE_I
    *p++ = channels_;           // bNrChannels
    *p++ = bitsPerSample_ / 8;  // bSubslotSize
    *p++ = bitsPerSample_;      // bBitResolution
    *p++ = 1;                   // bSamFreqType
    *p++ = (uint8_t)(sampleRate_ & 0xFF);
    *p++ = (uint8_t)((sampleRate_ >> 8) & 0xFF);
    *p++ = (uint8_t)((sampleRate_ >> 16) & 0xFF);
    return p;
  }

  uint8_t* addIsoDataEndpointDesc(uint8_t* p) {
    uint16_t packetSize = calcMaxPacketSize();
    *p++ = 7;                         // bLength
    *p++ = 0x05;                      // ENDPOINT descriptor
    *p++ = epIn;                      // bEndpointAddress
    *p++ = 0x05;                      // bmAttributes (Isochronous, Async)
    *p++ = packetSize & 0xFF;         // wMaxPacketSize LSB
    *p++ = (packetSize >> 8) & 0xFF;  // wMaxPacketSize MSB
    *p++ = 0x01;                      // bInterval
    return p;
  }

  uint8_t* addCsIsoEndpointDesc(uint8_t* p) {
    *p++ = 8;     // bLength
    *p++ = 0x25;  // CS_ENDPOINT
    *p++ = 0x01;  // EP_GENERAL subtype
    *p++ = 0x00;  // bmAttributes
    *p++ = 0x00;  // bmControls
    *p++ = 0x00;
    *p++ = 0x00;  // wLockDelayUnits, wLockDelay
    return p;
  }

  uint32_t sampleRate_;
  uint8_t channels_;
  uint8_t bitsPerSample_;
  uint8_t epIn;
  uint8_t epOut;

  const uint8_t entityIdInputTerminal_;
  const uint8_t entityIdFeatureUnit_;
  const uint8_t entityIdOutputTerminal_;
};

}  // namespace tinyusb
