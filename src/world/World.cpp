#include "World.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "../ecs/Systems.h"
#include "../render/Shader.h"
#include "WorldGenRegion.h"
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

World::World(const WorldGenConfig &config, bool silent)
    : config(config), shutdown(false) {
  worldSeed = config.seed;

  // Start mesh worker threads
  int threadCount = std::thread::hardware_concurrency();
  if (threadCount < 2)
    threadCount = 2;
  for (int i = 0; i < threadCount; ++i) {
    meshThreads.emplace_back(&World::WorkerLoop, this);
  }

  // Start generation worker threads
  for (int i = 0; i < threadCount; ++i) {
    genThreads.emplace_back(&World::GenerationWorkerLoop, this);
  }

  m_Generator = std::make_unique<WorldGenerator>(config);
  m_Generator->GenerateFixedMaps();

  if (!silent) {
    LOG_WORLD_INFO("World initialized with Seed: {}", worldSeed);
  }
}

World::~World() {
  shutdown = true;

  // Join Gen Threads first (Stop Producers)
  genCondition.notify_all();
  for (auto &t : genThreads) {
    if (t.joinable())
      t.join();
  }

  // Join Mesh Threads (Stop Consumers)
  condition.notify_all();
  for (auto &t : meshThreads) {
    if (t.joinable())
      t.join();
  }
}

void World::WorkerLoop() {
  while (true) {
    std::shared_ptr<Chunk> c = nullptr;
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      condition.wait(lock, [this] {
        return !meshQueue.empty() || !meshQueueHighPrio.empty() || shutdown;
      });

      if (shutdown)
        break;

      // Check high-priority queue first (block breaks)
      if (!meshQueueHighPrio.empty()) {
        c = meshQueueHighPrio.front();
        meshQueueHighPrio.pop_front();
        meshSet.erase(c.get());
      } else if (!meshQueue.empty()) {
        c = meshQueue.front();
        meshQueue.pop_front();
        meshSet.erase(c.get());
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
  std::vector<std::tuple<std::shared_ptr<Chunk>, std::vector<float>, int>>
      toUpload;

  // Throttle uploads to prevent main thread stalls
  // 32^3 chunks are heavy (~100k+ vertices potentially), but we handle them
  // fast now. Increased from 4 to 64 to reduce pop-in.
  const int MAX_UPLOADS = 128; // Increased from 64

  {
    std::lock_guard<std::mutex> lock(uploadMutex);
    if (!uploadQueue.empty()) {
      int count = std::min((int)uploadQueue.size(), MAX_UPLOADS);

      // Move 'count' items to local list
      toUpload.reserve(count);
      for (int i = 0; i < count; ++i) {
        toUpload.push_back(std::move(uploadQueue[i]));
      }

      // Remove from queue (erasing from front is O(N) for vector but N is small
      // here)
      uploadQueue.erase(uploadQueue.begin(), uploadQueue.begin() + count);
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

void World::QueueMeshUpdate(std::shared_ptr<Chunk> ptr, bool priority) {
  if (ptr) {
    Chunk *c = ptr.get();
    std::lock_guard<std::mutex> lock(queueMutex);
    if (meshSet.find(c) == meshSet.end()) {
      // Add to appropriate queue based on priority
      if (priority)
        meshQueueHighPrio.push_back(ptr);
      else
        meshQueue.push_back(ptr);
      meshSet.insert(c);
      condition.notify_one();
    }
  }
}

void World::GenerationWorkerLoop() {
  // Use thread-local generator to avoid race conditions in noise buffers
  WorldGenerator generator(config);
  generator.GenerateFixedMaps();

  while (true) {
    std::tuple<int, int, int> coord;
    {
      std::unique_lock<std::mutex> lock(genMutex);
      genCondition.wait(lock, [this] { return !genQueue.empty() || shutdown; });

      if (shutdown)
        break;

      if (!genQueue.empty()) {
        coord = genQueue.top().coord;
        genQueue.pop();
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

    // 1. Ensure Column Exists
    ChunkColumn *column = nullptr;

    // Optimization: Try to find existing column first
    {
      std::lock_guard<std::mutex> lock(columnMutex);
      auto it = columns.find({x, z});
      if (it != columns.end()) {
        column = it->second.get();
      }
    }

    // If not found, generate it
    if (!column) {
      auto newCol = std::make_unique<ChunkColumn>();
      m_Generator->GenerateColumn(*newCol, x, z);

      std::lock_guard<std::mutex> lock(columnMutex);
      // Insert or get existing (if race happened)
      auto result = columns.emplace(std::make_pair(x, z), std::move(newCol));
      column = result.first->second.get();
    }

    // 2. Create Chunk
    auto newChunk = std::make_shared<Chunk>();
    newChunk->chunkPosition = glm::ivec3(x, y, z);
    newChunk->setWorld(this);

    // 3. Generate Blocks using Column
    generator.GenerateChunk(*newChunk, *column);

    // 3. Add to World (This links neighbors)
    {
      std::lock_guard<std::mutex> lock(worldMutex);
      chunks[coord] = std::move(newChunk);
      std::shared_ptr<Chunk> c = chunks[coord];

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
          std::shared_ptr<Chunk> n = it->second;
          c->neighbors[dirs[i]] = n;
          n->neighbors[opps[i]] = c;
        }
      }
    }

    std::shared_ptr<Chunk> c = getChunk(x, y, z); // Safe retrieval
    if (c) {
      // 4. Calculate Light
      c->calculateSunlight();
      c->calculateBlockLight();

      // 5. Spread Light (Might need neighboring chunks)
      c->spreadLight();

      // Queue mesh update
      QueueMeshUpdate(c, false);

      // Update neighbors
      int dirs_indices[] = {
          Chunk::DIR_LEFT, Chunk::DIR_RIGHT, Chunk::DIR_FRONT,
          Chunk::DIR_BACK, Chunk::DIR_TOP,   Chunk::DIR_BOTTOM,
      };
      for (int i = 0; i < 6; i++) {
        auto n = c->getNeighbor(dirs_indices[i]);
        if (n) {
          // re-calculate sunlight because we might have just blocked the sky.
          if (dirs_indices[i] == Chunk::DIR_BOTTOM) {
            n->calculateSunlight();
            n->calculateBlockLight();
          }

          n->spreadLight();
          QueueMeshUpdate(n, false);
        }
      }

      // ===== DECORATION PHASE =====
      // Decoration follows a 3x3 rule. When a column is generated, it might
      // satisfy the 3x3 requirement for itself or any of its 8 neighbors.
      auto isColumnReady = [this](int colX, int colZ) -> ChunkColumn * {
        std::lock_guard<std::mutex> lock(columnMutex);
        auto it = columns.find({colX, colZ});
        if (it == columns.end() || it->second->decorated)
          return nullptr;

        // Check 3x3 neighborhood for ALL 8 chunks in each column
        // We need the full 3x3x8 block context to prevent feature cut-offs
        for (int dx = -1; dx <= 1; dx++) {
          for (int dz = -1; dz <= 1; dz++) {
            // 1. Column must exist
            if (columns.find({colX + dx, colZ + dz}) == columns.end()) {
              return nullptr;
            }
            // 2. All vertical chunks must exist in chunks map
            for (int y = 0; y < 8; y++) {
              // We don't hold the lock for the whole 3x3x8 search to avoid
              // deadlock, but we must check chunks map safely. Actually
              // getChunk uses worldMutex, but we are inside
              // GenerationWorkerLoop which might be called while holding other
              // locks. However, getChunk is generally safe to call.
              if (!this->getChunk(colX + dx, y, colZ + dz)) {
                return nullptr;
              }
            }
          }
        }
        return it->second.get();
      };

      // Check center and 8 neighbors
      for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
          int targetX = x + dx;
          int targetZ = z + dz;

          ChunkColumn *target = isColumnReady(targetX, targetZ);
          if (target) {
            // Mark as decorated FIRST under lock if possible (to avoid race)
            {
              std::lock_guard<std::mutex> lock(columnMutex);
              if (target->decorated)
                continue;
              target->decorated = true;
            }

            // Create WorldGenRegion and decorate
            WorldGenRegion region(this, targetX, targetZ);
            generator.Decorate(region, *target);

            // 1. Recalculate Lighting synchronously Top-Down TO prevent cutoffs
            for (int Lx = -1; Lx <= 1; Lx++) {
              for (int Lz = -1; Lz <= 1; Lz++) {
                for (int Ly = 7; Ly >= 0; Ly--) {
                  std::shared_ptr<Chunk> c =
                      getChunk(targetX + Lx, Ly, targetZ + Lz);
                  if (c && c->needsLightingUpdate) {
                    c->calculateSunlight();
                    c->calculateBlockLight();
                    c->spreadLight();
                    c->needsLightingUpdate = false;
                  }
                }
              }
            }

            // 2. Queue for meshing (now that lighting is valid)
            for (int Lx = -1; Lx <= 1; Lx++) {
              for (int Lz = -1; Lz <= 1; Lz++) {
                for (int Ly = 0; Ly < 8; Ly++) {
                  std::shared_ptr<Chunk> c =
                      getChunk(targetX + Lx, Ly, targetZ + Lz);
                  if (c && c->meshDirty) { // setBlock sets meshDirty=true
                    QueueMeshUpdate(c, false);
                  }
                }
              }
            }
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

          // Dynamic height limit based on config
          int chunksY = config.worldHeight / CHUNK_SIZE;

          // Column Visibility Check
          // Check if the entire column AABB is in frustum
          bool columnVisible = false;
          glm::vec3 colMin(x * CHUNK_SIZE, 0, z * CHUNK_SIZE);
          glm::vec3 colMax =
              colMin + glm::vec3(CHUNK_SIZE, chunksY * CHUNK_SIZE, CHUNK_SIZE);

          if (isAABBInFrustum(colMin, colMax, planes)) {
            columnVisible = true;
          }

          // Base Priority for Column (Purely Distance based)
          float basePriority = 10000.0f / (distance + 0.1f);

          if (columnVisible) {
            basePriority *= 2.0f; // Boost visible columns
          }

          if (distance < 3.0f) {
            basePriority *= 5.0f; // Urgent boost for spawn/player range
          }

          for (int y = 0; y < chunksY; ++y) {
            float priority = basePriority;

            // Minor adjustments to order within the column (Surface first, then
            // others) This ensures they are grouped but critical ones processed
            // first
            int distToSurface = std::abs(y - 2); // Assume Y=2 is center
            priority -= (float)distToSurface *
                        0.1f; // Tiny penalty for being away from center

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
  // Process chunks from queue
  // checkingMapLimit: fast map lookups to skip already loaded chunks
  // schedulingLimit: actual heavy generation tasks to send to thread pool
  const int MAX_CHUNKS_CHECKED = 20000;
  const int MAX_TASKS_SCHEDULED = 256; // Increased from 64

  int chunksChecked = 0;
  int tasksScheduled = 0;

  std::vector<std::tuple<int, int, int>> urgentBatch;

  while (chunksChecked < MAX_CHUNKS_CHECKED &&
         tasksScheduled < MAX_TASKS_SCHEDULED &&
         queueIndex < loadQueue.size()) {
    const auto &req = loadQueue[queueIndex];
    queueIndex++;
    chunksChecked++;

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

        // Just push to priority queue, it handles the sorting!
        genQueue.push({key, req.priority});

        genCondition.notify_one();

        // This counts as a scheduled task
        tasksScheduled++;
      }
    }
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
    std::shared_ptr<Chunk> chunkToUnload = nullptr;
    {
      std::lock_guard<std::mutex> lock(worldMutex);
      auto it = chunks.find(key);
      if (it != chunks.end()) {
        chunkToUnload = it->second;
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
        if (auto neighbor = chunkToUnload->getNeighbor(dirs[i])) {
          neighbor->neighbors[opps[i]].reset();
          chunkToUnload->neighbors[dirs[i]].reset();
        }
      }

      // Remove from mesh queue if present
      {
        std::lock_guard<std::mutex> lock(queueMutex);
        meshSet.erase(chunkToUnload.get());
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
    auto newChunk = std::make_shared<Chunk>();
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
    int opps[] = {Chunk::DIR_BACK, Chunk::DIR_FRONT,  Chunk::DIR_RIGHT,
                  Chunk::DIR_LEFT, Chunk::DIR_BOTTOM, Chunk::DIR_TOP};

    for (int i = 0; i < 6; ++i) {
      auto it = chunks.find(std::make_tuple(x + dx[i], y + dy[i], z + dz[i]));
      if (it != chunks.end()) {
        std::shared_ptr<Chunk> n = it->second;
        c->neighbors[dirs[i]] = n;
        n->neighbors[opps[i]] = c->shared_from_this();
      }
    }

    // Not generating, just adding empty struct for now?
    // Actually addChunk is usually called by generator?
    // Wait, typical usage: addChunk -> then generator populates it?
    // In this file, addChunk is expected to create a fresh one.
  }
}

void World::insertChunk(std::shared_ptr<Chunk> chunk) {
  if (!chunk)
    return;
  std::unique_lock<std::mutex> lock(worldMutex);
  auto key = std::make_tuple(chunk->chunkPosition.x, chunk->chunkPosition.y,
                             chunk->chunkPosition.z);

  // Replace or insert
  chunks[key] = chunk;
  chunk->setWorld(this);

  // Link Neighbors
  int dx[] = {0, 0, -1, 1, 0, 0};
  int dy[] = {0, 0, 0, 0, 1, -1};
  int dz[] = {1, -1, 0, 0, 0, 0};
  int dirs[] = {Chunk::DIR_FRONT, Chunk::DIR_BACK, Chunk::DIR_LEFT,
                Chunk::DIR_RIGHT, Chunk::DIR_TOP,  Chunk::DIR_BOTTOM};
  int opps[] = {Chunk::DIR_BACK, Chunk::DIR_FRONT,  Chunk::DIR_RIGHT,
                Chunk::DIR_LEFT, Chunk::DIR_BOTTOM, Chunk::DIR_TOP};

  for (int i = 0; i < 6; ++i) {
    auto it = chunks.find(std::make_tuple(chunk->chunkPosition.x + dx[i],
                                          chunk->chunkPosition.y + dy[i],
                                          chunk->chunkPosition.z + dz[i]));
    if (it != chunks.end()) {
      std::shared_ptr<Chunk> n = it->second;
      chunk->neighbors[dirs[i]] = n;
      n->neighbors[opps[i]] = chunk;
    }
  }

  // Queue for mesh update immediately so it shows up
  QueueMeshUpdate(chunk, true);
}

std::shared_ptr<Chunk> World::getChunk(int chunkX, int chunkY, int chunkZ) {
  std::lock_guard<std::mutex> lock(worldMutex);
  auto key = std::make_tuple(chunkX, chunkY, chunkZ);
  auto it = chunks.find(key);
  if (it != chunks.end())
    return it->second;
  return nullptr;
}

void World::getNeighbors(int cx, int cy, int cz, Chunk *chunks[3][3]) {
  std::lock_guard<std::mutex> lock(worldMutex);
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dz = -1; dz <= 1; ++dz) {
      if (dx == 0 && dz == 0)
        continue;
      auto key = std::make_tuple(cx + dx, cy, cz + dz);
      auto it = this->chunks.find(key); // Access member 'chunks'
      if (it != this->chunks.end()) {
        chunks[dx + 1][dz + 1] = it->second.get();
      } else {
        chunks[dx + 1][dz + 1] = nullptr;
      }
    }
  }
}

std::shared_ptr<const Chunk> World::getChunk(int chunkX, int chunkY,
                                             int chunkZ) const {
  std::lock_guard<std::mutex> lock(worldMutex);
  auto key = std::make_tuple(chunkX, chunkY, chunkZ);
  auto it = chunks.find(key);
  if (it != chunks.end())
    return it->second;
  return nullptr;
}

// Helper for floor division (explicitly defined to avoid ambiguity)
inline int floorDiv(int a, int b) {
  return (a >= 0) ? (a / b) : ((a - b + 1) / b);
}

ChunkBlock World::getBlock(int x, int y, int z) const {
  // Explicit chunk coordinate calculation handles negative coordinates
  // correctly
  int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);

  std::shared_ptr<const Chunk> c = getChunk(cx, cy, cz);
  if (!c) {
    // Return Air if chunk is not loaded
    return {BlockRegistry::getInstance().getBlock(AIR), 15, 0};
  }

  // Local coordinates (robust modulo)
  int lx = x % CHUNK_SIZE;
  if (lx < 0)
    lx += CHUNK_SIZE;

  int ly = y % CHUNK_SIZE;
  if (ly < 0)
    ly += CHUNK_SIZE;

  int lz = z % CHUNK_SIZE;
  if (lz < 0)
    lz += CHUNK_SIZE;

  // Utilize the chunk's getBlock which has its own bounds checks
  return c->getBlock(lx, ly, lz);
}

int World::getHeight(int x, int z) const {
  int cx = floorDiv(x, CHUNK_SIZE);
  int cz = floorDiv(z, CHUNK_SIZE);

  std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(columnMutex));

  auto it = columns.find({cx, cz});
  if (it != columns.end()) {
    int lx = x - cx * CHUNK_SIZE;
    int lz = z - cz * CHUNK_SIZE;
    return it->second->getHeight(lx, lz);
  }
  return 0; // Default
}

uint8_t World::getSkyLight(int x, int y, int z) {
  int cx = floorDiv(x, CHUNK_SIZE);
  int cy = floorDiv(y, CHUNK_SIZE);
  int cz = floorDiv(z, CHUNK_SIZE);

  std::shared_ptr<Chunk> c = getChunk(cx, cy, cz);
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

  std::shared_ptr<Chunk> c = getChunk(cx, cy, cz);
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

  std::shared_ptr<Chunk> c = getChunk(cx, cy, cz);
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

  std::shared_ptr<Chunk> c = getChunk(cx, cy, cz);
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

  std::shared_ptr<Chunk> c = getChunk(cx, cy, cz);
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
      std::shared_ptr<Chunk> n =
          getChunk(cx + nDx[i], cy + nDy[i], cz + nDz[i]);
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
      std::shared_ptr<Chunk> lower = getChunk(cx, cy - 1, cz);
      if (lower) {
        lower->calculateSunlight();
        lower->calculateBlockLight();
        lower->spreadLight();
        QueueMeshUpdate(lower, true);
      }
    } else {
      // Block placed - might shadow lower chunk
      std::shared_ptr<Chunk> lower = getChunk(cx, cy - 1, cz);
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
          std::shared_ptr<Chunk> n = getChunk(cx + nDx[i], cy - 1, cz + nDz[i]);
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
      int nz = z + nOff[i][2];

      ChunkBlock nb = getBlock(nx, ny, nz);
      if (nb.isActive()) {
        nb.block->onNeighborChange(*this, nx, ny, nz, x, y, z);
      }
    }
  }
}

int World::render(Shader &shader, const glm::mat4 &viewProjection,
                  const glm::vec3 &cameraPos, int renderDistInput) {
  // Collect Visible Chunks under lock
  std::vector<std::shared_ptr<Chunk>> visibleChunks;
  visibleChunks.reserve(chunks.size());

  {
    std::lock_guard<std::mutex> lock(worldMutex);

    // Frustum Culling
    auto planes = extractPlanes(viewProjection);

    // Optimization: Grid Iteration + Column Culling
    int cx = (int)floor(cameraPos.x / CHUNK_SIZE);
    int cz = (int)floor(cameraPos.z / CHUNK_SIZE);
    int renderDist = renderDistInput + 1; // +1 buffer

    // Vertical range usually 0..7 for 256 height
    int minY = 0;
    int maxY = (config.worldHeight + CHUNK_SIZE - 1) / CHUNK_SIZE;

    {
      PROFILE_SCOPE("Culling & Vis List");
      for (int x = cx - renderDist; x <= cx + renderDist; ++x) {
        for (int z = cz - renderDist; z <= cz + renderDist; ++z) {
          // 1. Cull Column First
          glm::vec3 colMin(x * CHUNK_SIZE, 0, z * CHUNK_SIZE);
          glm::vec3 colMax(colMin.x + CHUNK_SIZE, config.worldHeight,
                           colMin.z + CHUNK_SIZE);

          if (!isAABBInFrustum(colMin, colMax, planes)) {
            continue; // Skip whole column
          }

          // 2. Iterate Chunks in Column
          for (int y = minY; y < maxY; ++y) {
            auto it = chunks.find(std::make_tuple(x, y, z));
            if (it == chunks.end())
              continue;

            std::shared_ptr<Chunk> c = it->second;

            glm::vec3 min(x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE);
            glm::vec3 max = min + glm::vec3(CHUNK_SIZE);

            bool visible = isAABBInFrustum(min, max, planes);

            if (c->meshDirty) {
              QueueMeshUpdate(c, visible);
              c->meshDirty = false;
            }

            if (visible) {
              visibleChunks.push_back(c);
            }
          }
        }
      }
    }
  }

  // Render outside lock
  int count = 0;

  // Pass 1: Opaque
  // Optimization: Sort Front-to-Back for opaque (minimizes overdraw)
  // Sort by distance (front to back for opaque? actually opaque doesn't
  // strictly need it but helps early Z. Transparent MUST be back to front.)
  // Let's sort front-to-back for opaque optimization
  {
    PROFILE_SCOPE("Sort Chunks");
    std::sort(visibleChunks.begin(), visibleChunks.end(),
              [&cameraPos](const std::shared_ptr<Chunk> &a,
                           const std::shared_ptr<Chunk> &b) {
                glm::vec3 posA = glm::vec3(a->chunkPosition * CHUNK_SIZE) +
                                 glm::vec3(CHUNK_SIZE / 2.0f);
                glm::vec3 posB = glm::vec3(b->chunkPosition * CHUNK_SIZE) +
                                 glm::vec3(CHUNK_SIZE / 2.0f);
                float distA = glm::dot(posA - cameraPos, posA - cameraPos);
                float distB = glm::dot(posB - cameraPos, posB - cameraPos);
                return distA < distB;
              });
  }

  // Render Opaque
  // Shader setup...
  shader.use();
  // Argument name is 'viewProjection'.
  // We should assume shader needs VP.
  // Actually shader code likely uses "projection" and "view" separate or "vp"?
  // Let's rely on main setup or uniform name.
  // Actually: src/main.cpp passes "cullMatrix" (P*V) as 2nd arg.
  // Chunk::render just uses it? No, checking Chunk::render...
  // Chunk::render doesn't use it. It sets "model".
  // World::render has shader reference.
  // Wait, looking at World.cpp original code...

  // Original code didn't set "view" or "projection" here?
  // Ah, lines 926+ in original file might set them?
  // I replaced loop.
  // Let's check where I am editing.

  // Render Opaque
  {
    PROFILE_SCOPE("Render Opaque");
    for (const auto &c : visibleChunks) {
      if (c) {
        c->render(shader, viewProjection, 0); // Opaque
        count++;
      }
    }
  }

  // Pass 2: Transparent
  // SORT Back-to-Front for correct transparency blending
  // We reuse the list but reverse it?
  // Actually, we just iterate backwards if we sorted Front-to-Back above.
  // Or just re-sort/reverse.
  // Iterating backwards is cheapest.

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE); // Disable depth write for transparent pass

  // Iterate backwards (Far to Near)
  {
    PROFILE_SCOPE("Transp Sort");
    for (auto it = visibleChunks.rbegin(); it != visibleChunks.rend(); ++it) {
      const auto &c = *it;
      if (c) {
        c->sortAndUploadTransparent(cameraPos);
      }
    }
  }

  {
    PROFILE_SCOPE("Transp Draw");
    for (auto it = visibleChunks.rbegin(); it != visibleChunks.rend(); ++it) {
      const auto &c = *it;
      if (c) {
        c->render(shader, viewProjection, 1); // Transparent
      }
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

    // Optimization: Check distance to chunk center first
    glm::vec3 chunkCenter =
        glm::vec3(c->chunkPosition * CHUNK_SIZE) + glm::vec3(CHUNK_SIZE / 2.0f);
    glm::vec3 diff = origin - chunkCenter;
    float distToCenterSq = glm::dot(diff, diff); // Squared distance
    // Radius of chunk is approx sqrt(3)*CHUNK_SIZE/2.
    // Safe cull radius: maxDist + ChunkRadius
    float cullDist = maxDist + (CHUNK_SIZE * 0.866f) + 2.0f;

    if (distToCenterSq > cullDist * cullDist)
      continue;

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
      glm::ivec3 globalPre =
          prePos + c->chunkPosition * CHUNK_SIZE; // Use local prePos

      // Distance check
      // Use center of block for crude distance?
      // Or exact distance?
      // Chunk::raycast logic steps along the ray.
      // It doesn't return the exact float intersection.
      // However, since we step from origin, the 'd' (distance) is implicitly
      // roughly known. But Chunk::raycast doesn't return 'd'.

      // We can calculate distance from origin to center of block.
      glm::vec3 blockCenter =
          glm::vec3(globalHit) + glm::vec3(0.5f); // +0.5 to center
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
