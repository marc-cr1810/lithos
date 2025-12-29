#include "TreeDecorator.h"
#include "Block.h"
#include "WorldGenerator.h"
#include <cstdlib>
#include <glm/glm.hpp>

// Helper functions (could be made private methods if header modified, but this
// keeps it localized)
static void GenerateOak(Chunk &chunk, int x, int y, int z) {
  int treeHeight = 4 + (rand() % 4);

  // Trunk
  for (int h = 1; h <= treeHeight; ++h) {
    chunk.setBlock(x, y + h, z, WOOD);
  }

  // Leaves
  int leavesStart = y + treeHeight - 2;
  int leavesEnd = y + treeHeight;

  for (int ly = leavesStart; ly <= leavesEnd; ++ly) {
    int radius = 2;
    if (ly == leavesEnd)
      radius = 1;

    for (int lx = x - radius; lx <= x + radius; ++lx) {
      for (int lz = z - radius; lz <= z + radius; ++lz) {
        if (abs(lx - x) == radius && abs(lz - z) == radius && (rand() % 2 == 0))
          continue;
        if (chunk.getBlock(lx, ly, lz).getType() == AIR)
          chunk.setBlock(lx, ly, lz, LEAVES);
      }
    }
  }

  // Top Cross
  chunk.setBlock(x, leavesEnd + 1, z, LEAVES);
  chunk.setBlock(x + 1, leavesEnd + 1, z, LEAVES);
  chunk.setBlock(x - 1, leavesEnd + 1, z, LEAVES);
  chunk.setBlock(x, leavesEnd + 1, z + 1, LEAVES);
  chunk.setBlock(x, leavesEnd + 1, z - 1, LEAVES);
}

static void GeneratePine(Chunk &chunk, int x, int y, int z) {
  int height = 6 + (rand() % 4); // 6-9

  // Trunk
  for (int h = 1; h <= height; ++h) {
    chunk.setBlock(x, y + h, z, PINE_WOOD);
  }

  // Cone Leaves
  int startLeaves = y + 2;
  for (int ly = startLeaves; ly <= y + height + 1; ++ly) {
    // Radius tapers up
    // height top = 0
    // height top-1 = 1
    // height top-2 = 1
    // height top-3 = 2
    // ...
    int distFromTop = (y + height + 1) - ly;
    int radius = 0;

    if (distFromTop == 0)
      radius = 0;
    else if (distFromTop < 3)
      radius = 1;
    else if (distFromTop < 5)
      radius = 2;
    else
      radius = 2; // Keep thin

    for (int lx = x - radius; lx <= x + radius; ++lx) {
      for (int lz = z - radius; lz <= z + radius; ++lz) {
        // Roundish
        if (radius > 0 && abs(lx - x) == radius && abs(lz - z) == radius) {
          if ((rand() % 2) != 0)
            continue; // Skip corners sometimes
        }
        if (chunk.getBlock(lx, ly, lz).getType() == AIR)
          chunk.setBlock(lx, ly, lz, PINE_LEAVES);
      }
    }
  }
}

static void GenerateCactus(Chunk &chunk, int x, int y, int z) {
  int height = 2 + (rand() % 3); // 2-4
  for (int h = 1; h <= height; ++h) {
    chunk.setBlock(x, y + h, z, CACTUS);
  }
}

#include "ChunkColumn.h"

void TreeDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                             const ChunkColumn &column) {
  glm::ivec3 pos = chunk.chunkPosition;

  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
      int gx = pos.x * CHUNK_SIZE + x;
      int gz = pos.z * CHUNK_SIZE + z;

      int height = column.getHeight(x, z);
      int localY = height - pos.y * CHUNK_SIZE;

      if (localY >= 0 && localY < CHUNK_SIZE) {
        // Padding check
        if (x < 2 || x > CHUNK_SIZE - 3 || z < 2 || z > CHUNK_SIZE - 3 ||
            localY > CHUNK_SIZE - 10)
          continue;

        Biome biome = column.getBiome(x, z);
        if (height < 60)
          continue;

        ChunkBlock surface = chunk.getBlock(x, localY, z);

        // Decisions
        if (biome == BIOME_DESERT) {
          // Cactus
          if (surface.getType() == SAND) {
            if ((rand() % 100) < 1) { // 1%
              GenerateCactus(chunk, x, localY, z);
            }
          }
        } else if (biome == BIOME_TUNDRA) {
          // Pine
          if (surface.getType() == SNOW || surface.getType() == GRASS ||
              surface.getType() == DIRT) {
            if ((rand() % 100) < 2) { // 2%
              // Fix ground to dirt?
              chunk.setBlock(x, localY, z, DIRT);
              GeneratePine(chunk, x, localY, z);
            }
          }
        } else if (biome == BIOME_FOREST) {
          // Dense Oak/Pine mix? Just Oak for now.
          if (surface.getType() == GRASS) {
            if ((rand() % 100) < 5) { // 5%
              chunk.setBlock(x, localY, z, DIRT);
              GenerateOak(chunk, x, localY, z);
            }
          }
        } else if (biome == BIOME_PLAINS) {
          // Sparse Oak
          if (surface.getType() == GRASS) {
            if ((rand() % 100) < 1) { // 1%
              chunk.setBlock(x, localY, z, DIRT);
              GenerateOak(chunk, x, localY, z);
            }
          }
        }
      }
    }
  }
}
