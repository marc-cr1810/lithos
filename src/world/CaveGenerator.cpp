#include "Block.h"
#include "Chunk.h"
#include "WorldGenerator.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <random>

CaveGenerator::CaveGenerator(const WorldGenConfig &config)
    : seed(config.seed), caveFrequency(config.caveFrequency),
      caveThreshold(config.caveThreshold), config(config) {}

bool CaveGenerator::IsCaveAt(int x, int y, int z, int maxDepth,
                             const CaveNoiseData &noiseData) {
  // Surface deterrent: instead of a hard cutoff, use a dynamic deterrent
  // We want SOME caves to reach the surface, but not all.
  float surfaceDeterrent = 0.0f;
  float grandEntranceBonus = 0.0f; // Added for larger openings

  if (y > maxDepth - 15) {
    // Read pre-computed entrance noise (2D)
    int idx2D = noiseData.Index2D(x + 2, z + 2); // +2 offset for padding
    float entranceNoise = noiseData.entranceNoise[idx2D];

    // Broader entrance zones (0.4 -> caveEntranceNoise)
    if (entranceNoise < config.caveEntranceNoise) {
      // Stronger deterrent: 0.15f per block (was 0.1f)
      surfaceDeterrent = (float)(y - (maxDepth - 15)) * 0.15f;
    } else {
      // Within entrance zones, we also boost the cave size for grand entrances
      grandEntranceBonus =
          (entranceNoise - config.caveEntranceNoise) * 0.3f; // Up to 0.24 bonus
    }
  }

  // Depth-based interconnectivity: more caves deeper
  float depthFactor = 1.0f - ((float)y / (float)maxDepth);

  // Read pre-computed cheese cave noise (with +2 offset for padding)
  int idx = noiseData.Index3D(x + 2, y + 2, z + 2);
  float cheeseNoise = noiseData.cheeseNoise[idx];

  // Base threshold is 0.68f. Lower threshold = bigger caves.
  float cheeseThreshold = (0.68f / config.caveSize) - (depthFactor * 0.12f) +
                          surfaceDeterrent - grandEntranceBonus;

  // Minimum size filter: check nearby points to ensure cave is at least a few
  // blocks wide This prevents tiny 1-2 block holes
  bool isCheeseCave = false;
  if (cheeseNoise > cheeseThreshold) {
    // Sample a few nearby points to ensure this is part of a larger cave
    int idx1 = noiseData.Index3D(x + 4, y + 2, z + 2); // +2 blocks in X
    int idx2 = noiseData.Index3D(x + 2, y + 4, z + 2); // +2 blocks in Y
    int idx3 = noiseData.Index3D(x + 2, y + 2, z + 4); // +2 blocks in Z

    float nearby1 = noiseData.cheeseNoise[idx1];
    float nearby2 = noiseData.cheeseNoise[idx2];
    float nearby3 = noiseData.cheeseNoise[idx3];

    // Only create cave if at least 2 out of 3 nearby points also pass threshold
    int nearbyCount = 0;
    if (nearby1 > cheeseThreshold)
      nearbyCount++;
    if (nearby2 > cheeseThreshold)
      nearbyCount++;
    if (nearby3 > cheeseThreshold)
      nearbyCount++;

    isCheeseCave = (nearbyCount >= 2);
  }

  // Read pre-computed spaghetti cave noise
  float spaghettiSizeMod = noiseData.spaghettiMod[idx];
  float spaghettiWidthBonus = (spaghettiSizeMod * 0.045f * config.caveSize);
  // float spaghettiWidthBonus = (spaghettiSizeMod * 0.045f * config.caveSize);
  // // Removed

  float spaghettiNoise1 = noiseData.spaghettiNoise1[idx];
  float spaghettiNoise2 = noiseData.spaghettiNoise2[idx];

  // Base threshold is 0.05f. Higher = wider tunnels.
  float spaghettiThreshold = (0.05f * config.caveSize) + (depthFactor * 0.08f) -
                             surfaceDeterrent + grandEntranceBonus;

  // if (spaghettiThreshold < 0.005f) // Removed
  //   spaghettiThreshold =
  //       0.0f; // Effectively disable if too low (narrowed to nothing) //
  //       Removed

  bool isSpaghettiCave = (std::abs(spaghettiNoise1) < spaghettiThreshold) &&
                         (std::abs(spaghettiNoise2) < spaghettiThreshold);

  // Combine cave types (cheese + spaghetti only)
  return isCheeseCave || isSpaghettiCave;
}

// Single-point IsCaveAt (slow path fallback)
bool CaveGenerator::IsCaveAt(int x, int y, int z, int maxDepth) {
  float surfaceDeterrent = 0.0f;
  float grandEntranceBonus = 0.0f;

  if (y > maxDepth - 15) {
    float entranceNoise =
        generator->FastNoise2D((float)x * 0.012f, (float)z * 0.012f, 5000);
    if (entranceNoise < config.caveEntranceNoise) {
      // Stronger deterrent: 0.15f per block (was 0.1f)
      surfaceDeterrent = (float)(y - (maxDepth - 15)) * 0.15f;
    } else {
      grandEntranceBonus = (entranceNoise - config.caveEntranceNoise) * 0.3f;
    }
  }

  int seedX = (seed * 7777) % 65536;
  int seedY = (seed * 8888) % 65536;
  int seedZ = (seed * 9999) % 65536;
  glm::vec3 pos((float)(x + seedX), (float)(y + seedY), (float)(z + seedZ));

  float depthFactor = 1.0f - ((float)y / (float)maxDepth);
  float cheeseScale = 0.01f * (config.caveFrequency / 0.015f);
  float cheeseNoise = generator->FastNoise3D(
      pos.x * cheeseScale, pos.y * cheeseScale, pos.z * cheeseScale, 1000);
  float cheeseThreshold = (0.68f / config.caveSize) - (depthFactor * 0.12f) +
                          surfaceDeterrent - grandEntranceBonus;

  bool isCheeseCave = false;
  if (cheeseNoise > cheeseThreshold) {
    float n1 =
        generator->FastNoise3D((pos.x + 2.0f) * cheeseScale,
                               pos.y * cheeseScale, pos.z * cheeseScale, 1000);
    float n2 = generator->FastNoise3D(pos.x * cheeseScale,
                                      (pos.y + 2.0f) * cheeseScale,
                                      pos.z * cheeseScale, 1000);
    float n3 = generator->FastNoise3D(pos.x * cheeseScale, pos.y * cheeseScale,
                                      (pos.z + 2.0f) * cheeseScale, 1000);

    int count = 0;
    if (n1 > cheeseThreshold)
      count++;
    if (n2 > cheeseThreshold)
      count++;
    if (n3 > cheeseThreshold)
      count++;
    isCheeseCave = (count >= 2);
  }

  float spagModScale = 0.01f * (config.caveFrequency / 0.015f);
  float spagNoiseScale = 0.03f * (config.caveFrequency / 0.015f);
  float spaghettiSizeMod = generator->FastNoise3D(
      pos.x * spagModScale, pos.y * spagModScale, pos.z * spagModScale, 2000);
  // float spaghettiWidthBonus = (spaghettiSizeMod * 0.045f * config.caveSize);
  // // Not used in slow path
  float spaghettiNoise1 =
      generator->FastNoise3D(pos.x * spagNoiseScale, pos.y * spagNoiseScale,
                             pos.z * spagNoiseScale, 3000);
  float spaghettiNoise2 = generator->FastNoise3D(
      (pos.x + 100.0f) * spagNoiseScale, (pos.y + 100.0f) * spagNoiseScale,
      (pos.z + 100.0f) * spagNoiseScale, 3000);

  float spaghettiThreshold = (0.05f * config.caveSize) + (depthFactor * 0.08f) -
                             surfaceDeterrent + grandEntranceBonus;
  bool isSpaghettiCave = (std::abs(spaghettiNoise1) < spaghettiThreshold) &&
                         (std::abs(spaghettiNoise2) < spaghettiThreshold);

  return isCheeseCave || isSpaghettiCave;
}

bool CaveGenerator::IsRavineAt(int gx, int gy, int gz, int surfaceHeight) {
  // Original implementation (slow path)
  if (gy > surfaceHeight || gy < surfaceHeight - config.ravineDepth) {
    return false;
  }

  // Surface Deterrent for Ravines: Pierce top 1 block ONLY if in an entrance
  // zone
  if (gy >= surfaceHeight - 1) {
    float entranceNoise =
        generator->FastNoise2D((float)gx * 0.012f, (float)gz * 0.012f, 5000);
    if (entranceNoise < config.caveEntranceNoise) {
      return false;
    }
  }

  int seedX = (seed * 3333) % 65536;
  int seedZ = (seed * 4444) % 65536;
  glm::vec2 pos2D((float)(gx + seedX), (float)(gz + seedZ));
  float ravineScale = 0.006f * (config.caveFrequency / 0.015f);
  float ravineNoise1 = glm::perlin(pos2D * ravineScale);
  float ravineNoise2 =
      glm::perlin(pos2D * ravineScale + glm::vec2(500.0f, 500.0f));

  float ravinePathWidth = 0.05f * config.ravineWidth;
  if (std::abs(ravineNoise1) >= ravinePathWidth ||
      std::abs(ravineNoise2) >= ravinePathWidth) {
    return false;
  }

  float depthRatio = (float)(surfaceHeight - gy) / (float)config.ravineDepth;
  float widthAtDepth = (3.0f + (1.0f - depthRatio) * 5.0f) * config.ravineWidth;
  float horizontalNoise =
      glm::perlin(glm::vec3(pos2D * 0.05f, (float)gy * 0.1f));

  return std::abs(horizontalNoise) < (widthAtDepth / 10.0f);
}

bool CaveGenerator::IsRavineAt(int bx, int by, int bz, int gx, int gy, int gz,
                               int surfaceHeight,
                               const CaveNoiseData &noiseData) {
  // Batched implementation (fast path)
  if (gy > surfaceHeight || gy < surfaceHeight - config.ravineDepth) {
    return false;
  }

  // Surface Deterrent for Ravines using BATCHED entrance noise
  if (gy >= surfaceHeight - 1) {
    int idx2D = noiseData.Index2D(bx + 2, bz + 2);
    float entranceNoise = noiseData.entranceNoise[idx2D];
    if (entranceNoise < config.caveEntranceNoise) {
      return false;
    }
  }

  // Rest of the logic remains on-the-fly (perlin is reasonably fast, but could
  // be batched if needed)
  int seedX = (seed * 3333) % 65536;
  int seedZ = (seed * 4444) % 65536;
  glm::vec2 pos2D((float)(gx + seedX), (float)(gz + seedZ));
  float ravineScale = 0.006f * (config.caveFrequency / 0.015f);
  float ravineNoise1 = glm::perlin(pos2D * ravineScale);
  float ravineNoise2 =
      glm::perlin(pos2D * ravineScale + glm::vec2(500.0f, 500.0f));

  float ravinePathWidth = 0.05f * config.ravineWidth;
  if (std::abs(ravineNoise1) >= ravinePathWidth ||
      std::abs(ravineNoise2) >= ravinePathWidth) {
    return false;
  }

  float depthRatio = (float)(surfaceHeight - gy) / (float)config.ravineDepth;
  float widthAtDepth = (3.0f + (1.0f - depthRatio) * 5.0f) * config.ravineWidth;
  float horizontalNoise =
      glm::perlin(glm::vec3(pos2D * 0.05f, (float)gy * 0.1f));

  return std::abs(horizontalNoise) < (widthAtDepth / 10.0f);
}

void CaveGenerator::GenerateWormCave(Chunk &chunk, int startX, int startY,
                                     int startZ, int maxDepth) {
  // Don't start caves too close to surface
  if (startY > maxDepth - 15) {
    return;
  }

  std::mt19937 rng(seed + startX * 1000 + startY * 100 + startZ);
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  std::uniform_real_distribution<float> radiusDist(1.5f, 3.5f);
  std::uniform_int_distribution<int> lengthDist(30, 80);

  glm::vec3 pos(startX, startY, startZ);

  // Initial direction with downward bias
  glm::vec3 direction =
      glm::normalize(glm::vec3(dist(rng),
                               dist(rng) - 0.4f, // Bias downward
                               dist(rng)));

  float radius = radiusDist(rng);
  int length = lengthDist(rng);
  int taperStart = length - 15; // Start tapering near end

  for (int i = 0; i < length; ++i) {
    // Carve sphere at current position
    CarveSphere(chunk, pos, radius);

    // Move forward
    pos += direction * 0.8f;

    // Randomly adjust direction
    direction +=
        glm::vec3(dist(rng) * 0.15f, dist(rng) * 0.1f, dist(rng) * 0.15f);
    direction = glm::normalize(direction);

    // Natural tapering near end
    if (i > taperStart) {
      radius *= 0.92f;
    }

    // Stop if we go too high or too low
    if (pos.y > maxDepth - 5 || pos.y < 5) {
      break;
    }

    // Branching - more likely deeper down
    float depthFactor = 1.0f - (pos.y / (float)maxDepth);
    float branchChance = depthFactor * 0.08f; // Up to 8% chance deep down

    std::uniform_real_distribution<float> branchDist(0.0f, 1.0f);
    if (branchDist(rng) < branchChance && i > 10) {
      // Create a branch
      GenerateWormCave(chunk, (int)pos.x, (int)pos.y, (int)pos.z, maxDepth);
    }
  }
}

void CaveGenerator::CarveSphere(Chunk &chunk, glm::vec3 center, float radius) {
  int minX = std::max(0, (int)(center.x - radius));
  int maxX = std::min(CHUNK_SIZE - 1, (int)(center.x + radius));
  int minY = std::max(0, (int)(center.y - radius));
  int maxY = std::min(CHUNK_SIZE - 1, (int)(center.y + radius));
  int minZ = std::max(0, (int)(center.z - radius));
  int maxZ = std::min(CHUNK_SIZE - 1, (int)(center.z + radius));

  for (int x = minX; x <= maxX; ++x) {
    for (int y = minY; y <= maxY; ++y) {
      for (int z = minZ; z <= maxZ; ++z) {
        float dist = glm::distance(glm::vec3(x, y, z), center);
        if (dist <= radius) {
          // Only carve if it's a solid block (don't replace air/water)
          ChunkBlock currentBlock = chunk.getBlock(x, y, z);
          if (currentBlock.getType() != AIR &&
              currentBlock.getType() != WATER) {
            chunk.setBlock(x, y, z, AIR);
          }
        }
      }
    }
  }
}
