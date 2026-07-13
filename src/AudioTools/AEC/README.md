# AEC: Automatic Echo Cancellation

Header-only echo cancellation building blocks: two adaptive echo cancellers,
so a device doesn't hear its own speaker output as "user input". 
This file documents the two end-user-facing classes, what they're good for,
and where their limits are.

Automated tests live in [`tests-cmake/stt`](../../../tests-cmake/stt) and
build/run on desktop via CMake (`AudioRealFFT` + a headless `MemoryStream`,
no hardware required).

| Class | File | Algorithm | Needs FFT? |
|---|---|---|---|
| [`LMSEchoCancellationStream`](#lmsechocancellationstream) | `LMSEchoCancellationStream.h` | Time-domain adaptive FIR (LMS) | No |
| [`MDFEchoCancellationStream`](#mdfechocancellationstream) | `MDFEchoCancellationStream.h` + `MDFEchoCancellation.h` | Block-frequency-domain NLMS (speexdsp MDF, two-path) | Yes |

`MDFEchoCancellationConfig.h` is not a class — it's compile-time tuning for
`MDFEchoCancellation.h` (`TWO_PATH`, `PLAYBACK_DELAY`, and a `FIXED_POINT`
toggle that **must stay disabled**; see the comment in that file for why).

---

## LMSEchoCancellationStream

```cpp
template <typename T = int16_t>
class LMSEchoCancellationStream : public AudioStream;
```

A small, dependency-free adaptive echo canceller: a single-precision LMS FIR
filter estimates the echo path from speaker to microphone and subtracts the
estimate from the mic signal. No FFT, no dynamic block processing — just a
per-sample filter update.

```cpp
LMSEchoCancellationStream<int16_t> aec(mic_in, /*lag=*/0, /*buffer_size=*/512,
                                        /*filter_len=*/32, /*mu=*/0.01f);
// speaker path: whatever you send to the speaker, also feed here
aec.write(speaker_buf, speaker_bytes);
// mic path: read the echo-cancelled signal
aec.readBytes(clean_mic_buf, len);
```

`write()` buffers the playback (speaker) signal into an internal sliding
delay line; `readBytes()` pulls mic samples from the wrapped `Stream` and
filters each one against that delay line. There's also a direct,
Stream-free API — `cancel(rec, play, out, len)` — for feeding both signals
explicitly in lock-step, useful for testing or non-duplex pipelines.

**Strengths**
- Tiny and simple: `O(filter_len)` work per sample, no FFT, trivial memory
  footprint — a reasonable fit for small microcontrollers with a short,
  simple echo path (e.g. a small speaker directly coupled to a nearby mic).
- The delay line is a real sliding window (fixed-size ring buffer that
  evicts as it fills), so it keeps adapting indefinitely rather than
  stalling after some fixed number of samples.
- `write()`/`readBytes()` and the direct `cancel()` API are verified to
  produce identical output for the same input, so you can prototype/tune
  against `cancel()` and trust the Stream-based path behaves the same.

**Weaknesses**
- Plain LMS with a fixed step size (`mu`), not NLMS — the right `mu` depends
  on your signal amplitude and echo strength, and needs to be tuned per
  deployment; too large diverges, too small converges too slowly to track a
  changing echo path.
- No double-talk protection. If someone talks into the mic *while* the
  speaker is playing, plain LMS has no way to tell "this is near-end speech"
  apart from "the echo path just changed", and can adapt in the wrong
  direction. (The MDF canceller below has a two-path foreground/background
  scheme specifically for this.)
- Only models a short, simple echo path well: `filter_len` must cover the
  actual echo delay in samples, and cost grows linearly with it — this is
  not the right tool for a long, reverberant echo tail.
- No sample-rate/timing compensation: `write()` (speaker) and `readBytes()`
  (mic) need to be driven at the same pace for the delay line to stay
  correctly time-aligned; there's no internal resampling or drift
  correction for a mismatched full-duplex pipeline.

---

## MDFEchoCancellationStream

```cpp
class MDFEchoCancellationStream : public AudioStream;
```

A header-only port of the audio-processing core of speexdsp's MDF
(Multi-Delay block Frequency) adaptive echo canceller — a block, frequency-
domain NLMS-style filter with a two-path (foreground/background) filter
switch for robustness against double-talk. `MDFEchoCancellationStream` is the
`Stream` wrapper; `MDFEchoCancellation` (in `MDFEchoCancellation.h`) is the
underlying algorithm and can be used directly (`capture()`/`playback()`) if
you don't want the `Stream` plumbing. The (fairly large) echo-state buffers
are allocated through the `audio_tools::Allocator` abstraction (see
`AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h`), defaulting to
the shared `DefaultAllocator`; pass a different one (e.g. `AllocatorPSRAM`
on ESP32) as the last constructor argument to place them elsewhere.

```cpp
AudioRealFFT fft;                 // dedicate this driver to the canceller (see below)
auto cfg = fft.defaultConfig(RXTX_MODE);
cfg.length = 256;                 // frame size in samples
cfg.sample_rate = 16000;
fft.begin(cfg);

MDFEchoCancellationStream aec(mic_in, /*filterLength=*/1024, fft);
aec.write(speaker_buf, frame_bytes);      // exactly one frame's worth
aec.readBytes(clean_mic_buf, frame_bytes); // exactly one frame's worth
```

**Strengths**
- A meaningfully stronger algorithm than the LMS canceller: block-frequency
  processing handles much longer echo tails without the linear-in-taps cost
  of a time-domain filter, and the two-path filter gives it a real (if
  simply-tuned) defense against double-talk.
- Multi-channel support (`nbMic` x `nbSpeakers`) is built into the core
  algorithm via a second constructor.
- After the fixes below, verified to actually converge: on a synthetic
  attenuated-echo signal (no near-end noise), residual echo energy drops to
  roughly 1e-7 of the original within a few hundred frames. See
  `test_mdf_converges` in `tests-cmake/stt/stt_test.cpp`.

**Weaknesses / things to know before relying on this**
- This was, by a wide margin, the most bug-dense file in the folder — it did
  not compile, and once it did, several more bugs kept it from ever
  attenuating any echo. All of the following are now fixed and covered by
  tests, but the density of issues is worth knowing about if you're
  extending this code further: a missing forward declaration for the FFT
  helper templates (compile failure), a duplicate constructor signature
  (compile failure), `write()`/`readBytes()` calling nonexistent/mismatched
  methods on the underlying canceller, `echo_word16_t`/`echo_word32_t`
  hardcoded to `int16_t`/`int32_t` while the algorithm body is written in
  plain float arithmetic (silently truncating fractional state to zero), a
  `0/0 = NaN` in the leak-estimate calculation, and an `(int32_t)` cast left
  over from true fixed-point mode that truncated the filter's entire
  gradient signal to whole numbers (usually zero). Getting real
  attenuation additionally required two fixes in the **shared**
  `AudioRealFFT.h` driver (bin extraction reading the wrong internal buffer,
  and the FFT engine not being reinitialized when re-`begin()`'d at a
  different size) — see that file's history for detail.
- **Only verified with the `AudioRealFFT` driver.** Other `AudioFFTBase`
  drivers (`AudioKissFFT`, `AudioCmsisFFT`, `AudioESP32FFT`,
  `AudioEspressifFFT`, `AudioFixedFFT`) have their own, separate bin-access
  implementations that have **not** been audited the way `AudioRealFFT` was.
  Using one of them here is unverified territory.
- `write()`/`readBytes()` must be called with buffers that are exact
  multiples of the frame size (`AudioFFTBase::config().length`); any
  trailing partial frame is silently dropped rather than buffered for next
  time, so feed frame-aligned chunks.
- Constructing (really: first using) this class re-`begin()`s the FFT driver
  you pass in at a *different* size (2x the configured frame size) than
  whatever you configured it with. Dedicate that `AudioFFTBase` instance to
  the canceller — don't share it with other FFT work, and don't call
  `begin()` on it again afterwards.
- The two-path switching thresholds and adaptation heuristics are the
  original speexdsp defaults, unmodified and only exercised here against a
  single synthetic tone with no near-end noise. Real acoustic echo (multi-
  tap reverberant paths, concurrent near-end speech, non-stationary noise)
  is not yet validated — treat convergence quality on real audio as
  unproven until you test it yourself.
