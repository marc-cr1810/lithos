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
                    // Water Level Check: Don't grow trees in water (Y < 60)
                    if(height < 60) continue;


                        // Check if block below is Grass
                        // Actually, we are running AFTER terrain generation, so the block at 'localY' should be GRASS.
                        ChunkBlock b = chunk.getBlock(x, localY, z);
                        if(b.getType() == GRASS)
                    {
                         if((rand() % 100) < 2) { // 2% chance
                             // Variable Height: 4 to 7
                             int treeHeight = 4 + (rand() % 4);
                             int y = localY;
                             
                             // Convert Grass under tree to Dirt
                             chunk.setBlock(x, y, z, DIRT);
                             
                             // Trunk
                             for(int h=1; h<=treeHeight; ++h) chunk.setBlock(x, y+h, z, WOOD);
                             
                             // Leaves based on height
                             // 2 layers of big leaves, 2 layers of small leaves?
                             int leavesStart = y + treeHeight - 2;
                             int leavesEnd = y + treeHeight; // top of trunk
                             
                             // Big canopy layer
                             for(int ly=leavesStart; ly<=leavesEnd; ++ly) {
                                 int radius = 2;
                                 // Taper top
                                 if(ly == leavesEnd) radius = 1; 

                                 for(int lx=x-radius; lx<=x+radius; ++lx) {
                                     for(int lz=z-radius; lz<=z+radius; ++lz) {
                                         // Corner check for roundness
                                         if(abs(lx-x) == radius && abs(lz-z) == radius && (rand()%2)==0) continue;
                                         
                                         if(chunk.getBlock(lx, ly, lz).getType() == AIR)
                                             chunk.setBlock(lx, ly, lz, LEAVES);
                                     }
                                 }
                             }
                             
                             // Very top crown
                             chunk.setBlock(x, leavesEnd+1, z, LEAVES);
                             chunk.setBlock(x+1, leavesEnd+1, z, LEAVES);
                             chunk.setBlock(x-1, leavesEnd+1, z, LEAVES);
                             chunk.setBlock(x, leavesEnd+1, z+1, LEAVES);
                             chunk.setBlock(x, leavesEnd+1, z-1, LEAVES);
                             // Tip
                             chunk.setBlock(x, leavesEnd+2, z, LEAVES);
                         }
                    }
                }
            }
        }
    }
}
