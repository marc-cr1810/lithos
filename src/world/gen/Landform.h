#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Terrain Y key positions and thresholds
struct YKey {
  int yLevel;      // Block Y
  float threshold; // Density threshold offset
};

// Terrain Octave parameters
struct OctaveParam {
  float amplitude = 1.0f;
  float threshold = 0.0f; // 0 = standard addition
};

struct LandformVariant {
  std::string nameSuffix;
  float weight = 1.0f;
  std::vector<YKey> yKeys;

  // Climate Matching for Variants
  bool useClimate = true;
  float minTemp = -50.0f; // VS default: -50
  float maxTemp = 50.0f;  // VS default: 50
  int minRain = 0;        // VS default: 0
  int maxRain = 255;      // VS default: 255.0f;

  float GetDensityThreshold(int y) const;
  void BuildLUT(int worldHeight);
  std::shared_ptr<std::vector<float>> densityLUT;
};

struct Landform {
  std::string name;
  float weight = 1.0f; // Global spawn weight

  // Mutation
  float mutationChance = 0.0f;
  std::string mutationTarget;

  // Climate Matching
  bool useClimate = true;
  float minTemp = -50.0f; // VS default: -50
  float maxTemp = 50.0f;  // VS default: 50
  int minRain = 0;        // VS default: 0
  int maxRain = 255;      // VS default: 255

  std::vector<OctaveParam> terrainOctaves;
  std::vector<YKey> yKeys;

  std::vector<LandformVariant> variants;

  // Visuals
  glm::vec3 foliageTint = {1.0f, 1.0f, 1.0f};
  float edgeBlendTarget = 64.0f; // Target height for edge blending

  // Helper: Interpolate threshold for a given Y
  float GetDensityThreshold(int y) const;
  const std::vector<float> *GetLUT() const;
  void BuildLUT(int worldHeight);
  std::shared_ptr<std::vector<float>> densityLUT;
};

class LandformRegistry {
public:
  static LandformRegistry &Get();

  void LoadFromJson(const std::string &path);

  void Register(const Landform &landform);
  const Landform *GetLandform(const std::string &name) const;

  // Select a landform based on environment
  // Returns by pointer to avoid expensive copies
  // VS-style: position-seeded weighted random selection
  const Landform *Select(int worldX, int worldZ, float temp, float humid) const;

  // Select using a pre-calculated entropy value (0..1)
  const Landform *Select(float entropy, float temp, float humid) const;

private:
  LandformRegistry(); // Private constructor to register defaults
  std::vector<Landform> landforms;
};
