#ifndef WORLD_H
#define WORLD_H

#include <unordered_map>
#include <tuple>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <condition_variable>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Chunk.h"
#include "Block.h"
#include "Chunk.h"
#include "Block.h"
#include "../render/Shader.h"
#include "../render/Shader.h"

// Hash function for std::tuple
// Hash function for std::tuple
struct key_hash
{
    std::size_t operator()(const std::tuple<int, int, int>& k) const
    {
        std::size_t seed = 0;
        auto hash_combine = [&seed](int v) {
            seed ^= std::hash<int>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        hash_combine(std::get<0>(k));
        hash_combine(std::get<1>(k));
        hash_combine(std::get<2>(k));
        return seed;
    }
};

class World {
public:
    World();
    ~World();
    
    void addChunk(int x, int y, int z); // Chunk coords
    Chunk* getChunk(int chunkX, int chunkY, int chunkZ);
    const Chunk* getChunk(int chunkX, int chunkY, int chunkZ) const;

    // Global world coordinates
    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);

    // Returns number of chunks rendered
    int render(Shader& shader, const glm::mat4& viewProjection);
    
    // Raycast against all chunks (or optimization)
    // Returns true and fills info if hit
    bool raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, glm::ivec3& outputPos, glm::ivec3& outputPrePos);

    uint8_t getSkyLight(int x, int y, int z);
    uint8_t getBlockLight(int x, int y, int z);

    // Threading
    void Update(); // Main Thread
    void QueueMeshUpdate(Chunk* c, bool priority = false);
    


    // Friend for generator if needed, or public method
    // Generator will just use addChunk/getChunk.

private:
    std::unordered_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>, key_hash> chunks;
    mutable std::mutex worldMutex; 

    // Worker Thread
    std::vector<std::thread> meshThreads;
    bool shutdown;
    std::condition_variable condition;
    
    std::mutex queueMutex;
    std::deque<Chunk*> meshQueue;
    std::unordered_set<Chunk*> meshSet; // For deduplication
    
    std::mutex uploadMutex;
    std::vector<std::tuple<Chunk*, std::vector<float>, int>> uploadQueue;
    
    void WorkerLoop();

    // Generation Thread Pool
    std::mutex genMutex;
    std::condition_variable genCondition;
    std::vector<std::thread> genThreads;
    std::deque<std::tuple<int, int, int>> genQueue;
    std::unordered_set<std::tuple<int, int, int>, key_hash> generatingChunks;
    
    void GenerationWorkerLoop();

public:
    void loadChunks(const glm::vec3& playerPos, int renderDistance, const glm::mat4& viewProjection);
    size_t getChunkCount() const;
    void renderDebugBorders(Shader& shader, const glm::mat4& viewProjection);
};

#endif
