#include "CaveGenerator.h"
#include "Block.h"
#include "Chunk.h"
#include "ChunkColumn.h"
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

void CaveGenerator::GenerateCaves(Chunk &chunk, const ChunkColumn &column,
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

  // 1. Generate Noise Buffers (Standard X/Y/Z)
  float cheeseScale = 0.01f * (config.caveFrequency / 0.015f);
  noiseManager.GenCave3D(cheeseMap.data(), startX, startY, startZ, CHUNK_SIZE,
                         CHUNK_SIZE, CHUNK_SIZE, cheeseScale);

  // Spaghetti offset
  noiseManager.GenCave3D(spag1Map.data(), startX + 1000, startY, startZ,
                         CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, 0.02f);
  noiseManager.GenCave3D(spag2Map.data(), startX, startY + 1000, startZ,
                         CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, 0.02f);

  noiseManager.GenCaveEntrance(entranceMap.data(), startX, startZ, CHUNK_SIZE,
                               CHUNK_SIZE);

  // cache pointers
  Block *lavaBlock = BlockRegistry::getInstance().getBlock(BlockType::LAVA);
  Block *airBlock = BlockRegistry::getInstance().getBlock(BlockType::AIR);
  Block *waterBlock = BlockRegistry::getInstance().getBlock(BlockType::WATER);
  Block *iceBlock = BlockRegistry::getInstance().getBlock(BlockType::ICE);

  int maxDepth = config.worldHeight;

  // Outer Loops: X and Z (Column)
  for (int lx = 0; lx < CHUNK_SIZE; lx++) {
    for (int lz = 0; lz < CHUNK_SIZE; lz++) {

      // Get Surface Height for this column!
      int surfaceHeight = column.getHeight(lx, lz);

      // Dynamic Transition: Apply surface logic relative to ACTUAL terrain
      // height This enables the "Deterrent" to work at Y=64, preventing swiss
      // cheese, while allowing "Grand Entrances" to punch through.
      int transitionY = surfaceHeight - 15;

      // Optimization: Don't process sky (strictly above surface)
      // But allow processing AT surface (wy == surfaceHeight) for entrances
      int processingCeiling = surfaceHeight;

      float entranceNoise = entranceMap[lx + lz * CHUNK_SIZE]; // 2D Index

      int fnIndexBase = lx + CHUNK_SIZE * (0 + CHUNK_SIZE * lz); // Base at ly=0

      for (int ly = 0; ly < CHUNK_SIZE; ly++) {
        int wy = startY + ly;
        if (wy <= 5)
          continue; // Bedrock skip

        // Optimization: Skip processing above the terrain
        if (wy > processingCeiling)
          continue;

        Block *currentBlock = chunk.blocks[lx][ly][lz].block;
        if (currentBlock == waterBlock || currentBlock == iceBlock)
          continue;

        // Index Stride is Width=32
        int fnIndex = fnIndexBase + (ly * CHUNK_SIZE);

        float depthFactor = 1.0f - ((float)wy / (float)maxDepth);
        float surfaceDeterrent = 0.0f;
        float grandEntranceBonus = 0.0f;

        // Surface Logic (Applied relative to actual surface now)
        if (wy > transitionY) {
          if (entranceNoise < config.caveEntranceNoise) {
            // Deter normal caves near surface
            surfaceDeterrent = (float)(wy - transitionY) * 0.15f;
          } else {
            // Allow entrances
            grandEntranceBonus =
                (entranceNoise - config.caveEntranceNoise) * 0.3f;
          }
        }

        float combined = surfaceDeterrent - grandEntranceBonus;

        // Cheese
        float cheeseThresh =
            ((config.caveThreshold + 0.13f) / config.caveSize) -
            (depthFactor * 0.12f) + combined;
        if (cheeseMap[fnIndex] > cheeseThresh) {
          if (wy < config.lavaLevel) {
            chunk.blocks[lx][ly][lz].block = lavaBlock;
            chunk.blocks[lx][ly][lz].metadata = 0;
          } else {
            chunk.blocks[lx][ly][lz].block = airBlock;
            chunk.blocks[lx][ly][lz].metadata = 0;
          }
          continue;
        }

        // Spaghetti
        float spagThresh = ((0.6f - config.caveThreshold) * config.caveSize) +
                           (depthFactor * 0.08f) - combined;

        float s1 = spag1Map[fnIndex];
        if (std::abs(s1) < spagThresh) {
          float s2 = spag2Map[fnIndex];
          if (std::abs(s2) < spagThresh) {
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
