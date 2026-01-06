#include "OreDecorator.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "Block.h"
#include "ChunkColumn.h"
#include "World.h"
#include "WorldGenRegion.h"
#include "WorldGenerator.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <random>

// Static member definition
std::vector<OreType> OreDecorator::oreTypes;

OreDecorator::OreDecorator() {
  // Config loaded at app startup via Application.cpp
}

void OreDecorator::LoadConfig(const std::filesystem::path &configPath) {
  std::ifstream file(configPath);
  if (!file.is_open()) {
    LOG_WARN("Failed to load ore config: {}", configPath.string());
    return;
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const std::exception &e) {
    LOG_ERROR("JSON parse error in {}: {}", configPath.string(), e.what());
    return;
  }

  // Load ore types
  if (j.contains("ores")) {
    for (const auto &ot : j["ores"]) {
      OreType ore;
      ore.code = ot.value("code", "");
      ore.blockId = ot.value("block", "");

      // Distribution settings
      if (ot.contains("distribution")) {
        auto &dist = ot["distribution"];
        std::string distType = dist.value("type", "uniform");

        if (distType == "uniform")
          ore.distributionType = DistributionType::UNIFORM;
        else if (distType == "triangle")
          ore.distributionType = DistributionType::TRIANGLE;
        else if (distType == "gaussian")
          ore.distributionType = DistributionType::GAUSSIAN;

        ore.minY = dist.value("minY", 0.0f);
        ore.maxY = dist.value("maxY", 1.0f);
        ore.peakY = dist.value("peakY", 0.5f);
        ore.meanY = dist.value("meanY", 0.5f);
        ore.stdDev = dist.value("stdDev", 0.1f);
        ore.veinsPerChunk = dist.value("veinsPerChunk", 10);
      }

      // Vein settings
      if (ot.contains("vein")) {
        auto &vein = ot["vein"];
        ore.minVeinSize = vein.value("minSize", 4);
        ore.maxVeinSize = vein.value("maxSize", 8);
      }

      // Spawn conditions
      if (ot.contains("spawnConditions")) {
        auto &sc = ot["spawnConditions"];
        if (sc.contains("replaceBlocks")) {
          ore.replaceBlocks =
              sc["replaceBlocks"].get<std::vector<std::string>>();
        }
        if (sc.contains("excludeBlocks")) {
          ore.excludeBlocks =
              sc["excludeBlocks"].get<std::vector<std::string>>();
        }
      }

      oreTypes.push_back(ore);
    }
  }

  LOG_INFO("Loaded {} ore types from {}", oreTypes.size(), configPath.string());
}

float OreDecorator::SampleDistribution(const OreType &ore, float random) const {
  // random is 0.0 to 1.0
  switch (ore.distributionType) {
  case DistributionType::UNIFORM:
    return ore.minY + random * (ore.maxY - ore.minY);

  case DistributionType::TRIANGLE: {
    // Triangle distribution with peak at peakY
    float range = ore.maxY - ore.minY;
    float peak = (ore.peakY - ore.minY) / range;

    if (random < peak) {
      return ore.minY + sqrt(random * range * (ore.peakY - ore.minY));
    } else {
      return ore.maxY - sqrt((1.0f - random) * range * (ore.maxY - ore.peakY));
    }
  }

  case DistributionType::GAUSSIAN: {
    // Box-Muller transform for gaussian
    static thread_local std::mt19937 rng(std::random_device{}());
    std::normal_distribution<float> dist(ore.meanY, ore.stdDev);
    float y = dist(rng);
    return std::max(ore.minY, std::min(ore.maxY, y));
  }
  }

  return ore.minY;
}

bool OreDecorator::MatchesPattern(const std::string &pattern,
                                  const std::string &blockId) const {
  // Simple wildcard matching for patterns like "lithos:rock_*"
  if (pattern.find('*') == std::string::npos) {
    return pattern == blockId;
  }

  // Find the wildcard position
  size_t wildcardPos = pattern.find('*');
  std::string prefix = pattern.substr(0, wildcardPos);

  // Check if blockId starts with the prefix
  if (blockId.size() < prefix.size()) {
    return false;
  }

  return blockId.substr(0, prefix.size()) == prefix;
}

bool OreDecorator::CanReplaceBlock(const std::string &blockId,
                                   const OreType &ore) const {
  // Check if in exclude list
  for (const auto &excluded : ore.excludeBlocks) {
    if (MatchesPattern(excluded, blockId)) {
      return false;
    }
  }

  // Check if in replace list
  if (ore.replaceBlocks.empty()) {
    return true; // No restrictions
  }

  for (const auto &allowed : ore.replaceBlocks) {
    if (MatchesPattern(allowed, blockId)) {
      return true;
    }
  }

  return false;
}

void OreDecorator::GenerateVein(WorldGenRegion &region, int x, int y, int z,
                                const OreType &ore, int size) {
  // Get ore block type
  Block *oreBlock = nullptr;
  for (int i = 0; i < 256; ++i) {
    Block *b = BlockRegistry::getInstance().getBlock(i);
    if (b && b->getResourceId() == ore.blockId) {
      oreBlock = b;
      break;
    }
  }

  if (!oreBlock) {
    return;
  }

  BlockType oreType = (BlockType)oreBlock->getId();

  // Generate blob-shaped vein
  for (int i = 0; i < size; ++i) {
    int dx = (rand() % 3) - 1;
    int dy = (rand() % 3) - 1;
    int dz = (rand() % 3) - 1;

    int px = x + dx;
    int py = y + dy;
    int pz = z + dz;

    // Check current block
    BlockType currentBlock = region.getBlock(px, py, pz);
    if (currentBlock == AIR || currentBlock == WATER || currentBlock == LAVA) {
      continue;
    }

    // Get block resource ID
    Block *block = BlockRegistry::getInstance().getBlock(currentBlock);
    if (!block) {
      continue;
    }

    std::string blockId = block->getResourceId();

    // Check if we can replace this block
    if (CanReplaceBlock(blockId, ore)) {
      region.setBlock(px, py, pz, oreType);
    }
  }
}

void OreDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                            const ChunkColumn &column) {
  // Old method - kept for compatibility but not used
}

void OreDecorator::Decorate(WorldGenerator &generator, WorldGenRegion &region,
                            const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("Decorator_Ores_Region",
                            generator.IsProfilingEnabled());

  int colX = region.getCenterX();
  int colZ = region.getCenterZ();

  // Process each ore type
  for (const auto &ore : oreTypes) {
    for (int i = 0; i < ore.veinsPerChunk; ++i) {
      // Random position in chunk
      int x = colX * CHUNK_SIZE + (rand() % CHUNK_SIZE);
      int z = colZ * CHUNK_SIZE + (rand() % CHUNK_SIZE);

      // Sample Y based on distribution
      float randomY = (float)(rand() % 10000) / 10000.0f;
      float normalizedY = SampleDistribution(ore, randomY);
      int y = (int)(normalizedY * 256.0f); // Convert to absolute Y

      // Clamp to valid range
      y = std::max(0, std::min(255, y));

      // Determine vein size
      int veinSize =
          ore.minVeinSize + (rand() % (ore.maxVeinSize - ore.minVeinSize + 1));

      // Generate vein
      GenerateVein(region, x, y, z, ore, veinSize);
    }
  }
}
