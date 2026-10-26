// Desktop test of the stack independent USB Audio host logic
// (USBAudioHostBase) using a mock USBAudioHostBackend: descriptor parsing,
// topology resolution, format negotiation, the mount sequence, the runtime
// volume/mute requests and the isochronous data path.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioHostBase.h"

using namespace audio_tools;

/// Records all requests and answers control requests via respond()
class MockBackend : public USBAudioHostBackend {
 public:
  struct Request {
    UsbSetupPacket setup;
    std::vector<uint8_t> out_data;
    bool is_set_interface = false;
  };
  USBAudioHostEvents* events = nullptr;
  std::vector<Request> requests;
  std::vector<std::pair<uint8_t, UsbHostStreamRole>> opened;
  std::vector<uint8_t> closed;
  uint8_t* pending_data = nullptr;
  bool pending = false;
  uint32_t now = 0;

  bool begin(USBAudioHostEvents* ev, const USBAudioConfig&) override {
    events = ev;
    return true;
  }
  void end() override {}
  void task() override {}
  bool control(uint8_t, const UsbSetupPacket& setup, uint8_t* data) override {
    assert(!pending);  // only one control request in flight
    Request r;
    r.setup = setup;
    if (!(setup.bmRequestType & 0x80) && data)
      r.out_data.assign(data, data + setup.wLength);
    requests.push_back(r);
    pending_data = data;
    pending = true;
    return true;
  }
  bool setInterface(uint8_t, uint8_t itf, uint8_t alt) override {
    assert(!pending);
    Request r;
    r.setup.bmRequestType = 0x01;
    r.setup.bRequest = 0x0B;
    r.setup.wValue = alt;
    r.setup.wIndex = itf;
    r.is_set_interface = true;
    requests.push_back(r);
    pending_data = nullptr;
    pending = true;
    return true;
  }
  bool openStream(uint8_t, const UsbEndpointDescriptorView& ep,
                  UsbHostStreamRole role) override {
    opened.push_back({ep.bEndpointAddress, role});
    return true;
  }
  void closeStream(uint8_t, uint8_t ep) override { closed.push_back(ep); }
  uint32_t millis() const override { return now; }

  /// Completes the pending control request
  void respond(bool ok, const std::vector<uint8_t>& in = {}) {
    assert(pending);
    pending = false;
    if (pending_data && !in.empty())
      memcpy(pending_data, in.data(), in.size());
    events->onControlComplete(ok, (uint16_t)in.size());
  }
};

/// Exposes the protected API of USBAudioHostBase for the test
class TestHost : public USBAudioHostBase {
 public:
  MockBackend mock;
  RingBuffer<uint8_t> tx{0};
  RingBuffer<uint8_t> rx{0};
  BaseBuffer<uint8_t>& bufferTx() override { return tx; }
  BaseBuffer<uint8_t>& bufferRx() override { return rx; }
  void serviceUSB() override { processUSB(); }
  USBAudioHostEvents& events() { return *this; }
  Stream& rxStream() { return rx_; }
  Stream& txStream() { return tx_; }
  bool idle() const { return step_ == Step::Idle; }

 protected:
  USBAudioHostBackend& backend() override { return mock; }
  void resizeBuffers() override {
    size_t size = (size_t)audioPacketSize() * config_.fifo_packets;
    if (isEpOutEnabled()) tx.resize(size);
    if (isEpInEnabled()) rx.resize(size);
  }
};

// UAC1 headset: speaker (itf 1, 48000/44100 stereo) + mic (itf 2, 48000
// mono) + a HID interface which does not belong to the audio function
static const std::vector<uint8_t> kUac1Headset = {
    // AC interface 0
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    // AC header UAC1 (bcdADC 1.00), 2 streaming interfaces: 1, 2
    0x0A, 0x24, 0x01, 0x00, 0x01, 0x48, 0x00, 0x02, 0x01, 0x02,
    // IT 1: USB streaming, 2 channels
    0x0C, 0x24, 0x02, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
    // FU 2 (src 1): master mute + volume
    0x0A, 0x24, 0x06, 0x02, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00,
    // OT 3: speaker (src 2)
    0x09, 0x24, 0x03, 0x03, 0x01, 0x03, 0x00, 0x02, 0x00,
    // IT 4: microphone, 1 channel
    0x0C, 0x24, 0x02, 0x04, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // FU 5 (src 4): master mute + volume
    0x09, 0x24, 0x06, 0x05, 0x04, 0x01, 0x03, 0x00, 0x00,
    // OT 6: USB streaming (src 5)
    0x09, 0x24, 0x03, 0x06, 0x01, 0x01, 0x00, 0x05, 0x00,
    // AS interface 1 alt 0 + alt 1
    0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
    // AS_GENERAL: terminal link 1, PCM
    0x07, 0x24, 0x01, 0x01, 0x01, 0x01, 0x00,
    // FORMAT_TYPE I: 2 ch, 2 bytes, 16 bits, rates 48000, 44100
    0x0E, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x02, 0x80, 0xBB, 0x00, 0x44,
    0xAC, 0x00,
    // EP 0x01 OUT iso adaptive, 200 bytes
    0x09, 0x05, 0x01, 0x09, 0xC8, 0x00, 0x01, 0x00, 0x00,
    0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,
    // AS interface 2 alt 0 + alt 1
    0x09, 0x04, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
    0x09, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
    // AS_GENERAL: terminal link 6, PCM
    0x07, 0x24, 0x01, 0x06, 0x01, 0x01, 0x00,
    // FORMAT_TYPE I: 1 ch, 2 bytes, 16 bits, 48000
    0x0B, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01, 0x80, 0xBB, 0x00,
    // EP 0x82 IN iso async, 100 bytes
    0x09, 0x05, 0x82, 0x05, 0x64, 0x00, 0x01, 0x00, 0x00,
    0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,
    // HID interface 3: not part of the audio function
    0x09, 0x04, 0x03, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00};
static const uint16_t kUac1AudioLen = (uint16_t)(kUac1Headset.size() - 9);

// UAC2 stereo microphone with clock source 0x10
static const std::vector<uint8_t> kUac2Mic = {
    // IAD
    0x08, 0x0B, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00,
    // AC interface 0, protocol 0x20
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x20, 0x00,
    // AC header UAC2
    0x09, 0x24, 0x01, 0x00, 0x02, 0x03, 0x40, 0x00, 0x00,
    // clock source 0x10
    0x08, 0x24, 0x0A, 0x10, 0x01, 0x07, 0x00, 0x00,
    // IT 1: microphone, clock 0x10, 2 channels
    0x11, 0x24, 0x02, 0x01, 0x01, 0x02, 0x00, 0x10, 0x02, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    // FU 2 (src 1): master mute + volume programmable
    0x12, 0x24, 0x06, 0x02, 0x01, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // OT 3: USB streaming (src 2, clock 0x10)
    0x0C, 0x24, 0x03, 0x03, 0x01, 0x01, 0x00, 0x02, 0x10, 0x00, 0x00, 0x00,
    // AS interface 1 alt 0 + alt 1
    0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x20, 0x00,
    0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x20, 0x00,
    // AS_GENERAL UAC2: link 3, PCM, 2 channels
    0x10, 0x24, 0x01, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02, 0x03,
    0x00, 0x00, 0x00, 0x00,
    // FORMAT_TYPE I UAC2: subslot 2, 16 bits
    0x06, 0x24, 0x02, 0x01, 0x02, 0x10,
    // EP 0x81 IN iso async, 196 bytes
    0x07, 0x05, 0x81, 0x05, 0xC4, 0x00, 0x01,
    0x08, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

static std::vector<uint8_t> le16(int16_t v) {
  return {(uint8_t)(v & 0xFF), (uint8_t)((uint16_t)v >> 8)};
}

/// Answers all control requests until the host is idle
static void answerAll(TestHost& host, int16_t vol_min, int16_t vol_max) {
  for (int guard = 0; guard < 50 && host.mock.pending; guard++) {
    const auto& r = host.mock.requests.back().setup;
    if (r.bRequest == 0x82) {
      host.mock.respond(true, le16(vol_min));  // UAC1 GET_MIN
    } else if (r.bRequest == 0x83) {
      host.mock.respond(true, le16(vol_max));  // UAC1 GET_MAX
    } else {
      host.mock.respond(true);
    }
    host.serviceUSB();
  }
  assert(!host.mock.pending);
}

static bool hasRequest(TestHost& host, uint8_t type, uint8_t req,
                       uint16_t value, uint16_t index) {
  for (auto& r : host.mock.requests) {
    if (r.setup.bmRequestType == type && r.setup.bRequest == req &&
        r.setup.wValue == value && r.setup.wIndex == index)
      return true;
  }
  return false;
}

static void test_uac1_headset() {
  TestHost host;
  auto cfg = host.defaultConfig(RXTX_MODE);
  cfg.sample_rate = 48000;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  assert(host.begin(cfg));

  uint16_t len = host.events().onAttach(1, 0x0D8C, 0x013C, kUac1Headset.data(),
                                        (uint16_t)kUac1Headset.size());
  // the HID interface is not consumed
  assert(len == kUac1AudioLen);
  assert(host.uacVersion() == 1);
  assert(host.rxStream().present());
  assert(host.txStream().present());
  assert(host.rxStream().fu_id == 5);
  assert(host.txStream().fu_id == 2);
  assert(host.rxStream().fu_volume_mask == 0x01);

  host.events().onConfigured(1);
  host.serviceUSB();
  answerAll(host, -15360, 0);
  assert(host.idle());
  assert(host.mounted());
  assert(host.isStreamingActiveRx());
  assert(host.isStreamingActiveTx());

  // alternate settings and the sample rate of the speaker endpoint
  assert(hasRequest(host, 0x01, 0x0B, 1, 2));  // SET_INTERFACE mic
  assert(hasRequest(host, 0x01, 0x0B, 1, 1));  // SET_INTERFACE speaker
  assert(hasRequest(host, 0x22, 0x01, 0x0100, 0x01));  // SET_CUR rate EP 1
  assert(host.mock.opened.size() == 2);

  // the mic is mono: this is the reported format
  assert(host.audioInfoRx().channels == 1);
  assert(host.audioInfoRx().sample_rate == 48000);
  assert(host.audioInfoTx().channels == 2);
  assert(host.audioInfo().channels == 1);

  // initial mute/volume were sent to both feature units
  assert(hasRequest(host, 0x21, 0x01, 0x0100, 0x0500));  // mute FU 5
  assert(hasRequest(host, 0x21, 0x01, 0x0100, 0x0200));  // mute FU 2
  assert(hasRequest(host, 0x21, 0x01, 0x0200, 0x0500));  // volume FU 5

  // runtime volume: 0.5 maps to the middle of -60..0 dB
  host.mock.requests.clear();
  host.setVolume(0.5f);
  host.serviceUSB();
  assert(host.mock.pending);
  auto& r = host.mock.requests.back();
  assert(r.setup.wValue == 0x0200 && r.setup.wIndex == 0x0500);
  int16_t v = (int16_t)(r.out_data[0] | (r.out_data[1] << 8));
  assert(v == -7680);
  answerAll(host, 0, 0);
  assert(hasRequest(host, 0x21, 0x01, 0x0200, 0x0200));

  // microphone data path
  host.rx.resize(1000);
  uint8_t pkt[96];
  for (int j = 0; j < 96; j++) pkt[j] = (uint8_t)j;
  host.events().onIsoIn(UsbHostStreamRole::RxData, pkt, sizeof(pkt));
  assert(host.getRxTotalBytes() == 96);
  assert(host.available() == 96);

  // detach
  host.events().onDetach(1);
  assert(!host.mounted());
  assert(!host.isStreamingActive());
  printf("test_uac1_headset: ok\n");
}

static void test_uac1_tx_pacing_44100() {
  TestHost host;
  auto cfg = host.defaultConfig(TX_MODE);
  cfg.sample_rate = 44100;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  assert(host.begin(cfg));
  host.events().onAttach(1, 1, 2, kUac1Headset.data(),
                         (uint16_t)kUac1Headset.size());
  assert(!host.rxStream().present());  // RX is not enabled
  host.events().onConfigured(1);
  host.serviceUSB();
  answerAll(host, -15360, 0);
  assert(host.mounted());
  assert(host.audioInfoTx().sample_rate == 44100);
  // 44100 is set on the endpoint
  bool found = false;
  for (auto& r : host.mock.requests) {
    if (r.setup.bmRequestType == 0x22) {
      found = true;
      assert(r.out_data.size() == 3);
      uint32_t rate = r.out_data[0] | (r.out_data[1] << 8) | (r.out_data[2] << 16);
      assert(rate == 44100);
    }
  }
  assert(found);

  // fill the buffer so that the prime is passed
  std::vector<uint8_t> data(host.tx.size(), 0x11);
  host.tx.writeArray(data.data(), (int)data.size());
  uint8_t pkt[200];
  uint32_t total = 0;
  for (int j = 0; j < 1000; j++) {
    uint16_t n = host.events().onIsoOut(pkt, sizeof(pkt));
    assert(n == 176 || n == 180);
    assert(n % 4 == 0);
    total += n;
    // keep the buffer filled
    host.tx.writeArray(data.data(), n);
  }
  // exactly 44100 frames per second
  assert(total == 44100 * 4);
  assert(host.getTxUnderflowBytes() == 0);

  // explicit feedback overrides the nominal rate (10.14: 45 samples)
  host.events().onIsoIn(UsbHostStreamRole::Feedback,
                        (const uint8_t*)"\x00\x40\x0B", 3);
  assert(host.txStream().feedback == (45u << 16));
  printf("test_uac1_tx_pacing_44100: ok\n");
}

static void test_uac2_mic() {
  TestHost host;
  auto cfg = host.defaultConfig(RX_MODE);
  cfg.sample_rate = 44100;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  cfg.rx_start_fill_percent = 0;
  assert(host.begin(cfg));

  // the stack provides the descriptors starting at the AC interface
  const uint8_t* ac = kUac2Mic.data() + 8;
  uint16_t len = host.events().onAttach(2, 3, 4, ac,
                                        (uint16_t)(kUac2Mic.size() - 8));
  assert(len == kUac2Mic.size() - 8);
  assert(host.uacVersion() == 2);
  assert(host.rxStream().clock_id == 0x10);
  assert(host.rxStream().fu_id == 2);
  assert(host.rxStream().fu_mute_mask == 0x01);
  assert(host.rxStream().fu_volume_mask == 0x01);

  host.events().onConfigured(2);
  host.serviceUSB();
  // GET_RANGE header of the clock
  assert(host.mock.pending);
  assert(host.mock.requests.back().setup.bRequest == 0x02);
  assert(host.mock.requests.back().setup.wLength == 2);
  host.mock.respond(true, {0x02, 0x00});
  // full GET_RANGE: 44100 and 48000
  assert(host.mock.requests.back().setup.wLength == 26);
  std::vector<uint8_t> range = {0x02, 0x00};
  for (uint32_t rate : {44100u, 48000u}) {
    for (int k = 0; k < 2; k++)
      for (int b = 0; b < 4; b++) range.push_back((uint8_t)(rate >> (8 * b)));
    for (int b = 0; b < 4; b++) range.push_back(0);
  }
  host.mock.respond(true, range);
  // SET_CUR clock 44100
  auto& set_clock = host.mock.requests.back();
  assert(set_clock.setup.bmRequestType == 0x21);
  assert(set_clock.setup.wIndex == 0x1000);
  uint32_t rate = set_clock.out_data[0] | (set_clock.out_data[1] << 8) |
                  (set_clock.out_data[2] << 16);
  assert(rate == 44100);
  host.mock.respond(true);
  // SET_INTERFACE 1/1
  assert(host.mock.requests.back().is_set_interface);
  host.mock.respond(true);
  // FU volume GET_RANGE
  assert(host.mock.requests.back().setup.bRequest == 0x02);
  assert(host.mock.requests.back().setup.wIndex == 0x0200);
  host.mock.respond(true, {0x01, 0x00, 0x00, 0xA0, 0x00, 0x00, 0x00, 0x01});
  assert(host.rxStream().vol_min == (int16_t)0xA000);
  host.serviceUSB();
  answerAll(host, 0, 0);
  assert(host.mounted());
  assert(host.audioInfo().sample_rate == 44100);

  // sample rate change: alt 0, SET_CUR 48000, alt 1
  host.mock.requests.clear();
  AudioInfo info = host.audioInfo();
  info.sample_rate = 48000;
  host.setAudioInfo(info);
  host.serviceUSB();
  answerAll(host, 0, 0);
  assert(host.mock.requests.size() >= 3);
  assert(host.mock.requests[0].is_set_interface &&
         host.mock.requests[0].setup.wValue == 0);
  assert(hasRequest(host, 0x21, 0x01, 0x0100, 0x1000));
  assert(host.mock.requests.back().is_set_interface ||
         host.mock.requests.back().setup.bRequest == 0x01);
  assert(host.audioInfo().sample_rate == 48000);
  assert(host.isStreamingActiveRx());

  // readBytes returns the received data
  uint8_t pkt[192];
  memset(pkt, 7, sizeof(pkt));
  host.events().onIsoIn(UsbHostStreamRole::RxData, pkt, sizeof(pkt));
  uint8_t out[256];
  assert(host.readBytes(out, sizeof(out)) == 192);
  assert(out[0] == 7);

  // end() keeps the attached device: begin() opens it again
  host.end();
  assert(!host.mounted());
  assert(!host.isStreamingActive());
  host.mock.requests.clear();
  assert(host.begin());
  host.serviceUSB();
  answerAll(host, 0, 0);
  assert(host.mounted());
  assert(hasRequest(host, 0x01, 0x0B, 1, 1));
  printf("test_uac2_mic: ok\n");
}

static void test_vid_pid_filter() {
  TestHost host;
  auto cfg = host.defaultConfig(RX_MODE);
  cfg.host_vid = 0x1234;
  assert(host.begin(cfg));
  assert(host.events().onAttach(1, 0x0D8C, 1, kUac1Headset.data(),
                                (uint16_t)kUac1Headset.size()) == 0);
  printf("test_vid_pid_filter: ok\n");
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  test_uac1_headset();
  test_uac1_tx_pacing_44100();
  test_uac2_mic();
  test_vid_pid_filter();
  printf("all tests passed\n");
  return 0;
}
