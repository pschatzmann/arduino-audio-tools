#include <cassert>
#include <cstdint>

#include "AudioTools.h"

using namespace audio_tools;

static void test_bin_state_across_calls() {
  Bin bin(4, 2, false, 16);
  int16_t chunk1[] = {1, 101, 2, 102, 3, 103};
  int16_t chunk2[] = {4, 104, 5, 105, 6, 106, 7, 107, 8, 108};
  int16_t out[4] = {};

  assert(bin.convert(reinterpret_cast<uint8_t *>(out),
                     reinterpret_cast<uint8_t *>(chunk1), sizeof(chunk1)) == 0);

  size_t bytes = bin.convert(reinterpret_cast<uint8_t *>(out),
                             reinterpret_cast<uint8_t *>(chunk2),
                             sizeof(chunk2));
  int16_t expected[] = {10, 410, 26, 426};

  assert(bytes == sizeof(expected));
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    assert(out[i] == expected[i]);
  }
}

static void test_bin_exact_completion() {
  Bin bin(4, 1, true, 16);
  int16_t chunk1[] = {2, 4, 6};
  int16_t chunk2[] = {8};
  int16_t out[1] = {};

  assert(bin.convert(reinterpret_cast<uint8_t *>(out),
                     reinterpret_cast<uint8_t *>(chunk1), sizeof(chunk1)) == 0);

  size_t bytes = bin.convert(reinterpret_cast<uint8_t *>(out),
                             reinterpret_cast<uint8_t *>(chunk2),
                             sizeof(chunk2));
  assert(bytes == sizeof(int16_t));
  assert(out[0] == 5);
}

static void test_bin_reset_discards_partial_state() {
  Bin bin(4, 1, false, 16);
  int16_t chunk1[] = {1, 2};
  int16_t chunk2[] = {10, 20, 30, 40};
  int16_t out[1] = {};

  assert(bin.convert(reinterpret_cast<uint8_t *>(out),
                     reinterpret_cast<uint8_t *>(chunk1), sizeof(chunk1)) == 0);
  bin.reset();

  size_t bytes = bin.convert(reinterpret_cast<uint8_t *>(out),
                             reinterpret_cast<uint8_t *>(chunk2),
                             sizeof(chunk2));
  assert(bytes == sizeof(int16_t));
  assert(out[0] == 100);
}

static void test_bin_invalid_configuration() {
  Bin bin(0, 1, false, 16);
  int16_t chunk[] = {1, 2, 3, 4};
  int16_t out[1] = {};

  assert(bin.convert(reinterpret_cast<uint8_t *>(out),
                     reinterpret_cast<uint8_t *>(chunk), sizeof(chunk)) == 0);
}

static void test_channel_bin_diff_state_across_calls() {
  ChannelBinDiff diff(2, 4, false, 16);
  int16_t chunk1[] = {10, 1, 100, 5};
  int16_t chunk2[] = {20, 2, 200, 10, 30, 3, 300, 15, 40, 4, 400, 20};
  int16_t out[4] = {};

  assert(diff.convert(reinterpret_cast<uint8_t *>(out),
                      reinterpret_cast<uint8_t *>(chunk1), sizeof(chunk1)) == 0);

  size_t bytes = diff.convert(reinterpret_cast<uint8_t *>(out),
                              reinterpret_cast<uint8_t *>(chunk2),
                              sizeof(chunk2));
  int16_t expected[] = {27, 285, 63, 665};

  assert(bytes == sizeof(expected));
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    assert(out[i] == expected[i]);
  }
}

static void test_channel_bin_diff_exact_completion() {
  ChannelBinDiff diff(2, 4, true, 16);
  int16_t chunk1[] = {10, 1, 100, 5};
  int16_t chunk2[] = {20, 2, 200, 10};
  int16_t out[2] = {};

  assert(diff.convert(reinterpret_cast<uint8_t *>(out),
                      reinterpret_cast<uint8_t *>(chunk1), sizeof(chunk1)) == 0);

  size_t bytes = diff.convert(reinterpret_cast<uint8_t *>(out),
                              reinterpret_cast<uint8_t *>(chunk2),
                              sizeof(chunk2));
  int16_t expected[] = {13, 142};

  assert(bytes == sizeof(expected));
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    assert(out[i] == expected[i]);
  }
}

static void test_channel_bin_diff_reset_discards_partial_state() {
  ChannelBinDiff diff(2, 4, false, 16);
  int16_t chunk1[] = {10, 1, 100, 5};
  int16_t chunk2[] = {30, 3, 300, 15, 40, 4, 400, 20};
  int16_t out[2] = {};

  assert(diff.convert(reinterpret_cast<uint8_t *>(out),
                      reinterpret_cast<uint8_t *>(chunk1), sizeof(chunk1)) == 0);
  diff.reset();

  size_t bytes = diff.convert(reinterpret_cast<uint8_t *>(out),
                              reinterpret_cast<uint8_t *>(chunk2),
                              sizeof(chunk2));
  int16_t expected[] = {63, 665};

  assert(bytes == sizeof(expected));
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    assert(out[i] == expected[i]);
  }
}

static void test_channel_bin_diff_invalid_configuration() {
  ChannelBinDiff diff(0, 4, false, 16);
  int16_t chunk[] = {10, 1, 100, 5};
  int16_t out[2] = {};

  assert(diff.convert(reinterpret_cast<uint8_t *>(out),
                      reinterpret_cast<uint8_t *>(chunk), sizeof(chunk)) == 0);
}

static void test_decimate_state_across_calls() {
  Decimate dec(3, 1, 16);
  int16_t chunk1[] = {1, 2};
  int16_t chunk2[] = {3, 4, 5, 6};
  int16_t out[2] = {};

  assert(dec.convert(reinterpret_cast<uint8_t *>(out),
                     reinterpret_cast<uint8_t *>(chunk1), sizeof(chunk1)) == 0);

  size_t bytes = dec.convert(reinterpret_cast<uint8_t *>(out),
                             reinterpret_cast<uint8_t *>(chunk2),
                             sizeof(chunk2));
  int16_t expected[] = {3, 6};

  assert(bytes == sizeof(expected));
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    assert(out[i] == expected[i]);
  }
}

static void test_decimate_reset_discards_partial_state() {
  Decimate dec(3, 1, 16);
  int16_t chunk1[] = {1, 2};
  int16_t chunk2[] = {10, 20, 30};
  int16_t out[1] = {};

  assert(dec.convert(reinterpret_cast<uint8_t *>(out),
                     reinterpret_cast<uint8_t *>(chunk1), sizeof(chunk1)) == 0);
  dec.reset();

  size_t bytes = dec.convert(reinterpret_cast<uint8_t *>(out),
                             reinterpret_cast<uint8_t *>(chunk2),
                             sizeof(chunk2));
  assert(bytes == sizeof(int16_t));
  assert(out[0] == 30);
}

static void test_decimate_invalid_configuration() {
  Decimate dec(0, 1, 16);
  int16_t chunk[] = {1, 2, 3};
  int16_t out[1] = {};

  assert(dec.convert(reinterpret_cast<uint8_t *>(out),
                     reinterpret_cast<uint8_t *>(chunk), sizeof(chunk)) == 0);
}

static void test_converter_auto_center_reset() {
  ConverterAutoCenter center(1, 16);
  int16_t data1[] = {100, 100, 100, 100};
  int16_t data2[] = {50, 50, 50, 50};

  assert(center.convert(reinterpret_cast<uint8_t *>(data1), sizeof(data1)) ==
         sizeof(data1));
  for (size_t i = 0; i < sizeof(data1) / sizeof(data1[0]); ++i) {
    assert(data1[i] == 0);
  }

  center.reset();
  assert(center.convert(reinterpret_cast<uint8_t *>(data2), sizeof(data2)) ==
         sizeof(data2));
  for (size_t i = 0; i < sizeof(data2) / sizeof(data2[0]); ++i) {
    assert(data2[i] == 0);
  }
}

static void test_smooth_transition_start_uses_per_edge_state() {
  SmoothTransition<int16_t> transition(2, true, false, 0.5f);
  int16_t data1[] = {10, 20, 30, 40};
  int16_t data2[] = {50, 60, 70, 80};

  assert(transition.convert(reinterpret_cast<uint8_t *>(data1), sizeof(data1)) ==
         sizeof(data1));
  int16_t expected1[] = {0, 0, 15, 20};
  for (size_t i = 0; i < sizeof(expected1) / sizeof(expected1[0]); ++i) {
    assert(data1[i] == expected1[i]);
  }

  assert(transition.convert(reinterpret_cast<uint8_t *>(data2), sizeof(data2)) ==
         sizeof(data2));
  int16_t expected2[] = {50, 60, 70, 80};
  for (size_t i = 0; i < sizeof(expected2) / sizeof(expected2[0]); ++i) {
    assert(data2[i] == expected2[i]);
  }
}

static void test_smooth_transition_end_advances_factor() {
  SmoothTransition<int16_t> transition(2, false, true, 0.5f);
  int16_t data1[] = {10, 20, 30, 40};
  int16_t data2[] = {50, 60, 70, 80};

  assert(transition.convert(reinterpret_cast<uint8_t *>(data1), sizeof(data1)) ==
         sizeof(data1));
  int16_t expected1[] = {5, 10, 0, 0};
  for (size_t i = 0; i < sizeof(expected1) / sizeof(expected1[0]); ++i) {
    assert(data1[i] == expected1[i]);
  }

  assert(transition.convert(reinterpret_cast<uint8_t *>(data2), sizeof(data2)) ==
         sizeof(data2));
  int16_t expected2[] = {50, 60, 70, 80};
  for (size_t i = 0; i < sizeof(expected2) / sizeof(expected2[0]); ++i) {
    assert(data2[i] == expected2[i]);
  }
}

int main() {
  test_bin_state_across_calls();
  test_bin_exact_completion();
  test_bin_reset_discards_partial_state();
  test_bin_invalid_configuration();
  test_channel_bin_diff_state_across_calls();
  test_channel_bin_diff_exact_completion();
  test_channel_bin_diff_reset_discards_partial_state();
  test_channel_bin_diff_invalid_configuration();
  test_decimate_state_across_calls();
  test_decimate_reset_discards_partial_state();
  test_decimate_invalid_configuration();
  test_converter_auto_center_reset();
  test_smooth_transition_start_uses_per_edge_state();
  test_smooth_transition_end_advances_factor();
  return 0;
}
