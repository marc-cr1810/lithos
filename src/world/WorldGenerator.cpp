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
                         type = GRASS;
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
                if(gy > 1) {
                     float caveNoise = glm::perlin(glm::vec3((float)gx, (float)gy, (float)gz) * 0.06f);
                     if(caveNoise > 0.25f) {
                         type = AIR;
                     }
                }



                chunk.setBlock(x, y, z, type);
                
                // Water Level (Sea Level @ Y=10)
                // If AIR and below sea level, fill with WATER
                if(type == AIR && gy <= 18) {
                     chunk.setBlock(x, y, z, WATER);
                }
            }
        }
    }
    
    // Apply Decorators
    for(auto d : decorators) {
        d->Decorate(chunk, *this);
    }
}
