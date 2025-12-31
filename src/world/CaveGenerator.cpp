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

bool CaveGenerator::IsCaveAt(int x, int y, int z, int maxDepth) {
  // Surface deterrent: instead of a hard cutoff, use a dynamic deterrent
  // We want SOME caves to reach the surface, but not all.
  float surfaceDeterrent = 0.0f;
  float grandEntranceBonus = 0.0f; // Added for larger openings

  if (y > maxDepth - 15) {
    // 2D noise to decide where caves ARE allowed to reach the surface
    // (entrances) Slightly lower frequency for larger "entrance regions" (0.015
    // -> 0.012)
    float entranceNoise =
        glm::perlin(glm::vec2((float)x * 0.012f, (float)z * 0.012f));

    // Broader entrance zones (0.4 -> caveEntranceNoise)
    if (entranceNoise < config.caveEntranceNoise) {
      surfaceDeterrent = (float)(y - (maxDepth - 15)) * 0.1f;
    } else {
      // Within entrance zones, we also boost the cave size for grand entrances
      grandEntranceBonus =
          (entranceNoise - config.caveEntranceNoise) * 0.3f; // Up to 0.24 bonus
    }
  }

  // 3D Perlin noise for cave placement
  int seedX = (seed * 7777) % 65536;
  int seedY = (seed * 8888) % 65536;
  int seedZ = (seed * 9999) % 65536;

  glm::vec3 pos((float)(x + seedX), (float)(y + seedY), (float)(z + seedZ));

  // Depth-based interconnectivity: more caves deeper
  float depthFactor = 1.0f - ((float)y / (float)maxDepth);

  // Layer 1: "Cheese" caves - Swiss cheese style, larger caverns
  // Low frequency, creates large blob-like caves. Base is 0.01f
  float cheeseScale = 0.01f * (config.caveFrequency / 0.015f);
  float cheeseNoise = generator->FastNoise3D(
      pos.x * cheeseScale, pos.y * cheeseScale, pos.z * cheeseScale, 1000);

  // Base threshold is 0.68f. Lower threshold = bigger caves.
  float cheeseThreshold = (0.68f / config.caveSize) - (depthFactor * 0.12f) +
                          surfaceDeterrent - grandEntranceBonus;

  // Minimum size filter: check nearby points to ensure cave is at least a few
  // blocks wide This prevents tiny 1-2 block holes
  bool isCheeseCave = false;
  if (cheeseNoise > cheeseThreshold) {
    // Sample a few nearby points to ensure this is part of a larger cave
    float nearby1 =
        generator->FastNoise3D((pos.x + 2.0f) * cheeseScale,
                               pos.y * cheeseScale, pos.z * cheeseScale, 1000);
    float nearby2 = generator->FastNoise3D(pos.x * cheeseScale,
                                           (pos.y + 2.0f) * cheeseScale,
                                           pos.z * cheeseScale, 1000);
    float nearby3 =
        generator->FastNoise3D(pos.x * cheeseScale, pos.y * cheeseScale,
                               (pos.z + 2.0f) * cheeseScale, 1000);

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

  // Layer 2: "Spaghetti" caves - Varied width winding tunnels
  // Base scale is 0.01f for mod, 0.03f for noise
  float spagModScale = 0.01f * (config.caveFrequency / 0.015f);
  float spagNoiseScale = 0.03f * (config.caveFrequency / 0.015f);

  float spaghettiSizeMod = generator->FastNoise3D(
      pos.x * spagModScale, pos.y * spagModScale, pos.z * spagModScale, 2000);
  float spaghettiWidthBonus = (spaghettiSizeMod * 0.045f * config.caveSize);

  // Use ridged noise (abs) for tunnel-like structures
  float spaghettiNoise1 =
      generator->FastNoise3D(pos.x * spagNoiseScale, pos.y * spagNoiseScale,
                             pos.z * spagNoiseScale, 3000);
  float spaghettiNoise2 = generator->FastNoise3D(
      (pos.x + 100.0f) * spagNoiseScale, (pos.y + 100.0f) * spagNoiseScale,
      (pos.z + 100.0f) * spagNoiseScale, 3000);

  // Base threshold is 0.05f. Higher = wider tunnels.
  float spaghettiThreshold = (0.05f * config.caveSize) +
                             (depthFactor * 0.025f) + spaghettiWidthBonus -
                             surfaceDeterrent;

  if (spaghettiThreshold < 0.005f)
    spaghettiThreshold =
        0.0f; // Effectively disable if too low (narrowed to nothing)

  bool isSpaghettiCave = (std::abs(spaghettiNoise1) < spaghettiThreshold) &&
                         (std::abs(spaghettiNoise2) < spaghettiThreshold);

  // Combine cave types (cheese + spaghetti only)
  return isCheeseCave || isSpaghettiCave;
}

bool CaveGenerator::IsRavineAt(int x, int y, int z, int surfaceHeight) {
  // Ravines are surface features that cut deep into the ground
  // Only generate ravines from surface down to a certain depth
  if (y > surfaceHeight || y < surfaceHeight - config.ravineDepth) {
    return false;
  }

  int seedX = (seed * 3333) % 65536;
  int seedZ = (seed * 4444) % 65536;

  glm::vec2 pos2D((float)(x + seedX), (float)(z + seedZ));

  // Use 2D ridged noise to create long, winding ravines
  // Base scale is 0.006f
  float ravineScale = 0.006f * (config.caveFrequency / 0.015f);
  float ravineNoise1 = glm::perlin(pos2D * ravineScale);
  float ravineNoise2 =
      glm::perlin(pos2D * ravineScale + glm::vec2(500.0f, 500.0f));

  // Ravines are where noise is close to zero (ridged)
  // Base width is 0.05f
  float ravinePathWidth = 0.05f * config.ravineWidth;
  bool isOnRavinePath = (std::abs(ravineNoise1) < ravinePathWidth) &&
                        (std::abs(ravineNoise2) < ravinePathWidth);

  if (!isOnRavinePath) {
    return false;
  }

  // Depth profile: wider at top, narrower at bottom
  float depthRatio = (float)(surfaceHeight - y) / (float)config.ravineDepth;
  float widthAtDepth = (3.0f + (1.0f - depthRatio) * 5.0f) *
                       config.ravineWidth; // 3-8 blocks wide base

  // Add some horizontal variation
  float horizontalNoise =
      glm::perlin(glm::vec3(pos2D * 0.05f, (float)y * 0.1f));

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
