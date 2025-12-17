#include "TreeDecorator.h"
#include "WorldGenerator.h"
#include "Block.h"
#include <cstdlib>

void TreeDecorator::Decorate(Chunk& chunk, WorldGenerator& generator)
{
    glm::ivec3 pos = chunk.chunkPosition;

    for(int x=0; x<CHUNK_SIZE; ++x)
    {
        for(int z=0; z<CHUNK_SIZE; ++z)
        {
            int gx = pos.x * CHUNK_SIZE + x;
            int gz = pos.z * CHUNK_SIZE + z;
            
            // We need the height for placement
            int height = generator.GetHeight(gx, gz);
            int localY = height - pos.y * CHUNK_SIZE;

            // Check if surface is in this chunk
            if(localY >= 0 && localY < CHUNK_SIZE)
            {
                // Simple random placement with padding
                // Padding 2 for leaves.
                if(x > 1 && x < 14 && z > 1 && z < 14 && localY < CHUNK_SIZE - 6) 
                {
                    // Check if block below is Grass
                    // Actually, we are running AFTER terrain generation, so the block at 'localY' should be GRASS.
                    Block b = chunk.getBlock(x, localY, z);
                    if(b.type == GRASS)
                    {
                         if((rand() % 100) < 2) { // 2% chance
                             int y = localY;
                             
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
            }
        }
    }
}
