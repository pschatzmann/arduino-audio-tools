/// Regression test for StrView::matches() (glob-style '*'/'?' matching,
/// plus ','/';'/'|' separated alternatives).
///
/// Covers a bug where a pattern ending in '*' (e.g. the "Track*" style
/// filters used by AudioSourceSDMMC/AudioSourceSTD/AudioSourceVFS file
/// filters, or OSC address patterns) failed to match a string that matches
/// the literal prefix exactly with nothing left over, e.g.
/// StrView("a").matches("a*") used to return false instead of true.
///
/// Also covers the alternatives syntax added to let a single file filter
/// select multiple extensions, e.g. "*.mp3;*.MP3;*.wav" - the exact style
/// of pattern a user tried (and which previously failed outright, since
/// the whole string was treated as one literal glob) when wiring up
/// AudioSourceSDMMC::setFileFilter() for a multi-format player.

#include <assert.h>

#include "AudioTools.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

using namespace audio_tools;

static bool m(const char* line, const char* pattern) {
  return StrView(line).matches(pattern);
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // plain literal match / mismatch
  assert(m("abc", "abc") == true);
  assert(m("abc", "abd") == false);

  // '?' wildcard
  assert(m("abc", "a?c") == true);
  assert(m("abc", "a?d") == false);
  assert(m("ab", "a?c") == false);  // '?' needs a char, string too short

  // leading '*' (suffix match) - always worked
  assert(m("test.mp3", "*.mp3") == true);
  assert(m("test.wav", "*.mp3") == false);
  assert(m("anything", "*") == true);
  assert(m("", "*") == true);

  // trailing '*' (prefix match) - this was the broken case
  assert(m("a", "a*") == true);
  assert(m("ab", "a*") == true);
  assert(m("ab", "ab*") == true);
  assert(m("Track", "Track*") == true);
  assert(m("Track01.mp3", "Track*") == true);
  assert(m("Tra", "Track*") == false);

  // multiple '*' wildcards, including one immediately at the end
  assert(m("abc", "*a*b*c*") == true);
  assert(m("aXbXc", "*a*b*c*") == true);
  assert(m("xaXbc", "*a*b*c*") == true);
  assert(m("xyz", "*a*b*c*") == false);

  // empty pattern / empty string edge cases
  assert(m("", "") == true);
  assert(m("a", "") == false);
  assert(m("", "a") == false);
  assert(m("", "**") == true);

  // alternatives separated by ',', ';' or '|'
  assert(m("x.wav", "*.mp3,*.wav") == true);
  assert(m("x.wav", "*.mp3|*.wav") == true);
  assert(m("x.wav", "*.mp3;*.wav") == true);
  assert(m("x.flac", "*.mp3;*.wav") == false);
  // whitespace around a separator is trimmed
  assert(m("x.wav", "*.mp3; *.wav") == true);
  assert(m("x.wav", "*.mp3 ; *.wav") == true);
  // mixed separators, and the real-world multi-extension filter from the
  // "using MultiDecoder for SD card" report (github discussion #2411)
  const char* multi_ext = "*.mp3;*.MP3;*.aac;*.AAC;*.wav;*.WAV;*.flac;*.FLAC";
  assert(m("09 - Track.MP3", multi_ext) == true);
  assert(m("Britney Spears - If U Seek Amy.flac", multi_ext) == true);
  assert(m("11.wav", multi_ext) == true);
  assert(m("WPSettings.dat", multi_ext) == false);
  assert(m("IndexerVolumeGuid", multi_ext) == false);
  // pattern made up only of separators has no real alternative
  assert(m("x", ",;|") == false);

  Serial.println("All StrView::matches() tests passed");
  exit(0);
}

void loop() {}
