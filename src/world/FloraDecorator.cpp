#include "FloraDecorator.h"
#include "Block.h"
#include "WorldGenerator.h"
#include <cstdlib>
#include <glm/glm.hpp>

void FloraDecorator::Decorate(Chunk &chunk, WorldGenerator &generator) {
  glm::ivec3 pos = chunk.chunkPosition;

  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
      int gx = pos.x * CHUNK_SIZE + x;
      int gz = pos.z * CHUNK_SIZE + z;

      int height = generator.GetHeight(gx, gz);
      int localY = height - pos.y * CHUNK_SIZE;

      if (localY >= 0 && localY < CHUNK_SIZE - 1) { // Ensure space above
        Biome biome = generator.GetBiome(gx, gz);
        if (height < 60)
          continue; // Not underwater

        ChunkBlock surface = chunk.getBlock(x, localY, z);
        ChunkBlock above = chunk.getBlock(x, localY + 1, z);

        if (above.getType() != AIR)
          continue;

        if (biome == BIOME_DESERT) {
          if (surface.getType() == SAND) {
            if ((rand() % 100) < 2) { // 2% Dead Bush
              chunk.setBlock(x, localY + 1, z, DEAD_BUSH);
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
