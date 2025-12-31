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
      int decorY = height + 1;
      int localY = decorY - pos.y * CHUNK_SIZE;

      // Only decorate if the decoration block itself is in this chunk
      if (localY >= 0 && localY < CHUNK_SIZE) {
        if (height < generator.GetConfig().seaLevel)
          continue; // Above water only

        Biome biome = column.getBiome(x, z);
        BlockType surface = generator.GetSurfaceBlock(gx, height, gz, true);

        if (biome == BIOME_DESERT) {
          if (surface == SAND) {
            int r = rand() % 100;
            float density = generator.GetConfig().floraDensity;
            if (r < density * 0.2f) { // 2% Dead Bush (relative)
              chunk.setBlock(x, localY, z, DEAD_BUSH);
            } else if (r < density) { // 8% Dry Short Grass
              chunk.setBlock(x, localY, z, DRY_SHORT_GRASS);
            } else if (r < density * 1.2f) { // 2% Dry Tall Grass
              chunk.setBlock(x, localY, z, DRY_TALL_GRASS);
            }
          }
        } else if (biome == BIOME_PLAINS) {
          if (surface == GRASS) {
            int r = rand() % 100;
            float density = generator.GetConfig().floraDensity;
            if (r < density) { // 10% Grass
              chunk.setBlock(x, localY, z, TALL_GRASS);
            } else if (r < density + 2) { // 2% Rose
              chunk.setBlock(x, localY, z, ROSE);
            }
          }
        } else if (biome == BIOME_FOREST) {
          if (surface == GRASS) {
            if ((rand() % 100) < (generator.GetConfig().floraDensity *
                                  0.5f)) { // 5% Grass (less than plains)
              chunk.setBlock(x, localY, z, TALL_GRASS);
            }
          }
        }
      }
    }
  }
}
