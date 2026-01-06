#include "FloraDecorator.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "Block.h"
#include "ChunkColumn.h"
#include "World.h"
#include "WorldGenRegion.h"
#include "WorldGenerator.h"
#include <cstdlib>
#include <fstream>
#include <glm/glm.hpp>
#include <random>

#include <cstring>
#include <iostream>
#include <vector>

// Static member definitions
std::vector<FloraType> FloraDecorator::floraTypes;
int FloraDecorator::meanPatchesPerChunk = 15;
int FloraDecorator::patchVariance = 8;
float FloraDecorator::totalWeight = 0.0f;

FloraDecorator::FloraDecorator() {
  // Config loaded at app startup via Application.cpp
}

void FloraDecorator::LoadConfig(const std::filesystem::path &configPath) {
  std::ifstream file(configPath);
  if (!file.is_open()) {
    LOG_WARN("Failed to load flora config: {}", configPath.string());
    return;
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const std::exception &e) {
    LOG_ERROR("JSON parse error in {}: {}", configPath.string(), e.what());
    return;
  }

  // Load patches per chunk settings
  if (j.contains("patchesPerChunk")) {
    auto &ppc = j["patchesPerChunk"];
    meanPatchesPerChunk = ppc.value("mean", 15);
    patchVariance = ppc.value("variance", 8);
  }

  // Load flora types
  if (j.contains("floraTypes")) {
    for (const auto &ft : j["floraTypes"]) {
      FloraType flora;
      flora.code = ft.value("code", "");
      flora.blockId = ft.value("block", "");
      flora.weight = ft.value("weight", 1.0f);

      // Spawn conditions
      if (ft.contains("spawnConditions")) {
        auto &sc = ft["spawnConditions"];
        flora.minTemp = sc.value("minTemp", -100.0f);
        flora.maxTemp = sc.value("maxTemp", 100.0f);
        flora.minRain = sc.value("minRain", 0.0f);
        flora.maxRain = sc.value("maxRain", 1.0f);
        flora.minY = sc.value("minY", 0.0f);
        flora.maxY = sc.value("maxY", 1.0f);

        if (sc.contains("allowedSurfaceBlocks")) {
          flora.allowedSurfaceBlocks =
              sc["allowedSurfaceBlocks"].get<std::vector<std::string>>();
        }
      }

      // Patch settings
      if (ft.contains("patchSettings")) {
        auto &ps = ft["patchSettings"];
        flora.minPatchSize = ps.value("minSize", 1);
        flora.maxPatchSize = ps.value("maxSize", 4);
        flora.density = ps.value("density", 0.5f);
      }

      floraTypes.push_back(flora);
      totalWeight += flora.weight;
    }

    // Resolve Block IDs
    for (auto &flora : floraTypes) {
      // Resolve main block ID
      Block *b = BlockRegistry::getInstance().getBlock(flora.blockId);
      if (b) {
        flora.resolvedBlockId = b->getId();
      } else {
        LOG_WARN("FloraDecorator: Could not resolve block '{}'", flora.blockId);
      }

      // Pre-calculate allowed surface blocks table
      for (int i = 0; i < 256; ++i) {
        Block *candidate = BlockRegistry::getInstance().getBlock(i);
        if (!candidate)
          continue;

        std::string candidateId = candidate->getResourceId();
        bool allowed = false;

        // If list is empty, assume ALL surface blocks allowed? Or NONE?
        // Logic in CanPlaceAt says if list not empty check it.
        // If list IS empty, CanPlaceAt logic currently returns true for surface
        // block check (but logic is `if (!empty) { check; if !allowed return
        // false; } return true;`) So empty list means NO restrictions.

        if (flora.allowedSurfaceBlocks.empty()) {
          allowed = true;
        } else {
          for (const auto &allowedId : flora.allowedSurfaceBlocks) {
            if (allowedId ==
                candidateId) { // Exact match? Or Pattern? Original used ==
              allowed = true;
              break;
            }
          }
        }
        flora.resolvedAllowedSurfaceBlocks[i] = allowed;
      }
    }
  }

  LOG_INFO("Loaded {} flora types from {}", floraTypes.size(),
           configPath.string());
}

FloraType *FloraDecorator::SelectFlora(float temp, float rain, float y) {
  // Build a list of eligible flora
  std::vector<FloraType *> eligible;
  float eligibleWeight = 0.0f;

  for (auto &flora : floraTypes) {
    if (temp >= flora.minTemp && temp <= flora.maxTemp &&
        rain >= flora.minRain && rain <= flora.maxRain && y >= flora.minY &&
        y <= flora.maxY) {
      eligible.push_back(&flora);
      eligibleWeight += flora.weight;
    }
  }

  if (eligible.empty() || eligibleWeight <= 0.0f) {
    return nullptr;
  }

  // Weighted random selection
  float r = (float)(rand() % 10000) / 10000.0f * eligibleWeight;
  float cumulative = 0.0f;

  for (auto *flora : eligible) {
    cumulative += flora->weight;
    if (r <= cumulative) {
      return flora;
    }
  }

  return eligible.back();
}

bool FloraDecorator::CanPlaceAt(int x, int y, int z, const FloraType &flora,
                                WorldGenRegion &region,
                                const ChunkColumn &column) {
  // Check if there's a solid block below
  BlockType blockBelow = region.getBlock(x, y - 1, z);
  if (blockBelow == AIR || blockBelow == WATER || blockBelow == LAVA) {
    return false;
  }

  // Check if current position is air
  BlockType currentBlock = region.getBlock(x, y, z);
  if (currentBlock != AIR) {
    return false;
  }

  // Check allowed surface blocks
  // Use pre-resolved table
  // The original check "if (!empty)" implies if empty, anything goes.
  // Our resolved table should reflect that.

  if (!flora.resolvedAllowedSurfaceBlocks[(uint8_t)blockBelow]) {
    return false;
  }

  return true;
}

void FloraDecorator::PlacePatch(WorldGenRegion &region, int centerX,
                                int centerY, int centerZ,
                                const FloraType &flora,
                                const ChunkColumn &column) {
  // Determine patch size
  int patchSize = flora.minPatchSize +
                  (rand() % (flora.maxPatchSize - flora.minPatchSize + 1));

  // Get block type from registry
  BlockType floraType = (BlockType)flora.resolvedBlockId;
  if (floraType == 0 && flora.blockId != "air") {
    Block *b = BlockRegistry::getInstance().getBlock(flora.blockId);
    if (b)
      floraType = (BlockType)b->getId();
  }

  // Place flora in a patch around center
  for (int attempt = 0; attempt < patchSize * 3; ++attempt) {
    int dx = (rand() % (patchSize * 2 + 1)) - patchSize;
    int dz = (rand() % (patchSize * 2 + 1)) - patchSize;
    int px = centerX + dx;
    int pz = centerZ + dz;

    // Get height at this location
    int localX = ((px % 16) + 16) % 16;
    int localZ = ((pz % 16) + 16) % 16;
    int py = column.getHeight(localX, localZ) + 1;

    // Check density
    float roll = (float)(rand() % 100) / 100.0f;
    if (roll > flora.density) {
      continue;
    }

    // Try to place
    if (CanPlaceAt(px, py, pz, flora, region, column)) {
      region.setBlock(px, py, pz, floraType);
    }
  }
}

void FloraDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                              const ChunkColumn &column) {
  // Old method - kept for compatibility but not used
}

void FloraDecorator::Decorate(WorldGenerator &generator, WorldGenRegion &region,
                              const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("Decorator_Flora_Region",
                            generator.IsProfilingEnabled());

  int colX = region.getCenterX();
  int colZ = region.getCenterZ();

  // Determine number of patches for this chunk
  static thread_local std::mt19937 rng(std::random_device{}());
  std::normal_distribution<float> dist(meanPatchesPerChunk,
                                       sqrt(patchVariance));
  int numPatches = std::max(0, (int)dist(rng));

  // Place patches
  for (int i = 0; i < numPatches; ++i) {
    // Random position in chunk
    int x = colX * CHUNK_SIZE + (rand() % CHUNK_SIZE);
    int z = colZ * CHUNK_SIZE + (rand() % CHUNK_SIZE);

    // Get climate data
    int localX = ((x % 16) + 16) % 16;
    int localZ = ((z % 16) + 16) % 16;
    float temp = column.temperatureMap[localX][localZ];
    float rain = column.humidityMap[localX][localZ];
    int height = column.getHeight(localX, localZ);
    float y = (float)height / 256.0f; // Normalize to 0-1

    // Select appropriate flora
    FloraType *flora = SelectFlora(temp, rain, y);
    if (!flora) {
      continue;
    }

    // Place patch
    PlacePatch(region, x, height + 1, z, *flora, column);
  }
}
