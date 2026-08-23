/// Synthetic test for DemuxerMP4::quickStart(): builds a minimal, valid
/// "moov-at-end" MP4 (mdat before moov - the layout most tools produce
/// unless asked for `-movflags +faststart`) entirely in memory, and checks
/// that quickStart() locates + parses 'moov' directly (without streaming
/// through 'mdat' first), repositions the source at 'mdat', and that
/// normal sequential playback from there dispatches the right audio
/// samples. A second, "faststart" (moov before mdat) build is used as a
/// control to confirm quickStart() is a no-op there and playback still
/// works via the plain sequential path.

#include <assert.h>
#include <cstring>
#include <vector>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"

using namespace audio_tools;

using Bytes = std::vector<uint8_t>;

static void appendU32BE(Bytes& v, uint32_t x) {
  v.push_back((uint8_t)(x >> 24));
  v.push_back((uint8_t)(x >> 16));
  v.push_back((uint8_t)(x >> 8));
  v.push_back((uint8_t)x);
}
static void appendStr(Bytes& v, const char* s) {
  for (int i = 0; i < 4; i++) v.push_back((uint8_t)s[i]);
}
static void appendBytes(Bytes& v, const Bytes& b) {
  v.insert(v.end(), b.begin(), b.end());
}

static Bytes box(const char* type, const Bytes& payload) {
  Bytes b;
  appendU32BE(b, (uint32_t)(8 + payload.size()));
  appendStr(b, type);
  appendBytes(b, payload);
  return b;
}

static Bytes buildFtyp() {
  Bytes p;
  appendStr(p, "isom");
  appendU32BE(p, 0);
  appendStr(p, "isom");
  return box("ftyp", p);
}

static Bytes buildHdlrAudio() {
  Bytes p;
  appendU32BE(p, 0);  // version/flags
  appendU32BE(p, 0);  // pre_defined
  appendStr(p, "soun");
  return box("hdlr", p);
}

static Bytes buildMdhd(uint32_t timescale) {
  Bytes p;
  appendU32BE(p, 0);          // version/flags
  appendU32BE(p, 0);          // creation
  appendU32BE(p, 0);          // modification
  appendU32BE(p, timescale);  // timescale
  appendU32BE(p, 0);          // duration
  return box("mdhd", p);
}

static Bytes buildMp4a(uint16_t channels, uint32_t sampleRate) {
  Bytes p(28, 0);
  p[7] = 1;  // data_reference_index = 1
  p[16] = (uint8_t)(channels >> 8);
  p[17] = (uint8_t)channels;
  p[19] = 16;  // samplesize = 16
  uint32_t sr = sampleRate << 16;
  p[24] = (uint8_t)(sr >> 24);
  p[25] = (uint8_t)(sr >> 16);
  p[26] = (uint8_t)(sr >> 8);
  p[27] = (uint8_t)sr;
  return box("mp4a", p);
}

static Bytes buildStsd(const Bytes& mp4aBox) {
  Bytes p;
  appendU32BE(p, 0);  // version/flags
  appendU32BE(p, 1);  // entry_count
  appendBytes(p, mp4aBox);
  return box("stsd", p);
}

static Bytes buildStsz(const std::vector<uint32_t>& sizes) {
  Bytes p;
  appendU32BE(p, 0);  // version/flags
  appendU32BE(p, 0);  // sampleSize = 0 -> variable, per-entry sizes below
  appendU32BE(p, (uint32_t)sizes.size());
  for (auto s : sizes) appendU32BE(p, s);
  return box("stsz", p);
}

static Bytes buildStsc(uint32_t samplesPerChunk) {
  Bytes p;
  appendU32BE(p, 0);
  appendU32BE(p, 1);  // entry_count
  appendU32BE(p, 1);  // first_chunk
  appendU32BE(p, samplesPerChunk);
  appendU32BE(p, 1);  // sample_description_index
  return box("stsc", p);
}

static Bytes buildStco(uint32_t chunkOffset) {
  Bytes p;
  appendU32BE(p, 0);
  appendU32BE(p, 1);  // entry_count
  appendU32BE(p, chunkOffset);
  return box("stco", p);
}

static Bytes buildStts(uint32_t sampleCount, uint32_t sampleDelta) {
  Bytes p;
  appendU32BE(p, 0);
  appendU32BE(p, 1);  // entry_count
  appendU32BE(p, sampleCount);
  appendU32BE(p, sampleDelta);
  return box("stts", p);
}

/// Builds the full 'moov' tree for one audio (AAC/mp4a) track whose single
/// chunk (all samples) starts at 'chunkOffset' in the final file.
static Bytes buildMoov(uint32_t chunkOffset,
                       const std::vector<uint32_t>& sampleSizes) {
  Bytes stsd = buildStsd(buildMp4a(1, 44100));
  Bytes stsz = buildStsz(sampleSizes);
  Bytes stsc = buildStsc((uint32_t)sampleSizes.size());
  Bytes stco = buildStco(chunkOffset);
  Bytes stts = buildStts((uint32_t)sampleSizes.size(), 1000);

  Bytes stblPayload;
  appendBytes(stblPayload, stsd);
  appendBytes(stblPayload, stsz);
  appendBytes(stblPayload, stsc);
  appendBytes(stblPayload, stco);
  appendBytes(stblPayload, stts);
  Bytes stbl = box("stbl", stblPayload);

  Bytes minf = box("minf", stbl);

  Bytes mdiaPayload;
  appendBytes(mdiaPayload, buildHdlrAudio());
  appendBytes(mdiaPayload, buildMdhd(44100));
  appendBytes(mdiaPayload, minf);
  Bytes mdia = box("mdia", mdiaPayload);

  Bytes trak = box("trak", mdia);
  return box("moov", trak);
}

static Bytes buildMdat(const std::vector<Bytes>& samples) {
  Bytes payload;
  for (auto& s : samples) appendBytes(payload, s);
  return box("mdat", payload);
}

/// Minimal in-memory stand-in for Arduino's File, matching just the surface
/// FileSeekableSource<FileT, WriterT> needs (duck-typed template).
struct MemFile {
  Bytes data;
  size_t pos = 0;
  size_t position() { return pos; }
  bool seek(size_t p) {
    if (p > data.size()) return false;
    pos = p;
    return true;
  }
  size_t readBytes(char* buf, size_t len) {
    size_t avail = data.size() - pos;
    size_t n = std::min(len, avail);
    memcpy(buf, data.data() + pos, n);
    pos += n;
    return n;
  }
};

/// Captures how many bytes/calls the audio output received, without
/// needing a real decoder.
class TestAudioSink : public Print {
 public:
  size_t write(uint8_t) override {
    total_bytes++;
    return 1;
  }
  size_t write(const uint8_t* buf, size_t len) override {
    total_bytes += len;
    calls++;
    return len;
  }
  size_t total_bytes = 0;
  int calls = 0;
};

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  std::vector<uint32_t> sizes = {10, 10, 10};
  std::vector<Bytes> samples;
  for (size_t i = 0; i < sizes.size(); i++) {
    samples.push_back(Bytes(sizes[i], (uint8_t)(0xA0 + i)));
  }

  // ---- 1) moov-at-end (non-faststart), auto-quickStart disabled: calls
  // DemuxerMP4::quickStart() directly, to check its own precise contract
  // (exact resulting file position, track info already available before
  // any 'mdat' byte streams) independent of FileSeekableSource's wiring ----
  {
    Bytes ftyp = buildFtyp();
    Bytes mdat = buildMdat(samples);
    uint32_t chunkOffset = (uint32_t)(ftyp.size() + 8);  // mdat's payload start
    Bytes moov = buildMoov(chunkOffset, sizes);

    MemFile file;
    appendBytes(file.data, ftyp);
    appendBytes(file.data, mdat);
    appendBytes(file.data, moov);

    uint64_t mdat_offset = ftyp.size();

    DemuxerMP4 demuxer;
    TestAudioSink audioSink;
    // quickStart=false: FileSeekableSource must NOT also auto-trigger
    // quickStart() here - a second call would re-parse 'moov' into a
    // duplicate Track.
    FileSeekableSource<MemFile, DemuxerMP4> src(file, demuxer, 1024, false);
    demuxer.setOutputAudio(audioSink);
    demuxer.begin();

    bool quick = demuxer.quickStart();
    assert(quick == true);
    // quickStart() must leave the file positioned exactly at 'mdat', not 0
    assert(file.position() == mdat_offset);
    // moov was parsed directly - track info must already be available
    // before any 'mdat' byte has been streamed
    assert(demuxer.getAudioInfo().format == AudioFormat::AAC);
    assert(demuxer.getAudioInfo().channels == 1);
    assert(demuxer.getAudioInfo().sample_rate == 44100);

    while (src.copy() > 0) {
    }

    // dispatchAudio() prepends a synthesized 7-byte ADTS header to each
    // 10-byte sample, as two separate write() calls
    assert(audioSink.total_bytes == 3 * (7 + 10));
    assert(audioSink.calls == 3 * 2);
    Serial.println("moov-at-end quickStart (manual, auto-disabled) test passed");
  }

  // ---- 1b) moov-at-end, default FileSeekableSource behavior: no manual
  // quickStart() call at all - the first copy() call must trigger it
  // automatically ----
  {
    Bytes ftyp = buildFtyp();
    Bytes mdat = buildMdat(samples);
    uint32_t chunkOffset = (uint32_t)(ftyp.size() + 8);
    Bytes moov = buildMoov(chunkOffset, sizes);

    MemFile file;
    appendBytes(file.data, ftyp);
    appendBytes(file.data, mdat);
    appendBytes(file.data, moov);

    DemuxerMP4 demuxer;
    TestAudioSink audioSink;
    // default constructor - quickStart defaults to true, no explicit
    // demuxer.quickStart() call anywhere in this scope
    FileSeekableSource<MemFile, DemuxerMP4> src(file, demuxer);
    demuxer.setOutputAudio(audioSink);
    demuxer.begin();

    while (src.copy() > 0) {
    }

    assert(demuxer.getAudioInfo().format == AudioFormat::AAC);
    assert(demuxer.getAudioInfo().channels == 1);
    assert(demuxer.getAudioInfo().sample_rate == 44100);
    assert(audioSink.total_bytes == 3 * (7 + 10));
    assert(audioSink.calls == 3 * 2);
    Serial.println("moov-at-end auto-quickStart (default) test passed");
  }

  // ---- 2) faststart control (moov before mdat), default (auto-quickStart
  // enabled) FileSeekableSource: quickStart() must be a no-op there (no
  // manual call at all here - relies on the default auto-trigger, same as
  // scenario 1b) and normal sequential playback must still work exactly
  // as before ----
  {
    Bytes ftyp = buildFtyp();
    // two-pass: build once to learn moov's size, then rebuild with the
    // real chunk offset (same size either way - only a field VALUE changes)
    Bytes moovForSize = buildMoov(0, sizes);
    uint32_t chunkOffset =
        (uint32_t)(ftyp.size() + moovForSize.size() + 8);  // mdat payload start
    Bytes moov = buildMoov(chunkOffset, sizes);
    assert(moov.size() == moovForSize.size());
    Bytes mdat = buildMdat(samples);

    MemFile file;
    appendBytes(file.data, ftyp);
    appendBytes(file.data, moov);
    appendBytes(file.data, mdat);

    DemuxerMP4 demuxer;
    TestAudioSink audioSink;
    FileSeekableSource<MemFile, DemuxerMP4> src(file, demuxer);
    demuxer.setOutputAudio(audioSink);
    demuxer.begin();

    while (src.copy() > 0) {
    }

    assert(demuxer.getAudioInfo().format == AudioFormat::AAC);
    assert(audioSink.total_bytes == 3 * (7 + 10));
    assert(audioSink.calls == 3 * 2);
    Serial.println("faststart control (auto-quickStart no-op) test passed");
  }

  Serial.println("All DemuxerMP4::quickStart tests passed");
  exit(0);
}

void loop() {}
