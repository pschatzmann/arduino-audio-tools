/* Copyright (C) 2003-2008 Jean-Marc Valin
 * Copyright (C) 2024 Phil Schatzmann (Header-only adaptation)
 *
 * Echo canceller configuration constants
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstdint>

namespace audio_tools {

// ============================================================================
// Feature Flags
// ============================================================================

/** Enable fixed-point arithmetic for embedded systems
 * When defined, uses integer math instead of floating-point for better
 * performance on processors without FPU.
 */
#define FIXED_POINT  // Disabled - floating-point mode is currently supported

/** Enable two-path filter for improved double-talk robustness
 * When enabled, the echo canceller maintains both a foreground and background
 * filter, allowing it to better handle situations where near-end and far-end
 * speech occur simultaneously (double-talk).
 */
#define TWO_PATH

// ============================================================================
// Echo Canceller Tuning Parameters
// ============================================================================


/** Playback buffer delay in frames (2)
 * Number of frames to buffer the playback signal before processing with the
 * captured signal. This compensates for typical system latencies.
 */
#define PLAYBACK_DELAY 2

// ============================================================================
// Mathematical Constants
// ============================================================================

#ifndef M_PI
/** Mathematical constant Pi (3.14159265358979323846) */
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Floating Point Mode Constants
// ============================================================================

#ifdef FIXED_POINT

/** Fixed-point zero constant (Q15 format) */
#define FLOAT_ZERO {0, 0}

/** Fixed-point one constant (Q15 format: 32767, exp 0) */
#define FLOAT_ONE {32767, 0}

/** Fixed-point half constant (Q15 format: 16384, exp 0) */
#define FLOAT_HALF {16384, 0}

#else

/** Floating point zero constant */
#define FLOAT_ZERO 0.f

/** Floating point one constant */
#define FLOAT_ONE 1.f

/** Floating point half constant */
#define FLOAT_HALF 0.5f

#endif  // FIXED_POINT

}  // namespace audio_tools
