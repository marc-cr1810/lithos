#include "OreDecorator.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "Block.h"
#include "Chunk.h"
#include "ChunkColumn.h"
#include "World.h"
#include "WorldGenRegion.h"
#include "WorldGenerator.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <random>

#include <cstring>
#include <nlohmann/json.hpp>

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

    // Resolve Block IDs
    for (auto &ore : oreTypes) {
      // Resolve main block ID
      Block *b = BlockRegistry::getInstance().getBlock(ore.blockId);
      if (b) {
        ore.resolvedBlockId = b->getId();
      } else {
        LOG_WARN("OreDecorator: Could not resolve block '{}'", ore.blockId);
      }

      // Pre-calculate replace/exclude tables
      for (uint32_t i = 0; i < 65536; ++i) {
        Block *candidate = BlockRegistry::getInstance().getBlock(i);
        if (!candidate)
          continue;

        std::string candidateId = candidate->getResourceId();

        // Check exclude
        bool excluded = false;
        for (const auto &ex : ore.excludeBlocks) {
          if (MatchesPattern(ex, candidateId)) {
            excluded = true;
            break;
          }
        }
        if (excluded) {
          ore.resolvedExcludeBlocks[i] = true;
          continue;
        }

        // Check replace
        bool replaceable = false;
        if (ore.replaceBlocks.empty()) {
          replaceable = true;
        } else {
          for (const auto &rep : ore.replaceBlocks) {
            if (MatchesPattern(rep, candidateId)) {
              replaceable = true;
              break;
            }
          }
        }
        ore.resolvedReplaceBlocks[i] = replaceable;
      }
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

bool OreDecorator::CanReplaceBlock(block_id blockId, const OreType &ore) const {
  if (ore.resolvedExcludeBlocks[blockId])
    return false;
  return ore.resolvedReplaceBlocks[blockId];
}

void OreDecorator::GenerateDisc(WorldGenRegion &region, int chunkX, int chunkZ,
                                int cx, int cy, int cz, const OreType &ore,
                                float radius, float thickness) {
  // 1. Calculate Bounds (Clamps to Chunk + Buffer)
  // Optimization: Only iterate intersection of Disc AABB and Chunk AABB
  int minX = std::max(chunkX * CHUNK_SIZE, (int)(cx - radius));
  int maxX = std::max(
      minX, std::min((chunkX + 1) * CHUNK_SIZE, (int)(cx + radius + 1)));

  int minZ = std::max(chunkZ * CHUNK_SIZE, (int)(cz - radius));
  int maxZ = std::max(
      minZ, std::min((chunkZ + 1) * CHUNK_SIZE, (int)(cz + radius + 1)));

  int minY = std::max(1, (int)(cy - thickness));
  int maxY = std::min(255, (int)(cy + thickness + 1));

  float radSq = radius * radius;
  float invRadSq = 1.0f / radSq;
  float thickSq = thickness * thickness;
  float invThickSq = 1.0f / thickSq;

  block_id oreBlock = ore.resolvedBlockId;

  // Y-Major Loop Order to fetch Chunk once per Y-level (or fewer)
  for (int y = minY; y < maxY; ++y) {
    float dy = (float)(y - cy);
    float dySq = dy * dy;

    // Ellipsoid Y check
    // (dx^2 + dz^2)/r^2 + dy^2/t^2 <= 1 => (dx^2 + dz^2) <= r^2 * (1 -
    // dy^2/t^2) Radius at this Y slice
    float radiusAtYFactor = 1.0f - (dySq * invThickSq);
    if (radiusAtYFactor <= 0.0f)
      continue;

    float sliceRadSq = radSq * radiusAtYFactor;

    // Get Chunk for this Y level
    Chunk *chunk = region.getChunk(minX, y, minZ);
    // Note: minX, minZ are inside the chunk (chunkX, chunkZ).

    if (!chunk)
      continue;

    int chunkYBase = (y / CHUNK_SIZE) * CHUNK_SIZE; // Base Y of current chunk
    int ly = y - chunkYBase;

    for (int x = minX; x < maxX; ++x) {
      float dx = (float)(x - cx);
      float dxSq = dx * dx;

      if (dxSq > sliceRadSq)
        continue;

      int lx = x - chunkX * CHUNK_SIZE;

      for (int z = minZ; z < maxZ; ++z) {
        float dz = (float)(z - cz);
        float dzSq = dz * dz;

        if (dxSq + dzSq > sliceRadSq)
          continue;

        int lz = z - chunkZ * CHUNK_SIZE;

        // DIRECT CHUNK ACCESS: No Map Lookup!
        const ChunkBlock &blk = chunk->getBlock(lx, ly, lz);
        block_id currentBlock = blk.getType();

        if (CanReplaceBlock(currentBlock, ore)) {
          chunk->setBlockNoMeshUpdate(lx, ly, lz, oreBlock);
        }
      }
    }
  }
}

void OreDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                            const ChunkColumn &column) {
  // Unused
}

struct LCG {
  uint32_t state;
  LCG(uint32_t seed) : state(seed) {}
  inline void Next() { state = state * 1664525u + 1013904223u; }
  inline float Float01() {
    Next();
    return (float)state / 4294967296.0f;
  }
  inline int Int(int max) {
    Next();
    return state % max;
  }
};

void OreDecorator::Decorate(WorldGenerator &generator, WorldGenRegion &region,
                            const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("Decorator_Ores_Region",
                            generator.IsProfilingEnabled());

  int chunkX = region.getCenterX();
  int chunkZ = region.getCenterZ();

  // We need to generate deposits that *originate* in neighboring chunks
  // but spill into this one, to avoid straight cutoffs.
  // Standard range is usually +/- 1 chunk for standard deposits.
  const int range = 1;

  for (int ox = -range; ox <= range; ox++) {
    for (int oz = -range; oz <= range; oz++) {
      int originX = chunkX + ox;
      int originZ = chunkZ + oz;

      // Use LCG seeded nicely
      uint32_t seed =
          generator.GetSeed() ^ (originX * 5221332) ^ (originZ * 27833211);
      LCG rng(seed);

      for (const auto &ore : oreTypes) {
        // Deposits per chunk
        // Use float accumulation for fractional counts
        float count = (float)ore.veinsPerChunk;
        while (count >= 1.0f) {
          // Generate One
          int lx = rng.Int(16);
          int lz = rng.Int(16);
          int cx = originX * CHUNK_SIZE + lx;
          int cz = originZ * CHUNK_SIZE + lz;

          float normalizedY = SampleDistribution(ore, rng.Float01());
          int cy = (int)(normalizedY * 256.0f);

          // Radius and Thickness
          // Map min/max size to radius (approximate)
          float radius =
              ore.minVeinSize / 2.0f +
              rng.Float01() * (ore.maxVeinSize - ore.minVeinSize) / 2.0f;
          float thickness = std::max(1.0f, radius * 0.5f); // Flattened disc

          // Call optimized generator
          // It handles bounds checking against the CURRENT chunk (chunkX,
          // chunkZ)
          GenerateDisc(region, chunkX, chunkZ, cx, cy, cz, ore, radius,
                       thickness);

          count -= 1.0f;
        }
        // Fractional chance
        if (rng.Float01() < count) {
          int lx = rng.Int(16);
          int lz = rng.Int(16);
          int cx = originX * CHUNK_SIZE + lx;
          int cz = originZ * CHUNK_SIZE + lz;
          float normalizedY = SampleDistribution(ore, rng.Float01());
          int cy = (int)(normalizedY * 256.0f);
          float radius =
              ore.minVeinSize / 2.0f +
              rng.Float01() * (ore.maxVeinSize - ore.minVeinSize) / 2.0f;
          float thickness = std::max(1.0f, radius * 0.5f);
          GenerateDisc(region, chunkX, chunkZ, cx, cy, cz, ore, radius,
                       thickness);
        }
      }
    }
  }
}
