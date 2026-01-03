#pragma once
#include <cmath>
#include <random>


class MathUtils {
public:
  static float SampleGaussian(std::mt19937 &rng, float mean, float variance) {
    if (variance <= 0.0f)
      return mean;
    std::normal_distribution<float> dist(mean, std::sqrt(variance));
    return dist(rng);
  }

  static float SampleUniform(std::mt19937 &rng, float min, float max) {
    if (min >= max)
      return min;
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
  }

  // Transform types for "evolve" parameters
  enum class TransformType { LINEAR, QUADRATIC, SINUS, NONE };

  static float ApplyTransform(float value, float progress, TransformType type,
                              float factor) {
    switch (type) {
    case TransformType::LINEAR:
      return value * (1.0f + (progress * factor));
    case TransformType::QUADRATIC:
      return value * (1.0f + (progress * progress * factor));
    case TransformType::SINUS:
      return value * (1.0f + (std::sin(progress * 3.14159f) * factor));
    default:
      return value;
    }
  }
};
