
#include "World.h"
#include "WorldGenerator.h"
#include <cmath>
#include <limits>
#include <iostream>
#include <array>
#include <algorithm>
#include <glm/gtc/matrix_access.hpp>

World::World() : shutdown(false) {
    workerThread = std::thread(&World::WorkerLoop, this);
    
    // Start Generation Threads (e.g., 2-4 threads)
    int numGenThreads = std::thread::hardware_concurrency() / 2;
    if(numGenThreads < 1) numGenThreads = 1;
    for(int i=0; i<numGenThreads; ++i) {
        genThreads.emplace_back(&World::GenerationWorkerLoop, this);
    }
}

World::~World() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        shutdown = true;
    }
    condition.notify_one();
    if(workerThread.joinable()) {
        workerThread.join();
    }
    
    // Join Gen Threads
    genCondition.notify_all();
    for(auto& t : genThreads) {
        if(t.joinable()) t.join();
    }
}

void World::WorkerLoop() {
    while(true) {
        Chunk* c = nullptr;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this]{ return !meshQueue.empty() || shutdown; });
            
            if(shutdown && meshQueue.empty()) break;
            
            if(!meshQueue.empty()) {
                c = meshQueue.front();
                meshQueue.pop_front();
            }
        }
        
        if(c) {
            // Collecting geometry
            std::vector<float> data = c->generateGeometry();
            
            // Queue for upload
            {
                std::lock_guard<std::mutex> lock(uploadMutex);
                uploadQueue.push_back({c, std::move(data)});
            }
        }
    }
}

void World::Update() {
    std::vector<std::pair<Chunk*, std::vector<float>>> toUpload;
    {
        std::lock_guard<std::mutex> lock(uploadMutex);
        if(!uploadQueue.empty()) {
            toUpload = std::move(uploadQueue);
            uploadQueue.clear();
        }
    }
    
    for(auto& pair : toUpload) {
        if(pair.first) pair.first->uploadMesh(pair.second);
    }
}

void World::QueueMeshUpdate(Chunk* c, bool priority) {
    if(c) {
        std::lock_guard<std::mutex> lock(queueMutex);
        if(priority) meshQueue.push_front(c);
        else meshQueue.push_back(c);
        condition.notify_one();
    }
}





void World::GenerationWorkerLoop() {
    WorldGenerator generator;
    while(true) {
        std::tuple<int, int, int> coord;
        {
            std::unique_lock<std::mutex> lock(genMutex);
            genCondition.wait(lock, [this]{ return !genQueue.empty() || shutdown; });
            
            if(shutdown && genQueue.empty()) break;
            
            if(!genQueue.empty()) {
                coord = genQueue.front();
                genQueue.pop_front();
            } else continue;
        }
        
        int x = std::get<0>(coord);
        int y = std::get<1>(coord);
        int z = std::get<2>(coord);

        // check if already exists (might have been added by another thread)
        {
            std::lock_guard<std::mutex> lock(worldMutex);
            if(chunks.find(coord) != chunks.end()) {
                 // Remove from generating set
                 std::lock_guard<std::mutex> gLock(genMutex);
                 generatingChunks.erase(coord);
                 continue;
            }
        }

        // 1. Create Chunk
        auto newChunk = std::make_unique<Chunk>();
        newChunk->chunkPosition = glm::ivec3(x, y, z);
        newChunk->setWorld(this);
        
        // 2. Generate Blocks
        generator.GenerateChunk(*newChunk);
        
        // 3. Add to World (This links neighbors)
        {
            std::lock_guard<std::mutex> lock(worldMutex);
            chunks[coord] = std::move(newChunk);
            Chunk* c = chunks[coord].get();
            
            // Link Neighbors (Copied from addChunk because we need to link NOW to calculate light)
             int dx[] = {0, 0, -1, 1, 0, 0};
             int dy[] = {0, 0, 0, 0, 1, -1};
             int dz[] = {1, -1, 0, 0, 0, 0};
             int dirs[] = {Chunk::DIR_FRONT, Chunk::DIR_BACK, Chunk::DIR_LEFT, Chunk::DIR_RIGHT, Chunk::DIR_TOP, Chunk::DIR_BOTTOM};
             int opps[] = {Chunk::DIR_BACK, Chunk::DIR_FRONT, Chunk::DIR_RIGHT, Chunk::DIR_LEFT, Chunk::DIR_BOTTOM, Chunk::DIR_TOP};

             for(int i=0; i<6; ++i) {
                  auto it = chunks.find(std::make_tuple(x + dx[i], y + dy[i], z + dz[i]));
                  if(it != chunks.end()) {
                      Chunk* n = it->second.get();
                      c->neighbors[dirs[i]] = n;
                      n->neighbors[opps[i]] = c;
                  }
             }
        }
        
        Chunk* c = getChunk(x, y, z); // Safe retrieval
        if(c) {
             // 4. Calculate Light
             c->calculateSunlight();
             c->calculateBlockLight();
             
             // 5. Spread Light (Might need neighboring chunks)
             c->spreadLight();
             
             // 6. Queue Mesh (Low Priority for Generation)
             QueueMeshUpdate(c, false);
             
             // Also queue neighbors for mesh update if they exist
             for(int i=0; i<6; ++i) {
                 if(c->neighbors[i]) {
                     Chunk* n = c->neighbors[i];
                     if(!n->meshDirty) { // Optimization: Only if not already dirty? 
                         // Actually, we must run spreadLight because we might have blocked their light or provided new light
                         // But we should check if they are fully generated? 
                         // Assuming neighbors in graph are valid.
                         n->spreadLight(); 
                         QueueMeshUpdate(n, false);
                     } else {
                         // Even if dirty, light might need update?
                         n->spreadLight();
                         QueueMeshUpdate(n, false);
                     }
                 }
             }
        }

        // Remove from generating set
        {
            std::lock_guard<std::mutex> lock(genMutex);
            generatingChunks.erase(coord);
        }
    }
}

void World::loadChunks(const glm::vec3& playerPos, int renderDistance) {
    int cx = (int)floor(playerPos.x / CHUNK_SIZE);
    int cz = (int)floor(playerPos.z / CHUNK_SIZE);
    
    // Simple radius check
    // Spiraling or distance sort would be better but simple loops for now
    for(int x = cx - renderDistance; x <= cx + renderDistance; ++x) {
        for(int z = cz - renderDistance; z <= cz + renderDistance; ++z) {
             int distSq = (x-cx)*(x-cx) + (z-cz)*(z-cz);
             if(distSq > renderDistance * renderDistance) continue;
             
             // Generate columns 0 to 4 (limited height for now)
             for(int y=0; y<5; ++y) {
                 auto key = std::make_tuple(x, y, z);
                 
                 bool exists = false;
                 {
                     std::lock_guard<std::mutex> lock(worldMutex);
                     if(chunks.find(key) != chunks.end()) exists = true;
                 }
                 
                 if(!exists) {
                     std::lock_guard<std::mutex> lock(genMutex);
                     if(generatingChunks.find(key) == generatingChunks.end()) {
                         generatingChunks.insert(key);
                         genQueue.push_back(key);
                         genCondition.notify_one();
                     }
                 }
             }
        }
    }
}

void World::addChunk(int x, int y, int z)
{
    std::unique_lock<std::mutex> lock(worldMutex);
    auto key = std::make_tuple(x, y, z);
    if(chunks.find(key) == chunks.end())
    {
        auto newChunk = std::make_unique<Chunk>();
        newChunk->chunkPosition = glm::ivec3(x, y, z);
        newChunk->setWorld(this);
        chunks[key] = std::move(newChunk);
        Chunk* c = chunks[key].get();
        
        // Link Neighbors (Under World Lock)
        // Order: Front(Z+), Back(Z-), Left(X-), Right(X+), Top(Y+), Bottom(Y-)
        int dx[] = {0, 0, -1, 1, 0, 0};
        int dy[] = {0, 0, 0, 0, 1, -1};
        int dz[] = {1, -1, 0, 0, 0, 0};
        int dirs[] = {Chunk::DIR_FRONT, Chunk::DIR_BACK, Chunk::DIR_LEFT, Chunk::DIR_RIGHT, Chunk::DIR_TOP, Chunk::DIR_BOTTOM};
        int opps[] = {Chunk::DIR_BACK, Chunk::DIR_FRONT, Chunk::DIR_RIGHT, Chunk::DIR_LEFT, Chunk::DIR_BOTTOM, Chunk::DIR_TOP}; // Opposites
        
        for(int i=0; i<6; ++i) {
             auto it = chunks.find(std::make_tuple(x + dx[i], y + dy[i], z + dz[i]));
             if(it != chunks.end()) {
                 Chunk* n = it->second.get();
                 c->neighbors[dirs[i]] = n;
                 n->neighbors[opps[i]] = c;
             }
        }
        
        lock.unlock(); // Safe to unlock now
        
        // Queue initial mesh generation
        QueueMeshUpdate(c, false);
        
        // Mark neighbors as dirty
        for(int i=0; i<6; ++i) {
            Chunk* n = c->neighbors[dirs[i]];
            if(n) QueueMeshUpdate(n);
        }
    }
}

Chunk* World::getChunk(int chunkX, int chunkY, int chunkZ)
{
    std::lock_guard<std::mutex> lock(worldMutex);
    auto key = std::make_tuple(chunkX, chunkY, chunkZ);
    auto it = chunks.find(key);
    if(it != chunks.end())
        return it->second.get();
    return nullptr;
}

const Chunk* World::getChunk(int chunkX, int chunkY, int chunkZ) const
{
    std::lock_guard<std::mutex> lock(worldMutex);
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
    
    int lx = x - cx * CHUNK_SIZE;
    int ly = y - cy * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
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

        QueueMeshUpdate(c, true); // Update Parent SuperChunk (High Priority)

        // Update neighbor chunks (Propagate Light)
        int nDx[] = {-1, 1, 0, 0, 0, 0};
        int nDy[] = {0, 0, -1, 1, 0, 0};
        int nDz[] = {0, 0, 0, 0, -1, 1};
        
        for(int i=0; i<6; ++i) {
            Chunk* n = getChunk(cx + nDx[i], cy + nDy[i], cz + nDz[i]);
            if(n) {
                 n->calculateSunlight();
                 n->calculateBlockLight();
                 n->spreadLight();
                 QueueMeshUpdate(n, true);
            }
        }
        
        // If we placed a light source, or removed one, we must update neighbors that light might reach
        // Simple but expensive approach: Update all 6 neighbors + diagonals? 
        // For now, let's just stick to immediate neighbors
        // Light propagation is handled in spreadLight() which pushes to queues.
        // It sets meshDirty. But now meshDirty does nothing unless we check it.
        // TODO: Ideally spreadLight should call QueueMeshUpdate instead of just setting dirty.
        // But spreadLight operates on many chunks.
        
        // Let's iterate neighbors and queue them if they were touched?
        // Actually, for simplicity, we rely on the above neighbor check.
    }
    
    // Check for light propagation downwards (Sky Light)
    // If we placed a block, we blocked sky light.
    // If we removed a block, we opened sky light.
    // This is handled by calculateSunlight -> but we need to propagate to lower chunks
    if(c) {
        if(type == AIR) {
            // Block removed - sunlight might go down
             Chunk* lower = getChunk(cx, cy-1, cz);
             if(lower) {
                lower->calculateSunlight();
                lower->calculateBlockLight();
                lower->spreadLight();
                QueueMeshUpdate(lower, true);
             }
        } else {
            // Block placed - might shadow lower chunk
            Chunk* lower = getChunk(cx, cy-1, cz);
            if(lower) {
                lower->calculateSunlight();
                lower->calculateBlockLight();
                lower->spreadLight();
                QueueMeshUpdate(lower, true);
                
                // Also update neighbors of this lower chunk, as light might have spread to them
                int nDx[] = {-1, 1, 0, 0};
                int nDz[] = {0, 0, -1, 1};
                for(int i=0; i<4; ++i) {
                    Chunk* n = getChunk(cx + nDx[i], cy-1, cz + nDz[i]);
                    if(n) {
                        n->spreadLight();
                    }
                }
            }
        }
    }
}

// Helper to extract frustum planes
// Each plane is vec4 (a, b, c, d) where ax+by+cz+d=0
std::array<glm::vec4, 6> extractPlanes(const glm::mat4& m) {
    std::array<glm::vec4, 6> planes;
    // Left
    planes[0] = glm::row(m, 3) + glm::row(m, 0);
    // Right
    planes[1] = glm::row(m, 3) - glm::row(m, 0);
    // Bottom
    planes[2] = glm::row(m, 3) + glm::row(m, 1);
    // Top
    planes[3] = glm::row(m, 3) - glm::row(m, 1);
    // Near
    planes[4] = glm::row(m, 3) + glm::row(m, 2);
    // Far
    planes[5] = glm::row(m, 3) - glm::row(m, 2);

    for(int i=0; i<6; ++i) {
        float len = glm::length(glm::vec3(planes[i]));
        planes[i] /= len;
    }
    return planes;
}

// Helper to check AABB vs Frustum
bool isAABBInFrustum(const glm::vec3& min, const glm::vec3& max, const std::array<glm::vec4, 6>& planes) {
    for(const auto& plane : planes) {
        // p-vertex (direction of normal)
        glm::vec3 p;
        p.x = plane.x > 0 ? max.x : min.x;
        p.y = plane.y > 0 ? max.y : min.y;
        p.z = plane.z > 0 ? max.z : min.z;
        
        // If p-vertex is on negative side of plane, box is outside
        if (glm::dot(glm::vec3(plane), p) + plane.w < 0)
            return false;
    }
    return true;
}

void World::render(Shader& shader, const glm::mat4& viewProjection)
{
    std::lock_guard<std::mutex> lock(worldMutex);
    
    // Frustum Culling
    auto planes = extractPlanes(viewProjection);
    
    for(auto& pair : chunks)
    {
        Chunk* c = pair.second.get();
        if(c->meshDirty) {
             QueueMeshUpdate(c);
             c->meshDirty = false;
        }
        
        // Culling
        glm::vec3 min = glm::vec3(c->chunkPosition.x * CHUNK_SIZE, c->chunkPosition.y * CHUNK_SIZE, c->chunkPosition.z * CHUNK_SIZE);
        glm::vec3 max = min + glm::vec3(CHUNK_SIZE);
        
        if(isAABBInFrustum(min, max, planes)) {
            c->render(shader, viewProjection);
        }
    }
}
// Removed getSuperChunk/getOrCreateSuperChunk definitions


bool World::raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, glm::ivec3& outputPos, glm::ivec3& outputPrePos)
{
    // Naive: check all chunks, find closest hit
    bool hitAny = false;
    float closestDist = maxDist + 1.0f;
    glm::ivec3 bestPos;
    glm::ivec3 bestPrePos;
    
    std::lock_guard<std::mutex> lock(worldMutex);
    
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

size_t World::getChunkCount() const {
    return chunks.size();
}
