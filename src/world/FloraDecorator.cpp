#include "FloraDecorator.h"
#include "Block.h"
#include "WorldGenerator.h"
#include <cstdlib>
#include <glm/glm.hpp>

#include "ChunkColumn.h"

void FloraDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                              const ChunkColumn &column) {
  glm::ivec3 pos = chunk.chunkPosition;

  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
      int gx = pos.x * CHUNK_SIZE + x;
      int gz = pos.z * CHUNK_SIZE + z;

      int height = column.getHeight(x, z);
      int localY = height - pos.y * CHUNK_SIZE;

      if (localY >= 0 && localY < CHUNK_SIZE - 1) { // Ensure space above
        Biome biome = column.getBiome(x, z);
        if (height < 60)
          continue; // Not underwater

        ChunkBlock surface = chunk.getBlock(x, localY, z);
        ChunkBlock above = chunk.getBlock(x, localY + 1, z);

        if (above.getType() != AIR)
          continue;

        if (biome == BIOME_DESERT) {
          if (surface.getType() == SAND) {
            int r = rand() % 100;
            if (r < 2) { // 2% Dead Bush
              chunk.setBlock(x, localY + 1, z, DEAD_BUSH);
            } else if (r < 10) { // 8% Dry Short Grass
              chunk.setBlock(x, localY + 1, z, DRY_SHORT_GRASS);
            } else if (r < 12) { // 2% Dry Tall Grass
              chunk.setBlock(x, localY + 1, z, DRY_TALL_GRASS);
            }
          }
        } else if (biome == BIOME_PLAINS) {
          if (surface.getType() == GRASS) {
            int r = rand() % 100;
            if (r < 10) { // 10% Grass
              chunk.setBlock(x, localY + 1, z, TALL_GRASS);
            } else if (r < 12) { // 2% Rose
              chunk.setBlock(x, localY + 1, z, ROSE);
            }
          }
        } else if (biome == BIOME_FOREST) {
          if (surface.getType() == GRASS) {
            if ((rand() % 100) < 5) { // 5% Grass (less than plains)
              chunk.setBlock(x, localY + 1, z, TALL_GRASS);
            }
          }
        }
      }
    }
  }
}
