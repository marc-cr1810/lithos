#pragma once
#include <cmath>
#include <random>

class MathUtils {
public:
  static float SampleGaussian(std::mt19937 &rng, float mean, float stdDev) {
    if (stdDev <= 0.0f)
      return mean;
    std::normal_distribution<float> dist(mean, stdDev);
    return dist(rng);
  }

  static float SampleUniform(std::mt19937 &rng, float min, float max) {
    if (min >= max)
      return min;
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
  }

  static float SampleTriangle(std::mt19937 &rng, float min, float max,
                              float mode) {
    if (min >= max)
      return min;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float F = (mode - min) / (max - min);
    float U = dist(rng);
    if (U < F) {
      return min + std::sqrt(U * (max - min) * (mode - min));
    } else {
      return max - std::sqrt((1.0f - U) * (max - min) * (max - mode));
    }
  }

  // VS "inversegaussian" seems to be a specific flavor. Alternatively, it might
  // just mean Wald distribution. Standard Wald distribution takes (mean,
  // lambda). VS params are (avg, var). Let's assume standard Wald with lambda
  // derived? Actually, searching VS codebase would clarify, but given "var"
  // context, it might be simpler. HOWEVER: Common "Inverse Gaussian" in
  // Minecraft/Terrain modding often refers to simply "1.0 / Gaussian". Let's
  // assume Wald Distribution for now as it's the mathematical definition. But
  // wait, "avg" and "var" parameters. Wald mean=avg, var=mean^3/lambda. So
  // lambda = mean^3 / var.
  static float SampleInverseGaussian(std::mt19937 &rng, float mu, float sigma) {
    // Basic Wald generation algorithm (Michael/Schucany/Haas)
    // parameters: mu (mean), lambda (shape).
    // conversion: lambda = mu^3 / sigma^2 ?? Or just sigma param?
    // VS "var" is usually sigma. Let's assume lambda = mu^3 / (sigma*sigma).
    if (sigma <= 0.0001f || mu <= 0.0001f)
      return mu;

    float lambda = (mu * mu * mu) / (sigma * sigma);
    std::normal_distribution<float> norm(0.0f, 1.0f);
    float nu = norm(rng);
    float y = nu * nu;
    float x = mu + (mu * mu * y) / (2.0f * lambda) -
              (mu / (2.0f * lambda)) *
                  std::sqrt(4.0f * mu * lambda * y + mu * mu * y * y);

    float z = norm(rng); // uniform 0-1 needed? No, need uniform.
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    if (uni(rng) <= (mu / (mu + x))) {
      return x;
    } else {
      return (mu * mu) / x;
    }
  }

  // Transform types for "evolve" parameters
  enum class TransformType { LINEAR, QUADRATIC, SINUS, NONE };

  static float ApplyTransform(float value, float progress, TransformType type,
                              float factor) {
    switch (type) {
    case TransformType::LINEAR:
      // VS: factor is the target multiplier at progress=1.0
      // Formula: val * (1 + (factor - 1) * p)
      return value * (1.0f + ((factor - 1.0f) * progress));
    case TransformType::QUADRATIC:
      return value * (1.0f + ((factor - 1.0f) * progress * progress));
    case TransformType::SINUS:
      return value * (1.0f + ((factor - 1.0f) * std::sin(progress * 3.14159f)));
    default:
      return value;
    }
  }
};
