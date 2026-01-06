#pragma once

#include "../Block.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct BlockLayerRule {
  std::string comment;
  std::string blockResourceId;
  uint8_t cachedBlockId = 0; // 0 is Air (default)

  // Sub-surface support (what lies beneath)
  std::string subSurfaceBlockResourceId;
  uint8_t cachedSubSurfaceBlockId =
      2; // Default to Dirt/Grass? Or 0? Let's say 0 means "use default dirt"
         // logic if we want, or explicit.

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
  std::pair<uint8_t, uint8_t> GetSurfaceBlocks(float temp, float rain,
                                               float fertility,
                                               float patchNoise, float yFrac,
                                               float beachNoise) const;

  uint8_t GetLiquidSurfaceBlockId(float temp, float rain, float fertility,
                                  float patchNoise, float yNormalized) const;

  // Returns ID or 0 if no rule matches
  uint8_t GetBeachBlockId(float temp, float rain, float beachNoise,
                          float yNormalized) const;

  // Returns ID or 0 if no rule matches
  uint8_t GetUnderwaterBlockId(float temp, float rain, float yNormalized) const;

private:
  BlockLayerConfig() = default;
  std::vector<BlockLayerRule> surfaceRules;
  std::vector<BlockLayerRule> liquidRules;
  std::vector<BlockLayerRule> beachRules;
  std::vector<BlockLayerRule> underwaterRules;
};
