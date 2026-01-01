#include "CaveGenerator.h"
#include "Block.h"
#include "Chunk.h"
#include "gen/NoiseManager.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <random>

CaveGenerator::CaveGenerator(const WorldGenConfig &config)
    : seed(config.seed), caveFrequency(config.caveFrequency),
      caveThreshold(config.caveThreshold), config(config) {
  // No internal init needed anymore
}

void CaveGenerator::GenerateCaves(Chunk &chunk,
                                  const NoiseManager &noiseManager) {
  int cx = chunk.chunkPosition.x;
  int cy = chunk.chunkPosition.y;
  int cz = chunk.chunkPosition.z;

  int startX = cx * CHUNK_SIZE;
  int startY = cy * CHUNK_SIZE;
  int startZ = cz * CHUNK_SIZE;

  // Thread local buffers to avoid allocation
  static thread_local std::vector<float> cheeseMap(CHUNK_SIZE * CHUNK_SIZE *
                                                   CHUNK_SIZE);
  static thread_local std::vector<float> spag1Map(CHUNK_SIZE * CHUNK_SIZE *
                                                  CHUNK_SIZE);
  static thread_local std::vector<float> spag2Map(CHUNK_SIZE * CHUNK_SIZE *
                                                  CHUNK_SIZE);
  static thread_local std::vector<float> entranceMap(CHUNK_SIZE * CHUNK_SIZE);

  // 1. Generate Noise Buffers
  float cheeseScale = 0.01f * (config.caveFrequency / 0.015f);
  noiseManager.GenCave3D(cheeseMap.data(), startX, startY, startZ, CHUNK_SIZE,
                         CHUNK_SIZE, CHUNK_SIZE, cheeseScale);

  // Spaghetti offset logic (1000 offset as in original)
  noiseManager.GenCave3D(spag1Map.data(), startX + 1000, startY, startZ,
                         CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, 0.02f);
  noiseManager.GenCave3D(spag2Map.data(), startX, startY + 1000, startZ,
                         CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, 0.02f);

  noiseManager.GenCaveEntrance(entranceMap.data(), startX, startZ, CHUNK_SIZE,
                               CHUNK_SIZE);

  // 2. Iterate and Carve
  int maxDepth = config.worldHeight;

  // Cache Block Pointers for speed (direct access)
  Block *lavaBlock = BlockRegistry::getInstance().getBlock(BlockType::LAVA);
  Block *airBlock = BlockRegistry::getInstance().getBlock(BlockType::AIR);
  Block *waterBlock = BlockRegistry::getInstance().getBlock(BlockType::WATER);
  Block *iceBlock = BlockRegistry::getInstance().getBlock(BlockType::ICE);

  for (int lx = 0; lx < CHUNK_SIZE; lx++) {
    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
      int wx = startX + lx;
      int wz = startZ + lz;

      // Entrance logic (2D)
      float entranceNoise = entranceMap[lx + lz * CHUNK_SIZE];
      float surfaceDeterrent = 0.0f;
      float grandEntranceBonus = 0.0f;

      // Depth varies by Y, but entrance noise is fixed for the column.
      // We can pre-calc the "potential" bonus/deterrent part that is dependent
      // on entranceNoise, but the Y-dependency means we calculate inside loop
      // or inner loop. Inside Y loop is fine, it's fast math.

      for (int ly = 0; ly < CHUNK_SIZE; ly++) {
        int wy = startY + ly;
        int index = lx + (ly * CHUNK_SIZE) + (lz * CHUNK_SIZE * CHUNK_SIZE);
        // Note: GenUniformGrid3D output order is X, Y, Z usually?
        // FastNoise2 GenUniformGrid3D: output[((z * height) + y) * width + x]
        // My index above: x + y*width + z*width*height
        // Wait, FastNoise Docs say: "x is the fastest moving axis"
        // So index = x + y*width + z*width*height is correct for standard
        // mapping? Let's verify FastNoise convention: "output array of size
        // width*height*depth" assuming x,y,z iteration order Typically: index =
        // x + width * (y + height * z) Let's use that to be safe and standard.
        int fnIndex = lx + CHUNK_SIZE * (ly + CHUNK_SIZE * lz);

        if (wy <= 5)
          continue;

        // Verify we aren't cutting into water/ocean (Block Check)
        // Direct Access: chunk.blocks[lx][ly][lz]
        Block *currentBlock = chunk.blocks[lx][ly][lz].block;
        if (currentBlock == waterBlock || currentBlock == iceBlock)
          continue;

        // Recalculate Logic
        if (wy > maxDepth - 15) {
          if (entranceNoise < config.caveEntranceNoise) {
            surfaceDeterrent = (float)(wy - (maxDepth - 15)) * 0.15f;
            grandEntranceBonus = 0.0f;
          } else {
            surfaceDeterrent = 0.0f;
            grandEntranceBonus =
                (entranceNoise - config.caveEntranceNoise) * 0.3f;
          }
        } else {
          surfaceDeterrent = 0.0f;
          grandEntranceBonus = 0.0f;
        }

        float depthFactor = 1.0f - ((float)wy / (float)maxDepth);

        // Cheese Logic
        float cheeseVal = cheeseMap[fnIndex];
        float cheeseThreshold = (0.68f / config.caveSize) -
                                (depthFactor * 0.12f) + surfaceDeterrent -
                                grandEntranceBonus;

        bool isCheese = (cheeseVal > cheeseThreshold);
        // Note: The original code checked neighbors [x+2] etc for density
        // filtering ("tiny holes").
        // This is expensive to replicate exactly in batch without multiple
        // samples. Simplifying: We skip the neighbor check for performance.
        // The fractal nature usually handles continuity.
        // If "tiny holes" are an issue, efficient neighbor checking in buffer
        // is possible but complexity is high for minor visual gain.
        // DECISION: Skip neighbor density check for now, trust FractalFBm.

        // Spaghetti Logic
        float spag1 = spag1Map[fnIndex];
        float spag2 = spag2Map[fnIndex];
        float spagThreshold = (0.05f * config.caveSize) +
                              (depthFactor * 0.08f) - surfaceDeterrent +
                              grandEntranceBonus;

        bool isSpaghetti = (std::abs(spag1) < spagThreshold) &&
                           (std::abs(spag2) < spagThreshold);

        if (isCheese || isSpaghetti) {
          if (wy < config.lavaLevel) {
            chunk.blocks[lx][ly][lz].block = lavaBlock;
            chunk.blocks[lx][ly][lz].metadata = 0;
          } else {
            chunk.blocks[lx][ly][lz].block = airBlock;
            chunk.blocks[lx][ly][lz].metadata = 0;
          }
        }
      }
    }
  }
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
