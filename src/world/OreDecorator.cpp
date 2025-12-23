#include "OreDecorator.h"
#include "WorldGenerator.h"
#include "Block.h"
#include <cstdlib>

void OreDecorator::Decorate(Chunk& chunk, WorldGenerator& generator)
{
    // Ores spawn underground (Stone)
    // Iterate random attempts per chunk
    
    // Coal: Common, any height below surface
    for(int i=0; i<10; ++i) {
        int x = rand() % CHUNK_SIZE;
        int y = rand() % CHUNK_SIZE;
        int z = rand() % CHUNK_SIZE;
        
        ChunkBlock b = chunk.getBlock(x, y, z);
        if(b.getType() == STONE) {
            GenerateOre(chunk, x, y, z, COAL_ORE);
        }
    }

    // Iron: Rare, lower depths
    // Need global Y to check depth?
    // chunk.chunkPosition.y * CHUNK_SIZE + y;
    int globalYBase = chunk.chunkPosition.y * CHUNK_SIZE;
    
    for(int i=0; i<5; ++i) {
        int x = rand() % CHUNK_SIZE;
        int y = rand() % CHUNK_SIZE;
        int z = rand() % CHUNK_SIZE;
        
        int gy = globalYBase + y;
        
        // Only deep? Say below 40.
        // Surface is around 22-34.
        // So actually everything is "deep" if chunk Y is negative or small.
        // Let's just spawn it in Stone for now.
        
        ChunkBlock b = chunk.getBlock(x, y, z);
        if(b.getType() == STONE) {
            GenerateOre(chunk, x, y, z, IRON_ORE);
        }
    }
}

void OreDecorator::GenerateOre(Chunk& chunk, int startX, int startY, int startZ, BlockType oreType)
{
    // Small cluster of 2-4 blocks
    chunk.setBlock(startX, startY, startZ, oreType);
    
    // Try neighbors
    for(int i=0; i<3; ++i) {
        int dx = (rand() % 3) - 1;
        int dy = (rand() % 3) - 1;
        int dz = (rand() % 3) - 1;
        
        int nx = startX + dx;
        int ny = startY + dy;
        int nz = startZ + dz;
        
        if(nx>=0 && nx<CHUNK_SIZE && ny>=0 && ny<CHUNK_SIZE && nz>=0 && nz<CHUNK_SIZE) {
             if(chunk.getBlock(nx, ny, nz).getType() == STONE) {
                 chunk.setBlock(nx, ny, nz, oreType);
             }
        }
    }
}
