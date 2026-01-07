#pragma once

#include "../Block.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct BlockLayerRule {
  std::string comment;
  std::string blockResourceId;
  block_id cachedBlockId = 0; // 0 is Air (default)

  // Sub-surface support (what lies beneath)
  std::string subSurfaceBlockResourceId;
  block_id cachedSubSurfaceBlockId = 0;

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
  float minBeachNoise = -1.0f;
  float maxBeachNoise = 1.0f;

  bool Matches(float temp, float rain, float fertility, float patchNoise,
               float yNormalized, float beachNoise) const;
};

class BlockLayerConfig {
public:
  static BlockLayerConfig &Get() {
    static BlockLayerConfig instance;
    return instance;
  }

  bool Load(const std::string &path);

  // Returns tuple: [surfaceBlockId, subSurfaceBlockId]
  // If no rule matches, returns {GRASS, DIRT} defaults
  std::pair<block_id, block_id> GetSurfaceBlocks(float temp, float rain,
                                                 float fertility,
                                                 float patchNoise, float yFrac,
                                                 float beachNoise) const;

  block_id GetLiquidSurfaceBlockId(float temp, float rain, float fertility,
                                   float patchNoise, float yNormalized) const;

  // Returns ID or 0 if no rule matches
  block_id GetBeachBlockId(float temp, float rain, float beachNoise,
                           float yNormalized) const;

  // Returns ID or 0 if no rule matches
  block_id GetUnderwaterBlockId(float temp, float rain,
                                float yNormalized) const;

private:
  BlockLayerConfig() = default;
  std::vector<BlockLayerRule> surfaceRules;
  std::vector<BlockLayerRule> liquidRules;
  std::vector<BlockLayerRule> beachRules;
  std::vector<BlockLayerRule> underwaterRules;
};
