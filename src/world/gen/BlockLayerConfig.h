#pragma once

#include "../Block.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct BlockLayerRule {
  std::string comment;
  std::string blockResourceId;
  uint8_t cachedBlockId = 0; // 0 is Air (default)

  // Conditions (Defaults allow eveything)
  float minTemp = -9999.0f;
  float maxTemp = 9999.0f;
  float minRain = 0.0f;
  float maxRain = 1.0f;
  float minFertility = 0.0f;
  float maxFertility = 1.0f;
  float minPatchNoise = -1.0f;
  float maxPatchNoise = 1.0f;
  float minY = 0.0f;
  float maxY = 1.0f; // Normalized Height (0.0 - 1.0)

  bool Matches(float temp, float rain, float fertility, float patchNoise,
               float yNormalized) const;
};

class BlockLayerConfig {
public:
  static BlockLayerConfig &Get() {
    static BlockLayerConfig instance;
    return instance;
  }

  bool Load(const std::string &path);
  uint8_t GetSurfaceBlockId(float temp, float rain, float fertility,
                            float patchNoise, float yNormalized) const;
  uint8_t GetLiquidSurfaceBlockId(float temp, float rain, float fertility,
                                  float patchNoise, float yNormalized) const;

private:
  BlockLayerConfig() = default;
  std::vector<BlockLayerRule> rules;
  std::vector<BlockLayerRule> liquidRules;
};
