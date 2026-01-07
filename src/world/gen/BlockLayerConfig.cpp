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

    auto parseRules = [&](const std::string &key,
                          std::vector<BlockLayerRule> &dest) {
      if (j.contains(key)) {
        for (const auto &item : j[key]) {
          BlockLayerRule rule;
          if (item.contains("comment"))
            rule.comment = item["comment"].get<std::string>();
          if (item.contains("block"))
            rule.blockResourceId = item["block"].get<std::string>();
          if (item.contains("subSurfaceBlock"))
            rule.subSurfaceBlockResourceId =
                item["subSurfaceBlock"].get<std::string>();

          if (item.contains("condition")) {
            auto &c = item["condition"];
            if (c.contains("minTemp"))
              rule.minTemp = c["minTemp"];
            if (c.contains("maxTemp"))
              rule.maxTemp = c["maxTemp"];
            if (c.contains("minRain"))
              rule.minRain = c["minRain"];
            if (c.contains("maxRain"))
              rule.maxRain = c["maxRain"];
            if (c.contains("minFertility"))
              rule.minFertility = c["minFertility"];
            if (c.contains("maxFertility"))
              rule.maxFertility = c["maxFertility"];
            if (c.contains("minPatchNoise"))
              rule.minPatchNoise = c["minPatchNoise"];
            if (c.contains("maxPatchNoise"))
              rule.maxPatchNoise = c["maxPatchNoise"];
            if (c.contains("minY"))
              rule.minY = c["minY"];
            if (c.contains("maxY"))
              rule.maxY = c["maxY"];
            if (c.contains("minBeachNoise"))
              rule.minBeachNoise = c["minBeachNoise"];
            if (c.contains("maxBeachNoise"))
              rule.maxBeachNoise = c["maxBeachNoise"];
          }

          // Resolve IDs immediately
          Block *b =
              BlockRegistry::getInstance().getBlock(rule.blockResourceId);
          if (b)
            rule.cachedBlockId = b->getId();

          if (!rule.subSurfaceBlockResourceId.empty()) {
            Block *sb = BlockRegistry::getInstance().getBlock(
                rule.subSurfaceBlockResourceId);
            if (sb)
              rule.cachedSubSurfaceBlockId = sb->getId();
          } else {
            // Default sub-surface to Dirt (1)
            Block *dirt = BlockRegistry::getInstance().getBlock("lithos:dirt");
            if (dirt)
              rule.cachedSubSurfaceBlockId = dirt->getId();
            else
              rule.cachedSubSurfaceBlockId = 1;
          }

          dest.push_back(rule);
        }
      }
    };

    surfaceRules.clear();
    liquidRules.clear();
    beachRules.clear();
    underwaterRules.clear();

    parseRules("surfaceRules", surfaceRules);
    parseRules("liquidSurfaceRules", liquidRules);
    parseRules("beachRules", beachRules);
    parseRules("underwaterRules", underwaterRules);

    LOG_INFO("Loaded rules from {}", path);

  } catch (const std::exception &e) {
    LOG_ERROR("Failed to load blocklayers config: {}", e.what());
    return false;
  }
  return true;
}

std::pair<block_id, block_id>
BlockLayerConfig::GetSurfaceBlocks(float temp, float rain, float fertility,
                                   float patchNoise, float yNormalized,
                                   float beachNoise) const {
  for (const auto &rule : surfaceRules) {
    if (rule.Matches(temp, rain, fertility, patchNoise, yNormalized,
                     beachNoise)) {
      return {rule.cachedBlockId, rule.cachedSubSurfaceBlockId};
    }
  }
  // Default: Grass, Dirt
  static block_id grass = 0, dirt = 0;
  if (grass == 0) {
    Block *gb = BlockRegistry::getInstance().getBlock("lithos:grass-soil");
    if (gb)
      grass = gb->getId();
    Block *db = BlockRegistry::getInstance().getBlock("lithos:dirt-soil");
    if (db)
      dirt = db->getId();
  }
  return {grass, dirt};
}

block_id BlockLayerConfig::GetBeachBlockId(float temp, float rain,
                                           float beachNoise,
                                           float yNormalized) const {
  for (const auto &rule : beachRules) {
    if (rule.Matches(temp, rain, 0.0f, 0.0f, yNormalized, beachNoise)) {
      return rule.cachedBlockId;
    }
  }
  return 0; // No beach
}

block_id BlockLayerConfig::GetUnderwaterBlockId(float temp, float rain,
                                                float yNormalized) const {
  for (const auto &rule : underwaterRules) {
    if (rule.Matches(temp, rain, 0.0f, 0.0f, yNormalized, 0.0f)) {
      return rule.cachedBlockId;
    }
  }
  return 0; // No underwater override
}

block_id BlockLayerConfig::GetLiquidSurfaceBlockId(float temp, float rain,
                                                   float fertility,
                                                   float patchNoise,
                                                   float yNormalized) const {
  for (const auto &rule : liquidRules) {
    if (rule.Matches(temp, rain, fertility, patchNoise, yNormalized, 0.0f)) {
      return rule.cachedBlockId;
    }
  }
  static block_id water = 0;
  if (water == 0) {
    Block *wb = BlockRegistry::getInstance().getBlock("lithos:water");
    if (wb)
      water = wb->getId();
  }
  return water; // Default to Water if no rule matches
}
