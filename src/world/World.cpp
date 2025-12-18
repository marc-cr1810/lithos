#include "World.h"
#include <cmath>
#include <limits>
#include <iostream>

World::World() {}

void World::addChunk(int x, int y, int z)
{
    auto key = std::make_tuple(x, y, z);
    if(chunks.find(key) == chunks.end())
    {
        auto newChunk = std::make_unique<Chunk>();
        newChunk->chunkPosition = glm::ivec3(x, y, z);
        newChunk->setWorld(this);
        chunks[key] = std::move(newChunk);
        Chunk* c = chunks[key].get();
        
        // Force immediate update so this chunk has valid valid lighting data
        // BEFORE neighbors try to read from it.
        c->updateMesh();
        
        // Mark neighbors as dirty so they can rebuild faces/lighting against this new chunk
        int dx[] = {-1, 1, 0, 0, 0, 0};
        int dy[] = {0, 0, -1, 1, 0, 0};
        int dz[] = {0, 0, 0, 0, -1, 1};
        
        for(int i=0; i<6; ++i) {
            Chunk* n = getChunk(x + dx[i], y + dy[i], z + dz[i]);
            if(n) n->meshDirty = true;
        }
    }
}

Chunk* World::getChunk(int chunkX, int chunkY, int chunkZ)
{
    auto key = std::make_tuple(chunkX, chunkY, chunkZ);
    auto it = chunks.find(key);
    if(it != chunks.end())
        return it->second.get();
    return nullptr;
}

const Chunk* World::getChunk(int chunkX, int chunkY, int chunkZ) const
{
    auto key = std::make_tuple(chunkX, chunkY, chunkZ);
    auto it = chunks.find(key);
    if(it != chunks.end())
        return it->second.get();
    return nullptr;
}

// Helper for floor division
int floorDiv(int a, int b) {
    return (a >= 0) ? (a / b) : ((a - b + 1) / b);
}

Block World::getBlock(int x, int y, int z) const
{
    int cx = floorDiv(x, CHUNK_SIZE);
    int cy = floorDiv(y, CHUNK_SIZE);
    int cz = floorDiv(z, CHUNK_SIZE);
    
    const Chunk* c = getChunk(cx, cy, cz);
    if(!c) return Block{AIR};
    
    int lx = x % CHUNK_SIZE;
    int ly = y % CHUNK_SIZE;
    int lz = z % CHUNK_SIZE;
    if(lx < 0) lx += CHUNK_SIZE;
    if(ly < 0) ly += CHUNK_SIZE;
    if(lz < 0) lz += CHUNK_SIZE;
    
    return c->getBlock(lx, ly, lz);
}

uint8_t World::getSkyLight(int x, int y, int z)
{
    int cx = floorDiv(x, CHUNK_SIZE);
    int cy = floorDiv(y, CHUNK_SIZE);
    int cz = floorDiv(z, CHUNK_SIZE);

    Chunk* c = getChunk(cx, cy, cz);
    if(!c) return 15; // Sunlight is bright outside
    
    int lx = x % CHUNK_SIZE;
    int ly = y % CHUNK_SIZE;
    int lz = z % CHUNK_SIZE;
    if(lx < 0) lx += CHUNK_SIZE;
    if(ly < 0) ly += CHUNK_SIZE;
    if(lz < 0) lz += CHUNK_SIZE;
    
    return c->getSkyLight(lx, ly, lz);
}

uint8_t World::getBlockLight(int x, int y, int z)
{
    int cx = floorDiv(x, CHUNK_SIZE);
    int cy = floorDiv(y, CHUNK_SIZE);
    int cz = floorDiv(z, CHUNK_SIZE);

    Chunk* c = getChunk(cx, cy, cz);
    if(!c) return 0; // Block light is dark outside
    
    int lx = x % CHUNK_SIZE;
    int ly = y % CHUNK_SIZE;
    int lz = z % CHUNK_SIZE;
    if(lx < 0) lx += CHUNK_SIZE;
    if(ly < 0) ly += CHUNK_SIZE;
    if(lz < 0) lz += CHUNK_SIZE;
    
    return c->getBlockLight(lx, ly, lz);
}

void World::setBlock(int x, int y, int z, BlockType type)
{
    int cx = floorDiv(x, CHUNK_SIZE);
    int cy = floorDiv(y, CHUNK_SIZE);
    int cz = floorDiv(z, CHUNK_SIZE);
    
    int lx = x - cx * CHUNK_SIZE;
    int ly = y - cy * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    
    Chunk* c = getChunk(cx, cy, cz);
    if(c) { 
        c->setBlock(lx, ly, lz, type);
        
        // Recalculate lighting for this chunk
        c->calculateSunlight();
        c->calculateBlockLight();
        c->spreadLight();

        c->updateMesh(); // Update self immediately

         // Update neighbors in waves (Manhattan distance) to ensure propagation
        // Center (0,0,0) is already done.
        // Wave 1: Face Neighbors (Dist 1) -> Pulls from Center
        // Wave 2: Edge Neighbors (Dist 2) -> Pulls from Faces
        // Wave 3: Corner Neighbors (Dist 3) -> Pulls from Edges
        
        // Update neighbors in waves (Manhattan distance) to ensure propagation
        // Optimize: Only rebuild mesh if light actually changed (meshDirty)
        // Mark specific neighbors as dirty if the block change is on the border (AO requirement)
        // This optimizes performance by only rebuilding neighbors that physically touch the changed block.
        if(lx == 0) { Chunk* n = getChunk(cx-1, cy, cz); if(n) n->meshDirty = true; }
        if(lx == CHUNK_SIZE-1) { Chunk* n = getChunk(cx+1, cy, cz); if(n) n->meshDirty = true; }
        if(ly == 0) { Chunk* n = getChunk(cx, cy-1, cz); if(n) n->meshDirty = true; }
        if(ly == CHUNK_SIZE-1) { Chunk* n = getChunk(cx, cy+1, cz); if(n) n->meshDirty = true; }
        if(lz == 0) { Chunk* n = getChunk(cx, cy, cz-1); if(n) n->meshDirty = true; }
        if(lz == CHUNK_SIZE-1) { Chunk* n = getChunk(cx, cy, cz+1); if(n) n->meshDirty = true; }

        std::vector<Chunk*> neighbors;
        neighbors.reserve(26);

        for(int d = 1; d <= 3; ++d) {
            for(int dx = -1; dx <= 1; ++dx) {
                for(int dy = -1; dy <= 1; ++dy) {
                    for(int dz = -1; dz <= 1; ++dz) {
                        if(std::abs(dx) + std::abs(dy) + std::abs(dz) == d) {
                            Chunk* n = getChunk(cx + dx, cy + dy, cz + dz);
                            if(n) {
                                n->spreadLight();
                                neighbors.push_back(n);
                            }
                        }
                    }
                }
            }
        }
        
        for(Chunk* n : neighbors) {
            if(n->meshDirty) {
                n->updateMesh();
                n->meshDirty = false; 
            }
        }

        // Propagate sunlight changes downwards (Vertical Column Update)
        // If we modified a block, it might open/close the sky for chunks/blocks below.
        for(int y = cy - 1; y >= 0; --y) {
            Chunk* lower = getChunk(cx, y, cz);
            if(lower) {
                lower->calculateSunlight();
                lower->calculateBlockLight();
                lower->spreadLight();
                lower->updateMesh();
                
                // Also update neighbors of this lower chunk, as light might have spread to them
                int nDx[] = {-1, 1, 0, 0};
                int nDz[] = {0, 0, -1, 1};
                for(int i=0; i<4; ++i) {
                    Chunk* n = getChunk(cx + nDx[i], y, cz + nDz[i]);
                    if(n) {
                        n->spreadLight();
                        n->updateMesh();
                    }
                }
            }
        }
    }
}

void World::render(Shader& shader)
{
    for(auto& pair : chunks)
    {
        pair.second->render(shader);
    }
}

bool World::raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, glm::ivec3& outputPos, glm::ivec3& outputPrePos)
{
    // Naive: check all chunks, find closest hit
    bool hitAny = false;
    float closestDist = maxDist + 1.0f;
    glm::ivec3 bestPos;
    glm::ivec3 bestPrePos;
    
    for(auto& pair : chunks)
    {
        Chunk* c = pair.second.get();
        glm::ivec3 hitPos, prePos;
        // Transform origin for chunk is handled inside Chunk::raycast now? No, I updated it to do the subtraction.
        // So we just pass global origin.
        
        if(c->raycast(origin, direction, maxDist, hitPos, prePos))
        {
            // Calculate distance to hitPos (global)
            // hitPos is block coord (int). Center? Corner?
            // Chunk::raycast returns the block coords (chunk-local + chunkPos*size).
            // Wait, Chunk::raycast returns *chunk local* coords in my previous edit?
            // "outputPos = glm::ivec3(x, y, z);" where x,y,z are loop vars 0..15.
            // YES. I need to convert them to global coords here or in Chunk::raycast.
            // Let's ensure Chunk::raycast returns global coords or we convert them.
            
            // Checking Chunk.cpp edit:
            // "glm::vec3 localOrigin = origin - glm::vec3(chunkPosition * CHUNK_SIZE);"
            // "outputPos = glm::ivec3(x, y, z);" -> LOCAL
            
            // So we must convert back.
            glm::ivec3 globalHit = hitPos + c->chunkPosition * CHUNK_SIZE;
            glm::ivec3 globalPre = prePos + c->chunkPosition * CHUNK_SIZE;
            
            // Distance check
            // Use center of block for crude distance?
            // Or exact distance?
            // Chunk::raycast logic steps along the ray. 
            // It doesn't return the exact float intersection.
            // However, since we step from origin, the 'd' (distance) is implicitly roughly known.
            // But Chunk::raycast doesn't return 'd'.
            
            // We can calculate distance from origin to center of block.
            glm::vec3 blockCenter = glm::vec3(globalHit) + glm::vec3(0.5f);
            float dist = glm::distance(origin, blockCenter);
            
            if(dist < closestDist)
            {
                closestDist = dist;
                bestPos = globalHit;
                bestPrePos = globalPre;
                hitAny = true;
            }
        }
    }
    
    if(hitAny)
    {
        outputPos = bestPos;
        outputPrePos = bestPrePos;
        return true;
    }
    return false;
}
