#include "FloraDecorator.h"
#include "../debug/Profiler.h"
#include "Block.h"
#include "ChunkColumn.h"
#include "World.h"
#include "WorldGenerator.h"
#include <cstdlib>
#include <glm/glm.hpp>

void FloraDecorator::Decorate(Chunk &chunk, WorldGenerator &generator,
                              const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("Decorator_Flora", generator.IsProfilingEnabled());
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

        // Use Column Data for decisions
        float temp = column.temperatureMap[x][z];
        float humid = column.humidityMap[x][z];
        // float beach = column.beachNoiseMap[x][z]; // Could use to avoid flora
        // on beaches
        float bushNoise = column.bushNoiseMap[x][z];

        // Verify the block where flora will sit (decorY - 1)
        BlockType groundBlock = AIR;
        if (chunk.getWorld()) {
          groundBlock = (BlockType)chunk.getWorld()
                            ->getBlock(gx, decorY - 1, gz)
                            .getType();
        } else {
          // Fallback to checking within chunk if available
          int groundLocalY = (decorY - 1) - pos.y * CHUNK_SIZE;
          if (groundLocalY >= 0 && groundLocalY < CHUNK_SIZE) {
            groundBlock =
                (BlockType)chunk.getBlock(x, groundLocalY, z).getType();
          } else {
            groundBlock =
                generator.GetSurfaceBlock(gx, decorY - 1, gz, &column);
          }
        }

        // Skip if ground is not solid
        if (groundBlock == AIR || groundBlock == WATER || groundBlock == LAVA)
          continue;

        // --- Desert Flora ---
        if (temp > 30.0f && humid < -0.5f) {
          if (groundBlock == SAND) {
            int r = rand() % 100;
            float density = generator.GetConfig().floraDensity;

            // Use Bush Noise (bushScale) to cluster dead bushes?
            if (bushNoise > 0.3f && r < density) {
              chunk.setBlock(x, localY, z, DEAD_BUSH);
            } else if (r < density * 0.5f) {
              chunk.setBlock(x, localY, z, DRY_SHORT_GRASS);
            }
          }
        }
        // --- Moderate/Lush Flora ---
        else if (temp > 5.0f && humid > -0.3f) {
          if (groundBlock == GRASS) {
            int r = rand() % 100;
            float density = generator.GetConfig().floraDensity;

            // Tall Grass & Roses
            // Boost density if "bush/forest" noise is high
            if (column.forestNoiseMap[x][z] > 0.0f) {
              density *= 1.5f;
            }

            if (r < density) {
              chunk.setBlock(x, localY, z, TALL_GRASS);
            } else if (r < density + 2) {
              chunk.setBlock(x, localY, z, ROSE);
            }
          }
        }
        // --- Cold Flora ---
        else if (temp < -0.2f) {
          // Tundra grass?
          if (groundBlock == GRASS ||
              groundBlock == DIRT) { // Snow usually covers specific blocks
            // Less flora in cold?
            if ((rand() % 100) < 5) {
              chunk.setBlock(x, localY, z, TALL_GRASS);
            }
          }
        }
      }
    }
  }
}
