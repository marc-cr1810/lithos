#include "CaveGenerator.h"
#include "Block.h"
#include "Chunk.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <random>

CaveGenerator::CaveGenerator(const WorldGenConfig &config)
    : seed(config.seed), caveFrequency(config.caveFrequency),
      caveThreshold(config.caveThreshold), config(config) {
  InitNoise();
}

void CaveGenerator::InitNoise() {
  auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
  auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();
  fnFractal->SetSource(fnSimplex);
  fnFractal->SetOctaveCount(3);
  fn3D = fnFractal; // Use fractal for 3D noise

  auto fnPerlin = FastNoise::New<FastNoise::Perlin>();
  fn2D = fnPerlin;
}

// Single-point IsCaveAt
bool CaveGenerator::IsCaveAt(int x, int y, int z, int maxDepth) {
  float surfaceDeterrent = 0.0f;
  float grandEntranceBonus = 0.0f;

  int seedX = (seed * 7777) % 65536;
  int seedZ = (seed * 9999) % 65536;

  // Entrance Noise check
  if (y > maxDepth - 15) {
    float entranceNoise = fn2D->GenSingle2D((float)(x + seedX) * 0.012f,
                                            (float)(z + seedZ) * 0.012f, seed);
    if (entranceNoise < config.caveEntranceNoise) {
      // Stronger deterrent
      surfaceDeterrent = (float)(y - (maxDepth - 15)) * 0.15f;
    } else {
      grandEntranceBonus = (entranceNoise - config.caveEntranceNoise) * 0.3f;
    }
  }

  glm::vec3 pos((float)(x + seedX), (float)(y), (float)(z + seedZ));
  // removed seedY offset for simplicity or keep it if needed

  float depthFactor = 1.0f - ((float)y / (float)maxDepth);
  float cheeseScale = 0.01f * (config.caveFrequency / 0.015f);

  float cheeseNoise = fn3D->GenSingle3D(
      pos.x * cheeseScale, pos.y * cheeseScale, pos.z * cheeseScale, seed);

  float cheeseThreshold = (0.68f / config.caveSize) - (depthFactor * 0.12f) +
                          surfaceDeterrent - grandEntranceBonus;

  bool isCheeseCave = false;
  if (cheeseNoise > cheeseThreshold) {
    // Check neighbors to avoid tiny holes
    int count = 0;
    if (fn3D->GenSingle3D((pos.x + 2) * cheeseScale, pos.y * cheeseScale,
                          pos.z * cheeseScale, seed) > cheeseThreshold)
      count++;
    if (fn3D->GenSingle3D(pos.x * cheeseScale, (pos.y + 2) * cheeseScale,
                          pos.z * cheeseScale, seed) > cheeseThreshold)
      count++;
    if (fn3D->GenSingle3D(pos.x * cheeseScale, pos.y * cheeseScale,
                          (pos.z + 2) * cheeseScale, seed) > cheeseThreshold)
      count++;

    isCheeseCave = (count >= 2);
  }

  // Spaghetti logic (simplified for now or similar)
  // Usually requires another noise source or just reuse fn3D with different
  // offset? Use fn3D with offset for spaghetti
  float spagNoise1 = fn3D->GenSingle3D(pos.x * 0.02f + 1000, pos.y * 0.02f,
                                       pos.z * 0.02f, seed);
  float spagNoise2 = fn3D->GenSingle3D(pos.x * 0.02f, pos.y * 0.02f + 1000,
                                       pos.z * 0.02f, seed);

  float spaghettiThreshold = (0.05f * config.caveSize) + (depthFactor * 0.08f) -
                             surfaceDeterrent + grandEntranceBonus;

  bool isSpaghettiCave = (std::abs(spagNoise1) < spaghettiThreshold) &&
                         (std::abs(spagNoise2) < spaghettiThreshold);

  return isCheeseCave || isSpaghettiCave;
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
