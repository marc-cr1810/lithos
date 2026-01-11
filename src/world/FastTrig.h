#ifndef FAST_TRIG_H
#define FAST_TRIG_H

#include <cmath>
#include <numbers>
#include <vector>

class FastTrig {
public:
  static void Initialize() {
    if (initialized)
      return;
    sinLUT.resize(4096);
    for (int i = 0; i < 4096; ++i) {
      sinLUT[i] = std::sin((float)i * (2.0f * 3.14159265f) / 4096.0f);
    }
    initialized = true;
  }

  static float Sin(float angle) {
    if (!initialized)
      Initialize();
    // angle in radians
    // normalize to 0-1
    float n = angle * (1.0f / (2.0f * 3.14159265f));
    int i = (int)(n * 4096.0f) & 4095;
    return sinLUT[i];
  }

  // Simplest robust implementation:
  // sin(x)
  static float FastSin(float x) {
    if (!initialized)
      Initialize();
    return sinLUT[(int)(x * 651.8986469f) & 4095]; // 4096 / 2pi = 651.89...
  }

  static float FastCos(float x) {
    if (!initialized)
      Initialize();
    return sinLUT[(int)((x + 1.57079632f) * 651.8986469f) & 4095];
  }

private:
  static std::vector<float> sinLUT;
  static bool initialized;
};
#endif
