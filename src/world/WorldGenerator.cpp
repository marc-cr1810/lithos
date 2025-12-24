#include "WorldGenerator.h"
#include "Block.h"
#include "Chunk.h"
#include <glm/gtc/noise.hpp>

#include "OreDecorator.h"
#include "TreeDecorator.h"

WorldGenerator::WorldGenerator(int seed) : seed(seed) {
  decorators.push_back(new OreDecorator());
  decorators.push_back(new TreeDecorator());
}

WorldGenerator::~WorldGenerator() {
  for (auto d : decorators)
    delete d;
  decorators.clear();
}

int WorldGenerator::GetHeight(int x, int z) {
  // Noise & Heightmap
  // Fix: Use modulo to prevent huge float values which destroy precision
  // (Blocky Chunks) Keep offset within ~0-100,000 range.
  int seedX = (seed * 1337) % 65536;
  int seedZ = (seed * 9999) % 65536;

  float nx = (float)x + (float)seedX;
  float nz = (float)z + (float)seedZ;

  float n = glm::perlin(glm::vec2(nx, nz) * 0.02f);

  // Map -1..1 to height. Base 64, Variation +/- 12 -> range 52 to 76 blocks
  // Raised from 22 to allow deeper underground
  return (int)(64 + n * 12);
}

void WorldGenerator::GenerateChunk(Chunk &chunk) {
  glm::ivec3 pos = chunk.chunkPosition;

  // Seed Offsets for Beach Noise
  int beachOffX = (seed * 5432) % 65536;
  int beachOffZ = (seed * 1234) % 65536;

  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
      // Global Coordinates
      int gx = pos.x * CHUNK_SIZE + x;
      int gz = pos.z * CHUNK_SIZE + z;

      // Get Height from single source of truth
      int height = GetHeight(gx, gz);

      // Calculate Beach Noise for this column
      float beachNoise =
          glm::perlin(glm::vec3(((float)gx + beachOffX) * 0.05f, 0.0f,
                                ((float)gz + beachOffZ) * 0.05f));
      // Threshold for being a beach: Positive noise means potential beach
      bool isBeach = (beachNoise > 0.1f);

      // Limit height for beaches (e.g., only up to Y=64, varying with noise)
      int beachHeightLimit = 60 + (int)(beachNoise * 4.0f); // 60 to 64

      // Determine Beach Type locally
      // Noise > 0.4 is Gravel, otherwise Sand
      BlockType beachBlock = (beachNoise > 0.4f) ? GRAVEL : SAND;

      for (int y = 0; y < CHUNK_SIZE; ++y) {
        int gy = pos.y * CHUNK_SIZE + y;

        BlockType type = AIR;

        if (gy <= height) {
          if (gy == height) {
            // Surface Block
            if (gy < 60) {
              // Underwater
              // Sandy/Gravel patches underwater
              if (beachNoise > 0.0f)
                type = beachBlock;
              else
                type = DIRT; // Muddy deeps
            } else {
              // Above water
              if (gy <= beachHeightLimit)
                type = beachBlock; // Natural Beach
              else
                type = GRASS;
            }
          } else if (gy > height - 4) {
            // Subsurface (Top 3 layers below surface)
            // If surface is Sand/Gravel, subsurface follows suit
            bool surfaceIsBeach = false;
            if (gy < 60) {
              if (beachNoise > 0.0f)
                surfaceIsBeach = true;
            } else {
              if (height <= beachHeightLimit)
                surfaceIsBeach = true;
            }

            if (surfaceIsBeach) {
              // sandstone? for now just same material
              type = beachBlock;
            } else
              type = DIRT;
          } else
            type = STONE;
        }

        // Bedrock at absolute 0
        if (gy == 0)
          type = STONE;

        // 3D Noise Caves
        // Parameters: Scale 0.06 (Broader), Threshold 0.25 (More frequent)
        // Removed (gy < height - 4) restriction to expose caves
        // Water Level (Sea Level @ Y=60)
        // If AIR and below sea level, fill with WATER
        // This must happen BEFORE Cave Generation to allow caves to stay dry
        // (by carving OUT the solids, but not water) Actually, if we want dry
        // caves, we carve solids into AIR. If we want caves NOT to be flooded,
        // we must Ensure Water only fills "Open Open Sky/Ocean" air. Wait. Old
        // logic: Terrain -> Caves(Air) -> WaterFill(Air->Water). Result:
        // Flooded Caves. New logic: Terrain -> WaterFill(Ocean Air) ->
        // Caves(Carve Solid). Result: Dry Caves.

        // 1. Fill Ocean Water if still AIR (Natural Terrain Air)
        if (type == AIR && gy <= 60) {
          type = WATER; // Temp local type

          // Note: We don't setBlock yet, we are building 'type'.
          // But we need to handle the "Grass Under Water" fix.
          // If we set type=WATER here, code below is fine.
        }

        // 2. Carve Caves
        // Parameters: Scale 0.06 (Broader), Threshold 0.25 (More frequent)

        // Crust Protection: Don't carve the very top layers of the terrain IF
        // IT IS UNDERWATER. This preserves the seabed integrity but allows
        // surface caves on land.
        bool isUnderwater = (height <= 60);
        bool preserveCrust = false;
        if (isUnderwater && gy > height - 3)
          preserveCrust = true;

        if (preserveCrust) {
          // Keep solid crust to prevent ocean draining
        } else if (gy > 1) {
          // Seabed Protection: Redundant check but safe
          // ...
          if (gy == height && height < 60) {
            // Do nothing
          } else {
            int caveOffX = (seed * 6789) % 65536;
            int caveOffY = (seed * 4321) % 65536;
            int caveOffZ = (seed * 1111) % 65536;

            float cx = (float)gx + (float)caveOffX;
            float cy = (float)gy + (float)caveOffY;
            float cz = (float)gz + (float)caveOffZ;
            float caveNoise = glm::perlin(glm::vec3(cx, cy, cz) * 0.06f);
            if (caveNoise > 0.25f) {
              if (type != WATER) {
                // Side Wall Protection: Check adjacent blocks for water
                // (intra-chunk)
                bool nearWater = false;
                if (x > 0 && chunk.getBlock(x - 1, y, z).getType() == WATER)
                  nearWater = true;
                if (x < CHUNK_SIZE - 1 &&
                    chunk.getBlock(x + 1, y, z).getType() == WATER)
                  nearWater = true;
                if (z > 0 && chunk.getBlock(x, y, z - 1).getType() == WATER)
                  nearWater = true;
                if (z < CHUNK_SIZE - 1 &&
                    chunk.getBlock(x, y, z + 1).getType() == WATER)
                  nearWater = true;
                // Also check Up/Down (though Up is handled by Crust/Seabed)
                if (y < CHUNK_SIZE - 1 &&
                    chunk.getBlock(x, y + 1, z).getType() == WATER)
                  nearWater = true;

                if (!nearWater) {
                  // Lava Lake Level
                  if (gy <= 10)
                    type = LAVA;
                  else
                    type = AIR;
                }
              }
            }
          }
        }

        chunk.setBlock(x, y, z, type);

        // Post-Set Fixes (Grass->Dirt)
        // Since we delayed setBlock, we can check logic
        if (type == WATER && gy <= 60) {
          // Grass under current block
          if (y > 0) {
            if (chunk.getBlock(x, y - 1, z).getType() == GRASS) {
              chunk.setBlock(x, y - 1, z, DIRT);
            }
          }
        }
      }
    }
  }

  // Apply Decorators
  for (auto d : decorators) {
    d->Decorate(chunk, *this);
  }
}
