#pragma once
#include <glm/glm.hpp>
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
};

struct Landform {
  std::string name;
  float weight = 1.0f; // Global spawn weight

  // Mutation
  float mutationChance = 0.0f;
  std::string mutationTarget;

  // Climate Matching
  bool useClimate = true;
  float minTemp = -1.0f;
  float maxTemp = 1.0f;
  float minRain = -1.0f; // Rainfall/Humidity
  float maxRain = 1.0f;

  std::vector<OctaveParam> terrainOctaves;
  std::vector<YKey> yKeys;

  std::vector<LandformVariant> variants;

  // Visuals
  glm::vec3 foliageTint = {1.0f, 1.0f, 1.0f};
  float edgeBlendTarget = 64.0f; // Target height for edge blending

  // Helper: Interpolate threshold for a given Y
  float GetDensityThreshold(int y) const;
};

class LandformRegistry {
public:
  static LandformRegistry &Get();

  void Register(const Landform &landform);
  const Landform *GetLandform(const std::string &name) const;

  // Select a landform based on environment
  // Returns by value to allow variants/mutations to be applied
  Landform Select(float landformNoise, float temp, float humid) const;

private:
  LandformRegistry(); // Private constructor to register defaults
  std::vector<Landform> landforms;
};
