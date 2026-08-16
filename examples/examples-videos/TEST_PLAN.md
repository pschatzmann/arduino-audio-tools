# Video Functionality Test Plan

Two-board hardware setup:

- **Board A — ESP32-S3 + PSRAM + camera**: the capture/encode/mux/serve side.
- **Board B — ESP32 (no PSRAM) + TFT display**: the receive/demux/decode/display side.

This plan sequences testing from simplest (one board, no network) to full
integration (both boards talking over HTTP), then stress/edge cases, then
lists functionality that has **no existing example** to exercise it yet.

---

## 0. Prerequisites

### Board A (ESP32-S3 + PSRAM + camera)
- Arduino settings: **PSRAM: OPI/QSPI PSRAM** (required for camera frame
  buffers), **USB CDC on Boot: Enabled** (to see `Serial` output).
- Camera wired per `examples/examples-custom-boards/esp32s3-mic-cam` (or
  update the `CAMERA_PIN_*` defines for your own board/module).
- Libraries: `esp32-camera` (bundled with the ESP32 board package),
  [TinyH264](https://github.com/pschatzmann/TinyH264), optionally
  [ESP32S3-h264](https://github.com/pschatzmann/ESP32S3-h264) for the
  hardware encode/decode path (S3-only).

### Board B (ESP32, no PSRAM + display)
- TFT display supported by [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
  (configure pins/driver in that library's `User_Setup.h` — not done in any
  sketch). If instead testing the `OutputTinyGPU` path, use a
  [TinyGPU](https://github.com/pschatzmann/TinyGPU)-supported driver
  (e.g. `ILI9341Driver`) instead of TFT_eSPI.
- Libraries: TFT_eSPI, [JPEGDecoder](https://github.com/Bodmer/JPEGDecoder),
  TinyH264, SD (for local-file tests).
- SD card (FAT32) with a **faststart** test MP4:
  ```
  ffmpeg -i in.mp4 -c:v libx264 -c:a aac -movflags +faststart out.mp4
  ```
  DemuxerMP4 is forward-only streaming and requires `moov` before `mdat`.
- No PSRAM: this board is exactly where memory regressions show up first —
  see Suite 4's heap-watermark checks.

### Both boards
- Same WiFi network for the client/server (Suite 3) tests.
- Fill in real `ssid`/`password`/`video_url` placeholders in each sketch
  before flashing.

---

## 1. Suite 1 — Board B standalone playback (no network, start here)

No camera or WiFi needed; SD card only. Confirms demux → decode → display
→ (optionally) audio, in isolation, before adding network complexity.

| # | Sketch | Exercises | Status |
|---|--------|-----------|--------|
| 1.1 | `mp4/sd-mp4-audio/sd-mp4-audio.ino` | `DemuxerMP4`, AAC audio only (video track ignored) | ✅ compiles |
| 1.2 | `mp4/sd-mp4-video/sd-mp4-video.ino` | `DemuxerMP4` (via `CodecCopy`), `H264Decoder`, `OutputTFT_eSPI`, video only | ✅ compiles (fixed a pre-existing bug this session: `setup()` never called `SD.begin()`/`SD.open()`, so `file` stayed unopened and the sketch never actually played anything) |
| 1.3 | `mp4/sd-mp4-audio-video/sd-mp4-audio-video.ino` | Combined: `DemuxerMP4` dispatch to both `H264Decoder`→`OutputTFT_eSPI` and AAC→I2S — the real A/V sync test | ✅ compiles (verified via `arduino-cli`) |
| 1.4 | `avi/sd-avi-video/sd-avi-video.ino` | `DemuxerAVI` (via `CodecCopy`), `H264Decoder`, `OutputTFT_eSPI`, video only — new this session | ✅ compiles (verified via `arduino-cli`) |
| 1.5 | `mpg/sd-mpg-video/sd-mpg-video.ino` | `DemuxerMPG` (via `CodecCopy`), `MPGDecoder`, `OutputTFT_eSPI`, video only — new this session | ✅ compiles (verified via `arduino-cli`) |

**Checklist per sketch** (see §6 for the full template):
- [ ] Compiles for Board B's FQBN
- [ ] Flashes, boots without panic
- [ ] Picture renders — correct colors (no R/B channel swap), correct
      orientation, no tearing
- [ ] 1.3 only: audio and video stay in sync over the full clip, not just
      the first few seconds
- [ ] Clean end-of-file handling (`file.close()`, no hang/reboot)

Run 1.1 before 1.2/1.3 — it isolates the audio decode path so an A/V sync
problem in 1.3 can be attributed to video, not audio. 1.4/1.5 are
video-only, same shape as 1.2, just swapping the container/codec — useful
to confirm a decode problem is codec/container-specific rather than a
`OutputTFT_eSPI`/display issue common to all three.

---

## 2. Suite 2 — Board A standalone capture/serve

Confirms camera → (encode) → mux → HTTP works before a second device is in
the loop. A desktop browser/VLC/ffplay can substitute for Board B here to
isolate Board A problems from Board B problems.

| # | Sketch | Exercises | Status |
|---|--------|-----------|--------|
| 2.0 | `examples-custom-boards/esp32s3-mic-cam/camera/camera.ino` | Raw camera bring-up only, no codec — do this first if the camera is new/untested | ✅ compiles (verified) |
| 2.1 | `avi/http-server-avi/http-server-avi.ino` | Camera(JPEG) → `MuxerAVI` (MJPEG passthrough, no video encoder) → `AudioServer` HTTP | ✅ compiles (verified) |
| 2.2 | `avi/http-server-avi-h264/http-server-avi-h264.ino` | Camera(YUV422) → `H264Encoder` (TinyH264, software) → `MuxerAVI` → HTTP | ✅ compiles (fixed a pre-existing bug this session: `H264Encoder<> h264Encoder;` treated the non-template `H264Encoder` as a template — only `H264EncoderESP32S3` is templated) |
| 2.2b | Same file, swap `H264Encoder`→`H264EncoderESP32S3` | Hardware `esp_h264` encode path (S3-only) | not compile-verified — requires editing the sketch to swap the class |
| 2.3 | `mpg/http-server-mpg/http-server-mpg.ino` | Camera(YUV422) → `MPGEncoder` (TinyMPG, MPEG-1) → `MuxerMPG` (self-contained Program Stream, no AVI/MP4 wrapper) → HTTP — new this session | ✅ compiles (verified via `arduino-cli`) |

**Checklist**:
- [ ] `esp_camera_init()` succeeds (check Serial log)
- [ ] `vlc http://<esp32-ip>/` (or ffplay) connects and shows live video
- [ ] Frame rate roughly matches `video_fps` (10 by default)
- [ ] Client disconnect leaves Board A ready for the next client (loop
      back to `esp_camera_fb_get()`, no crash/leak)
- [ ] 2.2 vs 2.2b: compare software vs. hardware encode CPU headroom /
      frame rate if both are tested

---

## 3. Suite 3 — Board A → Board B integration (the main event)

Both boards on the same network, Board A serving, Board B consuming.

| # | Server (Board A) | Client (Board B) | Path |
|---|---|---|---|
| 3.1 | `avi/http-server-avi/http-server-avi.ino` | `avi/http-client-avi-tft/http-client-avi-tft.ino` | MJPEG-in-AVI end-to-end: `DemuxerAVI` → `JPEGOutputTFT` — both ends ✅ compile (verified) |
| 3.2 | `avi/http-server-avi-h264/http-server-avi-h264.ino` | `avi/http-client-avi-h264/http-client-avi-h264.ino` | H.264-in-AVI end-to-end: `DemuxerAVI` → `H264Decoder` → `OutputTFT_eSPI` — ✅ client compiles (verified, 97% flash on esp32s3 — tight; recheck on Board B's actual FQBN/partition table) |
| 3.3 | *(gap — see §5)* | `mp4/http-client-mp4/http-client-mp4.ino` | H.264+AAC-in-MP4 over HTTP: `DemuxerMP4` → `H264Decoder`/AAC — ✅ client compiles (verified). No MP4-serving example exists yet (Board A only mux to AVI/MPG today) — workaround below. |
| 3.4 | `mpg/http-server-mpg/http-server-mpg.ino` | `mpg/http-client-mpg/http-client-mpg.ino` | MPEG-1 Program Stream end-to-end: `DemuxerMPG` → `MPGDecoder` → `OutputTFT_eSPI` — new this session, both ends ✅ compile (verified) |

**3.3 workaround** (no MP4 server example exists): serve a static
faststart MP4 from a desktop machine instead of Board A —
`python3 -m http.server` in the directory holding the test file, point
`video_url` at `http://<desktop-ip>:8000/out.mp4`. This still fully
exercises Board B's `DemuxerMP4` HTTP-client path; it just doesn't
exercise Board A's (currently nonexistent) MP4 muxing.

**Checklist**:
- [ ] Board B connects and renders within a few seconds
- [ ] No visible artifacts distinct from Suite 1 (rules out anything
      network-specific: dropped frames, stalls, resyncing after packet loss)
- [ ] Reconnect after Board A restarts (WiFi AP still up) recovers cleanly
- [ ] 3.1 vs 3.2: compare MJPEG (simpler, larger frames) against H.264
      (smaller frames, decode cost) for frame rate and visual quality

---

## 4. Suite 4 — Edge cases and stress

Focused on Board B given its RAM constraint (no PSRAM), and network
robustness on both.

- [ ] **Heap watermark, Board B**: log `ESP.getFreeHeap()` at boot, after
      `begin()`, and every N frames during a 10+ minute continuous playback
      (any Suite 1 or 3 sketch). Free heap should plateau, not trend down —
      a steady decline means a leak (check `H264Decoder`'s `frame_buffer`/
      `Vector` growth logic, and confirm `OutputTinyGPU`/`OutputTFT_eSPI`
      aren't accidentally re-allocating per frame).
- [ ] **Oversized/undersized frame into `OutputTinyGPU`**: feed a `write()`
      call with `len` smaller than `width*height*2` — confirm it logs and
      returns 0 (the bounds check added recently) rather than asserting/
      crashing.
- [ ] **Non-faststart MP4** (`moov` after `mdat`) fed to `DemuxerMP4`:
      confirm it fails/stalls gracefully with a log message, not a crash —
      this is a documented hard requirement, worth confirming it fails
      loud rather than silent.
- [ ] **Truncated/corrupt AVI or MP4** (chop a valid file mid-stream):
      same graceful-failure check on `DemuxerAVI`/`DemuxerMP4`.
- [ ] **WiFi drop mid-stream** (Suite 3): power-cycle the AP or walk Board B
      out of range — confirm Board B doesn't hang forever in `copier.copy()`
      and Board A's `sendVideo()` loop exits (`server.isClientConnected()`
      goes false) so the next client can connect.
- [ ] **Long soak, Board A**: leave a client connected for 30+ minutes,
      confirm camera capture doesn't stall (`esp_camera_fb_get()` returning
      `nullptr` repeatedly) and PSRAM doesn't fragment/exhaust.
- [ ] **Rapid connect/disconnect, Board A**: several short-lived clients in
      a row — confirms `MuxerAVI`/`H264Encoder` state resets cleanly between
      `sendVideo()` calls (nothing left over from the previous client).

---

## 5. Gaps — functionality with no existing example (stretch goals)

These are real, tested-by-compile library features with **zero example
sketch** exercising them today. Not blocking for the plan above, but worth
knowing about if you want fuller coverage:

*Closed this session:* `ContainerMPG`/`MPGEncoder`/`MPGDecoder` now has
full example coverage (2.3, 3.4, 1.5) — see the tables above. Also fixed a
gap it hit along the way: `MPGDecoder` didn't implement `VideoInfoSource`
(only `H264Decoder` did), so it couldn't be passed to
`OutputTFT_eSPI::setVideoInfoSource()` — added.

- **`MuxerMP4`** (write H.264+AAC to MP4) — no example writes MP4 at all;
  only reading (`DemuxerMP4`) is exercised. Would need a new Board A sketch
  (camera → `H264Encoder` → `MuxerMP4` → SD or HTTP), which would also
  close the Suite 3.3 gap above.
- **`VideoMuxer` / `VideoMuxerWithTasks` / `CameraFrameSource`** — the
  higher-level "pull frames on a timer, mux automatically" API layer has no
  example at all (`http-server-avi*.ino` both drive the camera→encoder→muxer
  loop by hand instead). Worth a smoke test if you plan to rely on this API.
- **`ContainerBinary` carrying video** — existing `ContainerBinary`
  examples (`examples/tests/codecs/test-container-binary*`) are audio-only;
  its video path (`setOutputVideo`/`outputVideo()`) is untested by any
  sketch.
- **`H264DecoderESP32S3`** (hardware decode) — S3-only, so only testable on
  Board A itself, which has no display in this setup. Without a screen,
  the best available check is headless: decode a known clip and confirm no
  `hasError()`/crash and correct reported `videoInfo()` dimensions, logged
  over Serial rather than viewed.
- **`OutputTinyGPU`** end-to-end — the class itself is compile-verified
  (including the zero-copy `write()` path), but no example sketch wires it
  into a full decode pipeline the way `OutputTFT_eSPI`/`JPEGOutputTFT` are
  in Suites 1/3. Only relevant if Board B's display uses a TinyGPU driver
  instead of TFT_eSPI.
- **`OutputOpenCV`** — desktop-only (OpenCV), not applicable to either
  board; skip unless you also want a desktop-side sanity check.

---

## 6. Per-test checklist template

Copy this for any sketch not already covered by a more specific checklist
above:

- [ ] Compiles clean for the target board (`arduino-cli compile --fqbn ...`)
- [ ] Flashes and boots without crash/panic (watch Serial at boot)
- [ ] Video renders: no black screen, correct colors (RGB/BGR swap is the
      most common symptom), correct orientation/rotation
- [ ] Frame rate roughly matches the source; no worse-than-expected stutter
- [ ] Audio present and in sync with video, if applicable
- [ ] No reboot or heap exhaustion over a 2+ minute sustained run
- [ ] Clean end-of-stream handling (file/connection closes without hang)
- [ ] Serial log free of repeated ERROR/WARN spam

---

## Recommended order

1. **Suite 1** (Board B alone — SD card, no network, isolates decode/display)
2. **Suite 2** (Board A alone — camera/encode/serve, use a desktop viewer)
3. **Suite 3** (both boards together — the real integration test)
4. **Suite 4** (stress/edge cases, once the happy path is solid)
5. **Suite 5 / gaps** (optional — only if you need that specific coverage)
