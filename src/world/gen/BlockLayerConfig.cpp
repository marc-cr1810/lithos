#include "BlockLayerConfig.h"
#include "../../debug/Logger.h"
#include "../Block.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

bool BlockLayerRule::Matches(float temp, float rain, float fertility,
                             float patchNoise, float yNormalized,
                             float beachNoise) const {
  if (temp < minTemp || temp > maxTemp)
    return false;
  if (rain < minRain || rain > maxRain)
    return false;
  if (fertility < minFertility || fertility > maxFertility)
    return false;
  if (patchNoise < minPatchNoise || patchNoise > maxPatchNoise)
    return false;
  if (yNormalized < minY || yNormalized > maxY)
    return false;
  if (beachNoise < minBeachNoise || beachNoise > maxBeachNoise)
    return false;
  return true;
}

bool BlockLayerConfig::Load(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open block layer config: {}", path);
    return false;
  }

  try {
    json j;
    file >> j;

    rules.clear();
    liquidRules.clear();

    if (j.contains("surfaceRules") && j["surfaceRules"].is_array()) {
      for (const auto &item : j["surfaceRules"]) {
        BlockLayerRule rule;

        if (item.contains("comment"))
          rule.comment = item["comment"].get<std::string>();
        if (item.contains("block"))
          rule.blockResourceId = item["block"].get<std::string>();

        // Resolve Block ID immediately
        Block *block =
            BlockRegistry::getInstance().getBlock(rule.blockResourceId);
        if (block) {
          rule.cachedBlockId = block->getId();
        } else {
          LOG_ERROR("BlockLayerConfig: Unknown block {}", rule.blockResourceId);
          continue; // Skip invalid blocks
        }

        if (item.contains("condition")) {
          const auto &cond = item["condition"];
          if (cond.contains("minTemp"))
            rule.minTemp = cond["minTemp"].get<float>();
          if (cond.contains("maxTemp"))
            rule.maxTemp = cond["maxTemp"].get<float>();
          if (cond.contains("minRain"))
            rule.minRain = cond["minRain"].get<float>();
          if (cond.contains("maxRain"))
            rule.maxRain = cond["maxRain"].get<float>();
          if (cond.contains("minFertility"))
            rule.minFertility = cond["minFertility"].get<float>();
          if (cond.contains("maxFertility"))
            rule.maxFertility = cond["maxFertility"].get<float>();
          if (cond.contains("minPatchNoise"))
            rule.minPatchNoise = cond["minPatchNoise"].get<float>();
          if (cond.contains("maxPatchNoise"))
            rule.maxPatchNoise = cond["maxPatchNoise"].get<float>();
          if (cond.contains("minY"))
            rule.minY = cond["minY"].get<float>();
          if (cond.contains("maxY"))
            rule.maxY = cond["maxY"].get<float>();
          if (cond.contains("minBeachNoise"))
            rule.minBeachNoise = cond["minBeachNoise"].get<float>();
          if (cond.contains("maxBeachNoise"))
            rule.maxBeachNoise = cond["maxBeachNoise"].get<float>();
        }

        rules.push_back(rule);
      }
    }

    if (j.contains("liquidSurfaceRules") &&
        j["liquidSurfaceRules"].is_array()) {
      for (const auto &item : j["liquidSurfaceRules"]) {
        BlockLayerRule rule;

        if (item.contains("comment"))
          rule.comment = item["comment"].get<std::string>();
        if (item.contains("block"))
          rule.blockResourceId = item["block"].get<std::string>();

        Block *block =
            BlockRegistry::getInstance().getBlock(rule.blockResourceId);
        if (block) {
          rule.cachedBlockId = block->getId();
        } else {
          LOG_ERROR("BlockLayerConfig: Unknown liquid block {}",
                    rule.blockResourceId);
          continue;
        }

        if (item.contains("condition")) {
          const auto &cond = item["condition"];
          if (cond.contains("minTemp"))
            rule.minTemp = cond["minTemp"].get<float>();
          if (cond.contains("maxTemp"))
            rule.maxTemp = cond["maxTemp"].get<float>();
          if (cond.contains("minRain"))
            rule.minRain = cond["minRain"].get<float>();
          if (cond.contains("maxRain"))
            rule.maxRain = cond["maxRain"].get<float>();
          if (cond.contains("minY"))
            rule.minY = cond["minY"].get<float>();
          if (cond.contains("maxY"))
            rule.maxY = cond["maxY"].get<float>();
        }

        liquidRules.push_back(rule);
      }
    }

    LOG_INFO("Loaded {} surface rules and {} liquid rules from {}",
             rules.size(), liquidRules.size(), path);
    return true;

  } catch (const json::exception &e) {
    LOG_ERROR("JSON Parse Error in {}: {}", path, e.what());
    return false;
  }
}

uint8_t BlockLayerConfig::GetSurfaceBlockId(float temp, float rain,
                                            float fertility, float patchNoise,
                                            float yNormalized,
                                            float beachNoise) const {
  for (const auto &rule : rules) {
    if (rule.Matches(temp, rain, fertility, patchNoise, yNormalized,
                     beachNoise)) {
      return rule.cachedBlockId;
    }
  }
  return 2; // Default to Grass (ID 2) if no rule matches
}

uint8_t BlockLayerConfig::GetLiquidSurfaceBlockId(float temp, float rain,
                                                  float fertility,
                                                  float patchNoise,
                                                  float yNormalized) const {
  for (const auto &rule : liquidRules) {
    if (rule.Matches(temp, rain, fertility, patchNoise, yNormalized, 0.0f)) {
      return rule.cachedBlockId;
    }
  }
  return 9; // Default to Water (ID 9) if no rule matches
}
