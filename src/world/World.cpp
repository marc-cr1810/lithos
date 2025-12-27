#include "World.h"
#include "../ecs/Systems.h"
#include "WorldGenerator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <limits>

// Helper to extract frustum planes
// Each plane is vec4 (a, b, c, d) where ax+by+cz+d=0
std::array<glm::vec4, 6> extractPlanes(const glm::mat4 &m) {
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

  for (int i = 0; i < 6; ++i) {
    float len = glm::length(glm::vec3(planes[i]));
    planes[i] /= len;
  }
  return planes;
}

// Helper to check AABB vs Frustum
bool isAABBInFrustum(const glm::vec3 &min, const glm::vec3 &max,
                     const std::array<glm::vec4, 6> &planes) {
  for (const auto &plane : planes) {
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

World::World() : shutdown(false) {
  // Random Seed
  std::srand(std::time(nullptr));
  worldSeed = std::rand();
  std::cout << "World Seed: " << worldSeed << std::endl;

  // Start Mesh Threads
  int numMeshThreads = std::thread::hardware_concurrency();
  if (numMeshThreads < 1)
    numMeshThreads = 1;

  // We already use some for generation, maybe balance it?
  // Let's just use hardware_concurrency for meshing as it's the bottleneck.
  for (int i = 0; i < numMeshThreads; ++i) {
    meshThreads.emplace_back(&World::WorkerLoop, this);
  }

  // Start Generation Threads (e.g., 2-4 threads)
  int numGenThreads = std::thread::hardware_concurrency() / 2;
  if (numGenThreads < 1)
    numGenThreads = 1;
  for (int i = 0; i < numGenThreads; ++i) {
    genThreads.emplace_back(&World::GenerationWorkerLoop, this);
  }
}

World::~World() {
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    shutdown = true;
  }
  condition.notify_all();
  for (auto &t : meshThreads) {
    if (t.joinable())
      t.join();
  }

  // Join Gen Threads
  genCondition.notify_all();
  for (auto &t : genThreads) {
    if (t.joinable())
      t.join();
  }
}

void World::WorkerLoop() {
  while (true) {
    Chunk *c = nullptr;
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      condition.wait(lock, [this] {
        return !meshQueue.empty() || !meshQueueHighPrio.empty() || shutdown;
      });

      if (shutdown && meshQueue.empty() && meshQueueHighPrio.empty())
        break;

      // Check high-priority queue first (block breaks)
      if (!meshQueueHighPrio.empty()) {
        c = meshQueueHighPrio.front();
        meshQueueHighPrio.pop_front();
        meshSet.erase(c);
      } else if (!meshQueue.empty()) {
        c = meshQueue.front();
        meshQueue.pop_front();
        meshSet.erase(c);
      }
    }

    if (c) {
      // Recalculate lighting if needed (moved from main thread)
      if (c->needsLightingUpdate) {
        c->calculateSunlight();
        c->calculateBlockLight();
        c->spreadLight();
        c->needsLightingUpdate = false;
      }

      // Collecting geometry
      int opaqueCount = 0;
      std::vector<float> data = c->generateGeometry(opaqueCount);

      // Queue for upload
      {
        std::lock_guard<std::mutex> lock(uploadMutex);
        uploadQueue.emplace_back(c, std::move(data), opaqueCount);
      }
    }
  }
}

void World::Tick() {
  currentTick++;
  updateBlocks();

  // ECS Update (Fixed Time Step: 1/20 = 0.05s)
  PhysicsSystem::Update(registry, 0.05f);
  CollisionSystem::Update(registry, *this, 0.05f);
}

void World::Update() {
  std::vector<std::tuple<Chunk *, std::vector<float>, int>> toUpload;
  {
    std::lock_guard<std::mutex> lock(uploadMutex);
    if (!uploadQueue.empty()) {
      toUpload = std::move(uploadQueue);
      uploadQueue.clear();
    }
  }

  for (auto &t : toUpload) {
    if (std::get<0>(t))
      std::get<0>(t)->uploadMesh(std::get<1>(t), std::get<2>(t));
  }
}

void World::scheduleBlockUpdate(int x, int y, int z, int delay) {
  std::lock_guard<std::mutex> lock(updateQueueMutex);
  updateQueue.push({x, y, z, currentTick + delay});
}

void World::updateBlocks() {
  // Process queue
  // We only process updates due for this tick or earlier
  std::lock_guard<std::mutex> lock(updateQueueMutex);

  while (!updateQueue.empty()) {
    if (updateQueue.top().tick > currentTick)
      break;

    BlockUpdate u = updateQueue.top();
    updateQueue.pop();

    // Unlock while updating to allow scheduling new updates
    updateQueueMutex.unlock();

    ChunkBlock b = getBlock(u.x, u.y, u.z);
    if (b.isActive()) {
      b.block->update(*this, u.x, u.y, u.z);
    }

    updateQueueMutex.lock();
  }
}

void World::QueueMeshUpdate(Chunk *c, bool priority) {
  if (c) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (meshSet.find(c) == meshSet.end()) {
      // Add to appropriate queue based on priority
      if (priority)
        meshQueueHighPrio.push_back(c);
      else
        meshQueue.push_back(c);
      meshSet.insert(c);
      condition.notify_one();
    }
    // If already queued, we don't re-add (deduplication)
    // High priority requests for already-queued chunks are handled
    // by workers checking high-priority queue first
  }
}

void World::GenerationWorkerLoop() {
  WorldGenerator generator(worldSeed);
  while (true) {
    std::tuple<int, int, int> coord;
    {
      std::unique_lock<std::mutex> lock(genMutex);
      genCondition.wait(lock, [this] { return !genQueue.empty() || shutdown; });

      if (shutdown && genQueue.empty())
        break;

      if (!genQueue.empty()) {
        coord = genQueue.front();
        genQueue.pop_front();
      } else
        continue;
    }

    int x = std::get<0>(coord);
    int y = std::get<1>(coord);
    int z = std::get<2>(coord);

    // check if already exists (might have been added by another thread)
    {
      std::lock_guard<std::mutex> lock(worldMutex);
      if (chunks.find(coord) != chunks.end()) {
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
      Chunk *c = chunks[coord].get();

      // Link Neighbors (Copied from addChunk because we need to link NOW to
      // calculate light)
      int dx[] = {0, 0, -1, 1, 0, 0};
      int dy[] = {0, 0, 0, 0, 1, -1};
      int dz[] = {1, -1, 0, 0, 0, 0};
      int dirs[] = {Chunk::DIR_FRONT, Chunk::DIR_BACK, Chunk::DIR_LEFT,
                    Chunk::DIR_RIGHT, Chunk::DIR_TOP,  Chunk::DIR_BOTTOM};
      int opps[] = {Chunk::DIR_BACK, Chunk::DIR_FRONT,  Chunk::DIR_RIGHT,
                    Chunk::DIR_LEFT, Chunk::DIR_BOTTOM, Chunk::DIR_TOP};

      for (int i = 0; i < 6; ++i) {
        auto it = chunks.find(std::make_tuple(x + dx[i], y + dy[i], z + dz[i]));
        if (it != chunks.end()) {
          Chunk *n = it->second.get();
          c->neighbors[dirs[i]] = n;
          n->neighbors[opps[i]] = c;
        }
      }
    }

    Chunk *c = getChunk(x, y, z); // Safe retrieval
    if (c) {
      // 4. Calculate Light
      c->calculateSunlight();
      c->calculateBlockLight();

      // 5. Spread Light (Might need neighboring chunks)
      c->spreadLight();

      // 6. Queue Mesh (Low Priority for Generation)
      QueueMeshUpdate(c, false);

      // Also queue neighbors for mesh update if they exist
      // Also queue neighbors for mesh update if they exist
      int dirs_indices[] = {Chunk::DIR_FRONT, Chunk::DIR_BACK,
                            Chunk::DIR_LEFT,  Chunk::DIR_RIGHT,
                            Chunk::DIR_TOP,   Chunk::DIR_BOTTOM};

      for (int i = 0; i < 6; ++i) {
        if (c->neighbors[dirs_indices[i]]) {
          Chunk *n = c->neighbors[dirs_indices[i]];

          // FIX: If we added a chunk ABOVE this neighbor, force it to
          // re-calculate sunlight because we might have just blocked the sky.
          if (dirs_indices[i] == Chunk::DIR_BOTTOM) {
            n->calculateSunlight();
            n->calculateBlockLight();
          }

          n->spreadLight();
          QueueMeshUpdate(n, false);
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

void World::loadChunks(const glm::vec3 &playerPos, int renderDistance,
                       const glm::mat4 &viewProjection) {
  int cx = (int)floor(playerPos.x / CHUNK_SIZE);
  int cz = (int)floor(playerPos.z / CHUNK_SIZE);

  // Priority Queue: Sort chunks by distance, visibility, and height
  struct ChunkRequest {
    int x, y, z;
    float priority;

    bool operator<(const ChunkRequest &other) const {
      return priority < other.priority; // Min heap (we want max priority first)
    }
  };

  static std::vector<ChunkRequest> loadQueue;
  static int lastCx = INT_MIN;
  static int lastCz = INT_MIN;
  static int lastRenderDistance = -1;
  static size_t queueIndex = 0;

  // Rebuild queue when player moves to new chunk
  if (cx != lastCx || cz != lastCz || renderDistance != lastRenderDistance) {
    loadQueue.clear();
    queueIndex = 0;

    auto planes = extractPlanes(viewProjection);
    int minX = cx - renderDistance;
    int maxX = cx + renderDistance;
    int minZ = cz - renderDistance;
    int maxZ = cz + renderDistance;
    int renderDistSq = renderDistance * renderDistance;

    // Build priority queue of all chunks in range
    for (int x = minX; x <= maxX; ++x) {
      for (int z = minZ; z <= maxZ; ++z) {
        int dx = x - cx;
        int dz = z - cz;
        int distSq = dx * dx + dz * dz;

        if (distSq <= renderDistSq) {
          float distance = std::sqrt((float)distSq);

          for (int y = 0; y < 5; ++y) {
            // Calculate priority score
            float priority = 1000.0f / (distance + 1.0f); // Closer = higher

            // Check if in frustum
            glm::vec3 min =
                glm::vec3(x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE);
            glm::vec3 max = min + glm::vec3(CHUNK_SIZE);
            if (isAABBInFrustum(min, max, planes)) {
              priority *= 2.0f; // Double priority if visible
            }

            // Boost ground-level chunks
            if (y <= 2) {
              priority *= 1.5f;
            }

            loadQueue.push_back({x, y, z, priority});
          }
        }
      }
    }

    // Sort by priority (highest first)
    std::sort(loadQueue.begin(), loadQueue.end(),
              [](const ChunkRequest &a, const ChunkRequest &b) {
                return a.priority > b.priority;
              });

    lastCx = cx;
    lastCz = cz;
    lastRenderDistance = renderDistance;
  }

  auto planes = extractPlanes(viewProjection);

  // Process top N chunks from queue
  const int MAX_CHUNKS_PER_FRAME = 80;
  int chunksProcessed = 0;

  while (chunksProcessed < MAX_CHUNKS_PER_FRAME &&
         queueIndex < loadQueue.size()) {
    const auto &req = loadQueue[queueIndex];
    queueIndex++;

    auto key = std::make_tuple(req.x, req.y, req.z);

    bool exists = false;
    {
      std::lock_guard<std::mutex> lock(worldMutex);
      if (chunks.find(key) != chunks.end())
        exists = true;
    }

    if (!exists) {
      std::lock_guard<std::mutex> lock(genMutex);
      if (generatingChunks.find(key) == generatingChunks.end()) {
        generatingChunks.insert(key);

        // High priority chunks go to front
        if (req.priority > 100.0f) {
          genQueue.push_front(key);
        } else {
          genQueue.push_back(key);
        }
        genCondition.notify_one();
      }
    }

    chunksProcessed++;
  }

  // Reset queue when finished
  if (queueIndex >= loadQueue.size()) {
    queueIndex = 0;
  }
}

void World::unloadChunks(const glm::vec3 &playerPos, int renderDistance) {
  int cx = (int)floor(playerPos.x / CHUNK_SIZE);
  int cz = (int)floor(playerPos.z / CHUNK_SIZE);

  // Unload distance = render distance + buffer to avoid thrashing
  int unloadDistance = renderDistance + 2;
  int unloadDistSq = unloadDistance * unloadDistance;

  std::vector<std::tuple<int, int, int>> toUnload;

  // Find chunks to unload
  {
    std::lock_guard<std::mutex> lock(worldMutex);
    for (auto &pair : chunks) {
      auto [x, y, z] = pair.first;
      int dx = x - cx;
      int dz = z - cz;
      int distSq = dx * dx + dz * dz;

      // Only check horizontal distance, keep all Y levels
      if (distSq > unloadDistSq) {
        toUnload.push_back(pair.first);
      }
    }
  }

  // Unload chunks
  for (auto &key : toUnload) {
    auto [x, y, z] = key;

    // Get chunk before erasing
    Chunk *chunkToUnload = nullptr;
    {
      std::lock_guard<std::mutex> lock(worldMutex);
      auto it = chunks.find(key);
      if (it != chunks.end()) {
        chunkToUnload = it->second.get();
      }
    }

    if (chunkToUnload) {
      // Unlink neighbors
      int dx[] = {0, 0, -1, 1, 0, 0};
      int dy[] = {0, 0, 0, 0, 1, -1};
      int dz[] = {1, -1, 0, 0, 0, 0};
      int dirs[] = {Chunk::DIR_FRONT, Chunk::DIR_BACK, Chunk::DIR_LEFT,
                    Chunk::DIR_RIGHT, Chunk::DIR_TOP,  Chunk::DIR_BOTTOM};
      int opps[] = {Chunk::DIR_BACK, Chunk::DIR_FRONT,  Chunk::DIR_RIGHT,
                    Chunk::DIR_LEFT, Chunk::DIR_BOTTOM, Chunk::DIR_TOP};

      for (int i = 0; i < 6; ++i) {
        Chunk *neighbor = chunkToUnload->neighbors[dirs[i]];
        if (neighbor) {
          neighbor->neighbors[opps[i]] = nullptr;
          chunkToUnload->neighbors[dirs[i]] = nullptr;
        }
      }

      // Remove from mesh queue if present
      {
        std::lock_guard<std::mutex> lock(queueMutex);
        meshSet.erase(chunkToUnload);
        // Note: Can't easily remove from deque, but meshSet prevents processing
      }

      // Remove from upload queue if present (CRITICAL!)
      {
        std::lock_guard<std::mutex> lock(uploadMutex);
        uploadQueue.erase(std::remove_if(uploadQueue.begin(), uploadQueue.end(),
                                         [chunkToUnload](const auto &item) {
                                           return std::get<0>(item) ==
                                                  chunkToUnload;
                                         }),
                          uploadQueue.end());
      }

      // Remove from generation queue if present
      {
        std::lock_guard<std::mutex> lock(genMutex);
        generatingChunks.erase(key);
        // Note: Can't easily remove from deque
      }

      // Finally, erase the chunk
      {
        std::lock_guard<std::mutex> lock(worldMutex);
        chunks.erase(key);
      }
    }
  }
}

void World::addChunk(int x, int y, int z) {
  std::unique_lock<std::mutex> lock(worldMutex);
  auto key = std::make_tuple(x, y, z);
  if (chunks.find(key) == chunks.end()) {
    auto newChunk = std::make_unique<Chunk>();
    newChunk->chunkPosition = glm::ivec3(x, y, z);
    newChunk->setWorld(this);
    chunks[key] = std::move(newChunk);
    Chunk *c = chunks[key].get();

    // Link Neighbors (Under World Lock)
    // Order: Front(Z+), Back(Z-), Left(X-), Right(X+), Top(Y+), Bottom(Y-)
    int dx[] = {0, 0, -1, 1, 0, 0};
    int dy[] = {0, 0, 0, 0, 1, -1};
    int dz[] = {1, -1, 0, 0, 0, 0};
    int dirs[] = {Chunk::DIR_FRONT, Chunk::DIR_BACK, Chunk::DIR_LEFT,
                  Chunk::DIR_RIGHT, Chunk::DIR_TOP,  Chunk::DIR_BOTTOM};
    int opps[] = {Chunk::DIR_BACK,   Chunk::DIR_FRONT,
                  Chunk::DIR_RIGHT,  Chunk::DIR_LEFT,
                  Chunk::DIR_BOTTOM, Chunk::DIR_TOP}; // Opposites

    for (int i = 0; i < 6; ++i) {
      auto it = chunks.find(std::make_tuple(x + dx[i], y + dy[i], z + dz[i]));
      if (it != chunks.end()) {
        Chunk *n = it->second.get();
        c->neighbors[dirs[i]] = n;
        n->neighbors[opps[i]] = c;
      }
    }

    lock.unlock(); // Safe to unlock now

    // Queue initial mesh generation
    QueueMeshUpdate(c, false);

    // Mark neighbors as dirty
    // Mark neighbors as dirty
    for (int i = 0; i < 6; ++i) {
      Chunk *n = c->neighbors[dirs[i]];
      if (n) {
        if (dirs[i] == Chunk::DIR_BOTTOM) {
          n->calculateSunlight();
          n->calculateBlockLight();
        }
        n->spreadLight();
        QueueMeshUpdate(n);
      }
    }
  }
}

Chunk *World::getChunk(int chunkX, int chunkY, int chunkZ) {
  std::lock_guard<std::mutex> lock(worldMutex);
  auto key = std::make_tuple(chunkX, chunkY, chunkZ);
  auto it = chunks.find(key);
  if (it != chunks.end())
    return it->second.get();
  return nullptr;
}

const Chunk *World::getChunk(int chunkX, int chunkY, int chunkZ) const {
  std::lock_guard<std::mutex> lock(worldMutex);
  auto key = std::make_tuple(chunkX, chunkY, chunkZ);
  auto it = chunks.find(key);
  if (it != chunks.end())
    return it->second.get();
  return nullptr;
}

// Helper for floor division
int floorDiv(int a, int b) { return (a >= 0) ? (a / b) : ((a - b + 1) / b); }

ChunkBlock World::getBlock(int x, int y, int z) const {
  int cx = floorDiv(x, CHUNK_SIZE);
  int cy = floorDiv(y, CHUNK_SIZE);
  int cz = floorDiv(z, CHUNK_SIZE);

  const Chunk *c = getChunk(cx, cy, cz);
  if (!c) {
    // Return Air with full sunlight (simulate open world)
    return ChunkBlock{BlockRegistry::getInstance().getBlock(AIR), 15, 0};
  }

  int lx = x - cx * CHUNK_SIZE;
  int ly = y - cy * CHUNK_SIZE;
  int lz = z - cz * CHUNK_SIZE;
  if (lx < 0)
    lx += CHUNK_SIZE;
  if (ly < 0)
    ly += CHUNK_SIZE;
  if (lz < 0)
    lz += CHUNK_SIZE;

  return c->getBlock(lx, ly, lz);
}

uint8_t World::getSkyLight(int x, int y, int z) {
  int cx = floorDiv(x, CHUNK_SIZE);
  int cy = floorDiv(y, CHUNK_SIZE);
  int cz = floorDiv(z, CHUNK_SIZE);

  Chunk *c = getChunk(cx, cy, cz);
  if (!c)
    return 15; // Sunlight is bright outside

  int lx = x % CHUNK_SIZE;
  int ly = y % CHUNK_SIZE;
  int lz = z % CHUNK_SIZE;
  if (lx < 0)
    lx += CHUNK_SIZE;
  if (ly < 0)
    ly += CHUNK_SIZE;
  if (lz < 0)
    lz += CHUNK_SIZE;

  return c->getSkyLight(lx, ly, lz);
}

uint8_t World::getBlockLight(int x, int y, int z) {
  int cx = floorDiv(x, CHUNK_SIZE);
  int cy = floorDiv(y, CHUNK_SIZE);
  int cz = floorDiv(z, CHUNK_SIZE);

  Chunk *c = getChunk(cx, cy, cz);
  if (!c)
    return 0; // Block light is dark outside

  int lx = x % CHUNK_SIZE;
  int ly = y % CHUNK_SIZE;
  int lz = z % CHUNK_SIZE;
  if (lx < 0)
    lx += CHUNK_SIZE;
  if (ly < 0)
    ly += CHUNK_SIZE;
  if (lz < 0)
    lz += CHUNK_SIZE;

  return c->getBlockLight(lx, ly, lz);
}

uint8_t World::getMetadata(int x, int y, int z) {
  int cx = floorDiv(x, CHUNK_SIZE);
  int cy = floorDiv(y, CHUNK_SIZE);
  int cz = floorDiv(z, CHUNK_SIZE);

  Chunk *c = getChunk(cx, cy, cz);
  if (!c)
    return 0;

  int lx = x % CHUNK_SIZE;
  int ly = y % CHUNK_SIZE;
  int lz = z % CHUNK_SIZE;
  if (lx < 0)
    lx += CHUNK_SIZE;
  if (ly < 0)
    ly += CHUNK_SIZE;
  if (lz < 0)
    lz += CHUNK_SIZE;

  return c->getMetadata(lx, ly, lz);
}

void World::setMetadata(int x, int y, int z, uint8_t val) {
  int cx = floorDiv(x, CHUNK_SIZE);
  int cy = floorDiv(y, CHUNK_SIZE);
  int cz = floorDiv(z, CHUNK_SIZE);

  int lx = x - cx * CHUNK_SIZE;
  int ly = y - cy * CHUNK_SIZE;
  int lz = z - cz * CHUNK_SIZE;

  Chunk *c = getChunk(cx, cy, cz);
  if (c) {
    c->setMetadata(lx, ly, lz, val);
    QueueMeshUpdate(c);
  }
}

void World::setBlock(int x, int y, int z, BlockType type) {
  int cx = floorDiv(x, CHUNK_SIZE);
  int cy = floorDiv(y, CHUNK_SIZE);
  int cz = floorDiv(z, CHUNK_SIZE);

  int lx = x - cx * CHUNK_SIZE;
  int ly = y - cy * CHUNK_SIZE;
  int lz = z - cz * CHUNK_SIZE;

  Chunk *c = getChunk(cx, cy, cz);
  if (c) {
    c->setBlock(lx, ly, lz, type);

    // Mark for lighting recalculation (done in worker thread)
    c->needsLightingUpdate = true;
    QueueMeshUpdate(c, true); // High priority for instant visual feedback

    // Update neighbor chunks
    int nDx[] = {-1, 1, 0, 0, 0, 0};
    int nDy[] = {0, 0, -1, 1, 0, 0};
    int nDz[] = {0, 0, 0, 0, -1, 1};

    for (int i = 0; i < 6; ++i) {
      Chunk *n = getChunk(cx + nDx[i], cy + nDy[i], cz + nDz[i]);
      if (n) {
        n->needsLightingUpdate = true;
        QueueMeshUpdate(n, true);
      }
    }

    // If we placed a light source, or removed one, we must update neighbors
    // that light might reach Simple but expensive approach: Update all 6
    // neighbors + diagonals? For now, let's just stick to immediate neighbors
    // Light propagation is handled in spreadLight() which pushes to queues.
    // It sets meshDirty. But now meshDirty does nothing unless we check it.
    // TODO: Ideally spreadLight should call QueueMeshUpdate instead of just
    // setting dirty. But spreadLight operates on many chunks.

    // Let's iterate neighbors and queue them if they were touched?
    // Actually, for simplicity, we rely on the above neighbor check.
  }

  // Check for light propagation downwards (Sky Light)
  // If we placed a block, we blocked sky light.
  // If we removed a block, we opened sky light.
  // This is handled by calculateSunlight -> but we need to propagate to lower
  // chunks
  if (c) {
    if (type == AIR) {
      // Block removed - sunlight might go down
      Chunk *lower = getChunk(cx, cy - 1, cz);
      if (lower) {
        lower->calculateSunlight();
        lower->calculateBlockLight();
        lower->spreadLight();
        QueueMeshUpdate(lower, true);
      }
    } else {
      // Block placed - might shadow lower chunk
      Chunk *lower = getChunk(cx, cy - 1, cz);
      if (lower) {
        lower->calculateSunlight();
        lower->calculateBlockLight();
        lower->spreadLight();
        QueueMeshUpdate(lower, true);

        // Also update neighbors of this lower chunk, as light might have spread
        // to them
        int nDx[] = {-1, 1, 0, 0};
        int nDz[] = {0, 0, -1, 1};
        for (int i = 0; i < 4; ++i) {
          Chunk *n = getChunk(cx + nDx[i], cy - 1, cz + nDz[i]);
          if (n) {
            n->spreadLight();
          }
        }
      }
    }

    // Block Update Logic
    ChunkBlock b = getBlock(x, y, z);
    if (b.isActive()) {
      b.block->onPlace(*this, x, y, z);
    }

    // Notify neighbors
    int nOff[6][3] = {{0, 1, 0},  {0, -1, 0}, {1, 0, 0},
                      {-1, 0, 0}, {0, 0, 1},  {0, 0, -1}};
    for (int i = 0; i < 6; ++i) {
      int nx = x + nOff[i][0];
      int ny = y + nOff[i][1];
      int nz = x + nOff[i][2]; // BUG HERE: z + nOff[i][2]
      int nzCorrect = z + nOff[i][2];

      ChunkBlock nb = getBlock(nx, ny, nzCorrect);
      if (nb.isActive()) {
        nb.block->onNeighborChange(*this, nx, ny, nzCorrect, x, y, z);
      }
    }
  }
}

int World::render(Shader &shader, const glm::mat4 &viewProjection) {
  // Collect Visible Chunks under lock
  std::vector<Chunk *> visibleChunks;
  visibleChunks.reserve(chunks.size());

  {
    std::lock_guard<std::mutex> lock(worldMutex);

    // Frustum Culling
    auto planes = extractPlanes(viewProjection);

    for (auto &pair : chunks) {
      Chunk *c = pair.second.get();
      // Culling
      glm::vec3 min = glm::vec3(c->chunkPosition.x * CHUNK_SIZE,
                                c->chunkPosition.y * CHUNK_SIZE,
                                c->chunkPosition.z * CHUNK_SIZE);
      glm::vec3 max = min + glm::vec3(CHUNK_SIZE);

      bool visible = isAABBInFrustum(min, max, planes);

      if (c->meshDirty) {
        QueueMeshUpdate(c, visible); // Priority if visible
        c->meshDirty = false;
      }

      if (visible) {
        visibleChunks.push_back(c);
      }
    }
  }

  // Render outside lock
  int count = 0;

  // Pass 1: Opaque
  for (Chunk *c : visibleChunks) {
    if (c) {
      c->render(shader, viewProjection, 0); // Opaque
      count++;
    }
  }

  // Pass 2: Transparent
  // We should probably disable Face Culling for water?
  // Actually, water usually has backfacesCULLED if we only want surface.
  // If we want to see underside of water surface, we need double-sided.
  // Standard MC is single-sided (cull back).
  // But we might want to disable Depth Write for transparency?
  // For now, keep Depth Write ON to ensure surface looks solid enough.
  // Or arguably OFF to blend particles? Liquid usually ON.

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE); // Disable depth write for transparent pass so
                         // selection box shows

  for (Chunk *c : visibleChunks) {
    if (c) {
      c->render(shader, viewProjection, 1); // Transparent
    }
  }
  glDepthMask(GL_TRUE); // Restore depth write

  // Render Entities
  RenderSystem::Render(registry, *this, shader, viewProjection);

  return count;
}
// Removed getSuperChunk/getOrCreateSuperChunk definitions

bool World::raycast(glm::vec3 origin, glm::vec3 direction, float maxDist,
                    glm::ivec3 &outputPos, glm::ivec3 &outputPrePos) {
  // Naive: check all chunks, find closest hit
  bool hitAny = false;
  float closestDist = maxDist + 1.0f;
  glm::ivec3 bestPos;
  glm::ivec3 bestPrePos;

  std::lock_guard<std::mutex> lock(worldMutex);

  for (auto &pair : chunks) {
    Chunk *c = pair.second.get();
    glm::ivec3 hitPos, prePos;
    // Transform origin for chunk is handled inside Chunk::raycast now? No, I
    // updated it to do the subtraction. So we just pass global origin.

    if (c->raycast(origin, direction, maxDist, hitPos, prePos)) {
      // Calculate distance to hitPos (global)
      // hitPos is block coord (int). Center? Corner?
      // Chunk::raycast returns the block coords (chunk-local + chunkPos*size).
      // Wait, Chunk::raycast returns *chunk local* coords in my previous edit?
      // "outputPos = glm::ivec3(x, y, z);" where x,y,z are loop vars 0..15.
      // YES. I need to convert them to global coords here or in Chunk::raycast.
      // Let's ensure Chunk::raycast returns global coords or we convert them.

      // Checking Chunk.cpp edit:
      // "glm::vec3 localOrigin = origin - glm::vec3(chunkPosition *
      // CHUNK_SIZE);" "outputPos = glm::ivec3(x, y, z);" -> LOCAL

      // So we must convert back.
      glm::ivec3 globalHit = hitPos + c->chunkPosition * CHUNK_SIZE;
      glm::ivec3 globalPre = prePos + c->chunkPosition * CHUNK_SIZE;

      // Distance check
      // Use center of block for crude distance?
      // Or exact distance?
      // Chunk::raycast logic steps along the ray.
      // It doesn't return the exact float intersection.
      // However, since we step from origin, the 'd' (distance) is implicitly
      // roughly known. But Chunk::raycast doesn't return 'd'.

      // We can calculate distance from origin to center of block.
      glm::vec3 blockCenter = glm::vec3(globalHit) + glm::vec3(0.5f);
      float dist = glm::distance(origin, blockCenter);

      if (dist < closestDist) {
        closestDist = dist;
        bestPos = globalHit;
        bestPrePos = globalPre;
        hitAny = true;
      }
    }
  }

  if (hitAny) {
    outputPos = bestPos;
    outputPrePos = bestPrePos;
    return true;
  }
  return false;
}

size_t World::getChunkCount() const { return chunks.size(); }

void World::renderDebugBorders(Shader &shader,
                               const glm::mat4 &viewProjection) {
  static unsigned int borderVAO = 0;
  static unsigned int borderVBO = 0;

  if (borderVAO == 0) {
    // Pos(3) + Color(3)
    // Red color: 1,0,0
    float r = 1.0f, g = 0.0f, b = 0.0f;
    float v[] = {
        // Edge 1 (Bottom)
        0, 0, 0, r, g, b, 1, 0, 0, r, g, b, 1, 0, 0, r, g, b, 1, 0, 1, r, g, b,
        1, 0, 1, r, g, b, 0, 0, 1, r, g, b, 0, 0, 1, r, g, b, 0, 0, 0, r, g, b,
        // Edge 2 (Top)
        0, 1, 0, r, g, b, 1, 1, 0, r, g, b, 1, 1, 0, r, g, b, 1, 1, 1, r, g, b,
        1, 1, 1, r, g, b, 0, 1, 1, r, g, b, 0, 1, 1, r, g, b, 0, 1, 0, r, g, b,
        // Edge 3 (Pilars)
        0, 0, 0, r, g, b, 0, 1, 0, r, g, b, 1, 0, 0, r, g, b, 1, 1, 0, r, g, b,
        1, 0, 1, r, g, b, 1, 1, 1, r, g, b, 0, 0, 1, r, g, b, 0, 1, 1, r, g, b};

    glGenVertexArrays(1, &borderVAO);
    glGenBuffers(1, &borderVBO);
    glBindVertexArray(borderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, borderVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    // Pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);
    // Color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
  }

  glBindVertexArray(borderVAO);
  shader.use();
  shader.setBool("useTexture", false);

  // Disable generic attribute 3 if it was enabled, or just set its value?
  // Using glVertexAttrib3f sets the constant value for the attribute when the
  // array is disabled. Ensure array 3 is disabled.
  glDisableVertexAttribArray(3);
  // Set Lighting (Sky=1, Block=1, AO=0 -> Max Brightness)
  glVertexAttrib3f(3, 1.0f, 1.0f, 0.0f);

  // Frustum Culling (reuse helper)
  auto planes = extractPlanes(
      viewProjection); // Need to move extractPlanes to be accessible or copy it
  // It's defined as a static helper in this file? check line 441. Yes.

  std::lock_guard<std::mutex> lock(worldMutex);
  for (auto &pair : chunks) {
    Chunk *c = pair.second.get();
    glm::vec3 min = glm::vec3(c->chunkPosition.x * CHUNK_SIZE,
                              c->chunkPosition.y * CHUNK_SIZE,
                              c->chunkPosition.z * CHUNK_SIZE);
    glm::vec3 max = min + glm::vec3(CHUNK_SIZE);

    if (isAABBInFrustum(min, max, planes)) {
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, min);
      model = glm::scale(model, glm::vec3(CHUNK_SIZE));
      shader.setMat4("model", model);
      glDrawArrays(GL_LINES, 0, 24);
    }
  }
}
