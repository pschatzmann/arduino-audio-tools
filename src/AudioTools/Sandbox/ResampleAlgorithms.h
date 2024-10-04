#pragma once
namespace audio_tools {

/// range 0:1
struct ReasampleLinearInterpolation {
  constexpr int getFrom() { return -getFrameCountSave(); };
  constexpr int getTo(int frames) { return frames - getFrameCountSave(); }
  constexpr int getFrameCountSave() { return 1; }
  float value(float* y, float xf) {
    int x = xf;
    int dx = xf - x;
    return y[x] + dx * (y[x + 1] - y[x]);
  }
};

/// range -1:2
struct ResampleBSpline {
  constexpr int getFrom() { return -1; };
  constexpr int getTo(int frames) { return frames - getFrameCountSave(); }
  constexpr int getFrameCountSave() { return 2; }
  float value(float* y, float xf) {
    int x = xf;
    int dx = xf - x;
    // 4-point, 3rd-order B-spline (x-form)
    float ym1py1 = y[x - 1] + y[x + 1];
    float c0 = 1.0f / 6.0f * ym1py1 + 2.0f / 3.0f * y[x];
    float c1 = 1.0f / 2.0f * (y[x + 1] - y[x - 1]);
    float c2 = 1.0f / 2.0f * ym1py1 - y[x];
    float c3 =
        1.0f / 2.0f * (y[x] - y[x + 1]) + 1.0f / 6.0f * (y[x + 2] - y[x - 1]);
    return ((c3 * dx + c2) * dx + c1) * dx + c0;
  }
};

/// range -1 : 2
struct ResampleLagrange {
  constexpr int getFrom() { return -1; };
  constexpr int getTo(int frames) { return frames - getFrameCountSave(); }
  constexpr int getFrameCountSave() { return 2; }
  float value(float* y, float xf) {
    int x = xf;
    int dx = xf - x;
    // 4-point, 3rd-order Lagrange (x-form)
    float c0 = y[x];
    float c1 = y[x + 1] - 1.0f / 3.0f * y[x - 1] - 1.0f / 2.0f * y[x] -
               1.0f / 6.0f * y[x + 2];
    float c2 = 1.0f / 2.0f * (y[x - 1] + y[x + 1]) - y[x];
    float c3 =
        1.0f / 6.0f * (y[x + 2] - y[x - 1]) + 1 / 2.0 * (y[x] - y[x + 1]);
    return ((c3 * dx + c2) * dx + c1) * dx + c0;
  }
};

// Range -1:2
struct ResmpleHermite {
  constexpr int getFrom() { return -1; };
  constexpr int getTo(int frames) { return frames - getFrameCountSave(); }
  constexpr int getFrameCountSave() { return 2; }
  float value(float* y, float xf) {
    int x = xf;
    int dx = xf - x;
    // 4-point, 3rd-order Hermite (x-form)
    float c0 = y[x];
    float c1 = 1.0f / 2.0f * (y[x + 1] - y[x - 1]);
    float c2 = y[x - 1] - 5.0f / 2.0f * y[x] + 2.0f * y[x + 1] -
               1.0f / 2.0f * y[x + 2];
    float c3 =
        1.0f / 2.0f * (y[x + 2] - y[x - 1]) + 3.0f / 2.0f * (y[x] - y[x + 1]);
    return ((c3 * dx + c2) * dx + c1) * dx + c0;
  }
};

// range -1:2
struct ResampleParabolic {
  constexpr int getFrom() { return -1; };
  constexpr int getTo(int frames) { return frames - getFrameCountSave(); }
  constexpr int getFrameCountSave() { return 2; }

  float value(float* y, float xf) {
    int x = xf;
    int dx = xf - x;
    // 4-point, 2nd-order parabolic 2x (x-form)
    float y1mym1 = y[x + 1] - y[x - 1];
    float c0 = 1.0f / 2.0f * y[x] + 1 / 4.0 * (y[x - 1] + y[x + 1]);
    float c1 = 1.0 / 2.0f * y1mym1;
    float c2 = 1.0f / 4.0f * (y[x + 2] - y[x] - y1mym1);
    return (c2 * dx + c1) * dx + c0;
  }
};

// range 0:1
struct Resample2Point3Order {
  constexpr int getFrom() { return -getFrameCountSave(); };
  constexpr int getTo(int frames) { return frames - getFrameCountSave(); }
  constexpr int getFrameCountSave() { return 1; }

  float value(float* y, float xf) {
    int x = xf;
    int dx = xf - x;
    // Optimal 2x (2-point, 3rd-order) (z-form)
    float z = dx - 0.5f;
    float even1 = y[x + 1] + y[x];
    float odd1 = y[x + 1] - y[x];
    float c0 = even1 * 0.50037842517188658f;
    float c1 = odd1 * 1.00621089801788210f;
    float c2 = even1 * -0.004541102062639801f;
    float c3 = odd1 * -1.57015627178718420f;
    return ((c3 * z + c2) * z + c1) * z + c0;
  }
};

// Range -1 : 2
struct Resample4Point2Order {
  constexpr int getFrom() { return -1; };
  constexpr int getTo(int frames) { return frames - getFrameCountSave(); }
  constexpr int getFrameCountSave() { return 2; }

  float value(float* y, float xf) {
    int x = xf;
    int dx = xf - x;

    // Optimal 2x (4-point, 2nd-order) (z-form)
    float z = dx - 0.5f;
    float even1 = y[x + 1] + y[x], odd1 = y[x + 1] - y[x];
    float even2 = y[x + 2] + y[x - 1], odd2 = y[x + 2] - y[x - 1];
    float c0 = even1 * 0.42334633257225274 + even2 * 0.07668732202139628;
    float c1 = odd1 * 0.26126047291143606 + odd2 * 0.24778879018226652;
    float c2 = even1 * -0.213439787561776841 + even2 * 0.21303593243799016;
    return (c2 * z + c1) * z + c0;
  }
};

}  // namespace audio_tools