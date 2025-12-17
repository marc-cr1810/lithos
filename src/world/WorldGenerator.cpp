#include "WorldGenerator.h"
#include "Chunk.h"
#include "Block.h"
#include <glm/gtc/noise.hpp>

WorldGenerator::WorldGenerator() {}

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
                         // Tree Generation
                         // Simple random placement, only if inside chunk bounds (padding for leaves)
                         // Padding 2 for leaves.
                         if(x > 1 && x < 14 && z > 1 && z < 14 && gy < CHUNK_SIZE - 6) {
                             if((rand() % 100) < 2) {
                                 // Trunk
                                 for(int h=1; h<=4; ++h) chunk.setBlock(x, y+h, z, WOOD);
                                 // Leaves
                                 for(int lx=x-2; lx<=x+2; ++lx) {
                                     for(int lz=z-2; lz<=z+2; ++lz) {
                                         for(int ly=y+3; ly<=y+4; ++ly) { // 2 layers of leaves
                                             if(chunk.getBlock(lx, ly, lz).type == AIR)
                                                 chunk.setBlock(lx, ly, lz, LEAVES);
                                         }
                                     }
                                 }
                                 // Top leaves
                                 chunk.setBlock(x, y+5, z, LEAVES);
                                 chunk.setBlock(x+1, y+5, z, LEAVES);
                                 chunk.setBlock(x-1, y+5, z, LEAVES);
                                 chunk.setBlock(x, y+5, z+1, LEAVES);
                                 chunk.setBlock(x, y+5, z-1, LEAVES);
                             }
                         }
                    }
                    else if(gy > height - 3) type = DIRT;
                    else type = STONE;
                }
                
                // Bedrock at absolute 0
                if(gy == 0) type = STONE; 
                
                // 3D Noise Caves
                // Parameters: Scale 0.06 (Broader), Threshold 0.25 (More frequent)
                if(gy < height - 4 && gy > 1) {
                     float caveNoise = glm::perlin(glm::vec3((float)gx, (float)gy, (float)gz) * 0.06f);
                     if(caveNoise > 0.25f) {
                         type = AIR;
                     }
                }

                chunk.setBlock(x, y, z, type);
            }
        }
    }
}
