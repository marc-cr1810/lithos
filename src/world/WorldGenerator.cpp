#include "WorldGenerator.h"
#include "Chunk.h"
#include "Block.h"
#include <glm/gtc/noise.hpp>

#include "TreeDecorator.h"
#include "OreDecorator.h"

WorldGenerator::WorldGenerator() 
{
    decorators.push_back(new OreDecorator());
    decorators.push_back(new TreeDecorator());
}

WorldGenerator::~WorldGenerator()
{
    for(auto d : decorators) delete d;
    decorators.clear();
}

int WorldGenerator::GetHeight(int x, int z)
{
    // Noise & Heightmap
    // Scale factor 0.02 gives smoother, broader hills (less extreme)
    float n = glm::perlin(glm::vec2((float)x, (float)z) * 0.02f); 
    // Map -1..1 to height. Base 22, Variation +/- 12 -> range 10 to 34 blocks
    // This matches the previous logic exactly
    return (int)(22 + n * 12); 
}

void WorldGenerator::GenerateChunk(Chunk& chunk)
{
    glm::ivec3 pos = chunk.chunkPosition;
    
    for(int x=0; x<CHUNK_SIZE; ++x)
    {
        for(int z=0; z<CHUNK_SIZE; ++z)
        {
            // Global Coordinates
            int gx = pos.x * CHUNK_SIZE + x;
            int gz = pos.z * CHUNK_SIZE + z;
            
            // Get Height from single source of truth
            int height = GetHeight(gx, gz);
            
            for(int y=0; y<CHUNK_SIZE; ++y)
            {
                int gy = pos.y * CHUNK_SIZE + y;
                
                BlockType type = AIR;
                
                if(gy <= height) {
                    if(gy == height) {
                         // Surface Block
                         if(gy < 18) type = DIRT; // Underwater surface is Dirt (below level 18)
                         else type = GRASS;       // At level 18 or above is Grass
                         // Tree logic removed, used Decorator
                    }
                    else if(gy > height - 3) type = DIRT;
                    else type = STONE;
                }
                
                // Bedrock at absolute 0
                if(gy == 0) type = STONE; 
                
                // 3D Noise Caves
                // Parameters: Scale 0.06 (Broader), Threshold 0.25 (More frequent)
                // Removed (gy < height - 4) restriction to expose caves
                // Water Level (Sea Level @ Y=10)
                // If AIR and below sea level, fill with WATER
                // This must happen BEFORE Cave Generation to allow caves to stay dry (by carving OUT the solids, but not water)
                // Actually, if we want dry caves, we carve solids into AIR.
                // If we want caves NOT to be flooded, we must Ensure Water only fills "Open Open Sky/Ocean" air.
                // Wait.
                // Old logic: Terrain -> Caves(Air) -> WaterFill(Air->Water). Result: Flooded Caves.
                // New logic: Terrain -> WaterFill(Ocean Air) -> Caves(Carve Solid). Result: Dry Caves.
                
                // 1. Fill Ocean Water if still AIR (Natural Terrain Air)
                if(type == AIR && gy <= 18) {
                    type = WATER; // Temp local type
                    
                    // Note: We don't setBlock yet, we are building 'type'. 
                    // But we need to handle the "Grass Under Water" fix.
                    // If we set type=WATER here, code below is fine.
                }

                // 2. Carve Caves
                // Parameters: Scale 0.06 (Broader), Threshold 0.25 (More frequent)
                
                // Crust Protection: Don't carve the very top layers of the terrain IF IT IS UNDERWATER.
                // This preserves the seabed integrity but allows surface caves on land.
                bool isUnderwater = (height <= 18);
                bool preserveCrust = false;
                if(isUnderwater && gy > height - 3) preserveCrust = true;

                if(preserveCrust) {
                     // Keep solid crust to prevent ocean draining
                }
                else if(gy > 1) {
                     // Seabed Protection: Redundant check but safe
                     // ...
                     if(gy == height && height < 18) {
                         // Do nothing
                     }
                     else {
                         float caveNoise = glm::perlin(glm::vec3((float)gx, (float)gy, (float)gz) * 0.06f);
                         if(caveNoise > 0.25f) {
                             if(type != WATER) {
                                 // Side Wall Protection: Check adjacent blocks for water (intra-chunk)
                                 bool nearWater = false;
                                 if(x > 0 && chunk.getBlock(x-1, y, z).type == WATER) nearWater = true;
                                 if(x < CHUNK_SIZE-1 && chunk.getBlock(x+1, y, z).type == WATER) nearWater = true;
                                 if(z > 0 && chunk.getBlock(x, y, z-1).type == WATER) nearWater = true;
                                 if(z < CHUNK_SIZE-1 && chunk.getBlock(x, y, z+1).type == WATER) nearWater = true;
                                 // Also check Up/Down (though Up is handled by Crust/Seabed)
                                 if(y < CHUNK_SIZE-1 && chunk.getBlock(x, y+1, z).type == WATER) nearWater = true;

                                 if(!nearWater) {
                                     // Lava Lake Level
                                     if(gy <= 10) type = LAVA;
                                     else type = AIR;
                                 }
                             }
                         }
                     }
                }

                chunk.setBlock(x, y, z, type);
                
                // Post-Set Fixes (Grass->Dirt)
                // Since we delayed setBlock, we can check logic
                if(type == WATER && gy <= 18) {
                     // Grass under current block
                     if(y > 0) {
                         if(chunk.getBlock(x, y-1, z).type == GRASS) {
                             chunk.setBlock(x, y-1, z, DIRT);
                         }
                     }
                }
            }
        }
    }
    
    // Apply Decorators
    for(auto d : decorators) {
        d->Decorate(chunk, *this);
    }
}
