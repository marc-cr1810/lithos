#include "TreeDecorator.h"
#include "Block.h"
#include "ChunkColumn.h"
#include "WorldGenerator.h"
#include <cstdlib>
#include <glm/glm.hpp>

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

  // Top Cross
  int tx = gx, ty = leavesEnd + 1, tz = gz;
  int offsets[5][3] = {{0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
  for (int i = 0; i < 5; ++i) {
    int ox = tx + offsets[i][0] - cp.x;
    int oy = ty + offsets[i][1] - cp.y;
    int oz = tz + offsets[i][2] - cp.z;
    if (ox >= 0 && ox < CHUNK_SIZE && oy >= 0 && oy < CHUNK_SIZE && oz >= 0 &&
        oz < CHUNK_SIZE) {
      if (chunk.getBlock(ox, oy, oz).getType() == AIR)
        chunk.setBlock(ox, oy, oz, LEAVES);
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
  glm::ivec3 cp = chunk.chunkPosition;
  int seed = generator.GetSeed();

  // Search a radius of 5 around the chunk for potential tree starts
  int searchRadius = 5;
  int minGX = cp.x * CHUNK_SIZE - searchRadius;
  int maxGX = (cp.x + 1) * CHUNK_SIZE + searchRadius;
  int minGZ = cp.z * CHUNK_SIZE - searchRadius;
  int maxGZ = (cp.z + 1) * CHUNK_SIZE + searchRadius;

  for (int gx = minGX; gx < maxGX; ++gx) {
    for (int gz = minGZ; gz < maxGZ; ++gz) {
      // Deterministic check if tree starts at (gx, gz)
      int height = generator.GetHeight(gx, gz);
      if (height < 60)
        continue; // No trees in ocean/low beach

      Biome biome = generator.GetBiome(gx, gz);
      BlockType surface = generator.GetSurfaceBlock(gx, height, gz, true);

      // We use different salts for different biomes to avoid identical layouts
      int roll = GetPosRand(gx, gz, seed, 100);

      if (biome == BIOME_DESERT) {
        if (surface == SAND && roll < generator.GetConfig().cactusDensity) {
          GenerateCactus(chunk, gx, height, gz, seed);
        }
      } else if (biome == BIOME_TUNDRA) {
        if ((surface == SNOW || surface == GRASS || surface == DIRT) &&
            roll < generator.GetConfig().pineDensity) {
          GeneratePine(chunk, gx, height, gz, seed);
          // Note: Ground flattening (SNOW->DIRT) only handled if trunk in THIS
          // chunk
          int lx = gx - cp.x * CHUNK_SIZE;
          int ly = height - cp.y * CHUNK_SIZE;
          int lz = gz - cp.z * CHUNK_SIZE;
          if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE &&
              lz >= 0 && lz < CHUNK_SIZE)
            chunk.setBlock(lx, ly, lz, DIRT);
        }
      } else if (biome == BIOME_FOREST) {
        if (surface == GRASS && roll < generator.GetConfig().oakDensity) {
          GenerateOak(chunk, gx, height, gz, seed);
          int lx = gx - cp.x * CHUNK_SIZE;
          int ly = height - cp.y * CHUNK_SIZE;
          int lz = gz - cp.z * CHUNK_SIZE;
          if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE &&
              lz >= 0 && lz < CHUNK_SIZE)
            chunk.setBlock(lx, ly, lz, DIRT);
        }
      } else if (biome == BIOME_PLAINS) {
        if (surface == GRASS && roll < 1) {
          GenerateOak(chunk, gx, height, gz, seed);
          int lx = gx - cp.x * CHUNK_SIZE;
          int ly = height - cp.y * CHUNK_SIZE;
          int lz = gz - cp.z * CHUNK_SIZE;
          if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE &&
              lz >= 0 && lz < CHUNK_SIZE)
            chunk.setBlock(lx, ly, lz, DIRT);
        }
      }
    }
  }
}
