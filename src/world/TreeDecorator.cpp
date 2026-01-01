#include "TreeDecorator.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "Block.h"
#include "ChunkColumn.h"
#include "WorldGenerator.h"
#include <cstdlib>
#include <glm/glm.hpp>
#include <vector>

// Helper for deterministic random based on position and seed
static int GetPosRand(int x, int z, int seed, int salt) {
  unsigned int h = (unsigned int)x * 73856093 ^ (unsigned int)z * 19349663 ^
                   (unsigned int)seed * 83492791 ^ (unsigned int)salt;
  return (int)(h % 100);
}

// Tree generation helpers that check chunk bounds
static void GenerateOak(Chunk &chunk, int gx, int gy, int gz, int seed) {
  int treeHeight = 4 + (GetPosRand(gx, gz, seed, 1) % 4);
  glm::ivec3 cp = chunk.chunkPosition * CHUNK_SIZE;

  // Trunk
  for (int h = 1; h <= treeHeight; ++h) {
    int ly = (gy + h) - cp.y;
    int lx = gx - cp.x;
    int lz = gz - cp.z;
    if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE && lz >= 0 &&
        lz < CHUNK_SIZE) {
      if (chunk.getBlock(lx, ly, lz).getType() == AIR ||
          chunk.getBlock(lx, ly, lz).getType() == LEAVES)
        chunk.setBlock(lx, ly, lz, WOOD);
    }
  }

  // Leaves
  int leavesStart = gy + treeHeight - 2;
  int leavesEnd = gy + treeHeight;

  for (int ly = leavesStart; ly <= leavesEnd; ++ly) {
    int radius = 2;
    if (ly == leavesEnd)
      radius = 1;

    for (int lx_g = gx - radius; lx_g <= gx + radius; ++lx_g) {
      for (int lz_g = gz - radius; lz_g <= gz + radius; ++lz_g) {
        if (abs(lx_g - gx) == radius && abs(lz_g - gz) == radius &&
            (GetPosRand(lx_g, lz_g, ly, seed) % 2 == 0))
          continue;

        int lx = lx_g - cp.x;
        int lz = lz_g - cp.z;
        int ly_local = ly - cp.y;

        if (lx >= 0 && lx < CHUNK_SIZE && ly_local >= 0 &&
            ly_local < CHUNK_SIZE && lz >= 0 && lz < CHUNK_SIZE) {
          if (chunk.getBlock(lx, ly_local, lz).getType() == AIR)
            chunk.setBlock(lx, ly_local, lz, LEAVES);
        }
      }
    }
  }
}

static void GeneratePine(Chunk &chunk, int gx, int gy, int gz, int seed) {
  int height = 6 + (GetPosRand(gx, gz, seed, 2) % 4); // 6-9
  glm::ivec3 cp = chunk.chunkPosition * CHUNK_SIZE;

  // Trunk
  for (int h = 1; h <= height; ++h) {
    int ly = (gy + h) - cp.y;
    int lx = gx - cp.x;
    int lz = gz - cp.z;
    if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE && lz >= 0 &&
        lz < CHUNK_SIZE) {
      chunk.setBlock(lx, ly, lz, PINE_WOOD);
    }
  }

  // Cone Leaves
  int startLeaves = gy + 2;
  for (int ly_g = startLeaves; ly_g <= gy + height + 1; ++ly_g) {
    int distFromTop = (gy + height + 1) - ly_g;
    int radius = 0;

    if (distFromTop == 0)
      radius = 0;
    else if (distFromTop < 3)
      radius = 1;
    else if (distFromTop < 5)
      radius = 2;
    else
      radius = 2; // Keep thin

    for (int lx_g = gx - radius; lx_g <= gx + radius; ++lx_g) {
      for (int lz_g = gz - radius; lz_g <= gz + radius; ++lz_g) {
        if (radius > 0 && abs(lx_g - gx) == radius &&
            abs(lz_g - gz) == radius) {
          if ((GetPosRand(lx_g, lz_g, ly_g, seed) % 2) != 0)
            continue;
        }

        int lx = lx_g - cp.x;
        int lz = lz_g - cp.z;
        int ly_local = ly_g - cp.y;

        if (lx >= 0 && lx < CHUNK_SIZE && ly_local >= 0 &&
            ly_local < CHUNK_SIZE && lz >= 0 && lz < CHUNK_SIZE) {
          if (chunk.getBlock(lx, ly_local, lz).getType() == AIR)
            chunk.setBlock(lx, ly_local, lz, PINE_LEAVES);
        }
      }
    }
  }
}

static void GenerateCactus(Chunk &chunk, int gx, int gy, int gz, int seed) {
  int height = 2 + (GetPosRand(gx, gz, seed, 3) % 3); // 2-4
  glm::ivec3 cp = chunk.chunkPosition * CHUNK_SIZE;
  for (int h = 1; h <= height; ++h) {
    int ly = (gy + h) - cp.y;
    int lx = gx - cp.x;
    int lz = gz - cp.z;
    if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE && lz >= 0 &&
        lz < CHUNK_SIZE) {
      chunk.setBlock(lx, ly, lz, CACTUS);
    }
  }
}

void TreeDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                             const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("Decorator_Trees", generator.IsProfilingEnabled());
  glm::ivec3 cp = chunk.chunkPosition;
  int seed = generator.GetSeed();

  int startX = cp.x * CHUNK_SIZE;
  int startZ = cp.z * CHUNK_SIZE;

  // Iterate over column
  for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
    for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
      int gx = startX + lx;
      int gz = startZ + lz;

      int height = column.getHeight(lx, lz);

      // Skip if height is not in this chunk (optional optimization, but we need
      // to place blocks relative to chunk) Actually, we should check if the
      // SURFACE is within or near this chunk. If the surface is Y=70 and this
      // chunk is Y=0..32, we do nothing. If chunk is Y=64..96, we decorate.

      int chunkYStart = cp.y * CHUNK_SIZE;
      int chunkYEnd = (cp.y + 1) * CHUNK_SIZE;

      // Tree usually starts at surface + 1
      // Tree usually starts at surface + 1
      if (height < chunkYStart - 10 || height > chunkYEnd + 10)
        continue;

      // New: Check against Sea Level to prevent underwater forests
      if (height < generator.GetConfig().seaLevel)
        continue;

      // Check surface block type (Must be soil)
      int ly_surf = height - cp.y * CHUNK_SIZE;
      if (ly_surf >= 0 && ly_surf < CHUNK_SIZE) {
        BlockType surfaceBlock =
            static_cast<BlockType>(chunk.getBlock(lx, ly_surf, lz).getType());
        if (surfaceBlock == WATER || surfaceBlock == ICE ||
            surfaceBlock == AIR) {
          continue;
        }
      }

      // Get Noise Data
      float temp = column.temperatureMap[lx][lz];
      float humid = column.humidityMap[lx][lz];
      float forest = column.forestNoiseMap[lx][lz];

      // --- Decision Logic ---

      // 1. Cactus (Desert: Hot & Dry)
      // High Temp (> 30C), Low Humidity
      if (temp > 30.0f && humid < -0.5f) {
        if (GetPosRand(gx, gz, seed, 300) <
            generator.GetConfig().cactusDensity) {
          GenerateCactus(chunk, gx, height, gz, seed);
        }
      }

      // 2. Trees (Need Forest Noise + suitable Temp/Humid)
      if (forest > 0.2f) {

        // Pine (Cold < 5C)
        if (temp < 5.0f) {
          if (GetPosRand(gx, gz, seed, 400) <
              generator.GetConfig().pineDensity) {
            GeneratePine(chunk, gx, height, gz, seed);
          }
        }
        // Oak (Moderate: 5C to 35C)
        else if (temp >= 5.0f && temp < 35.0f && humid > -0.3f) {
          if (GetPosRand(gx, gz, seed, 500) <
              generator.GetConfig().oakDensity) {
            GenerateOak(chunk, gx, height, gz, seed);
          }
        }
      }
    }
  }
}
