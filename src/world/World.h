#ifndef WORLD_H
#define WORLD_H

#include <map>
#include <tuple>
#include <vector>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Chunk.h"
#include "Block.h"
#include "../render/Shader.h"

class World {
public:
    World();
    
    void addChunk(int x, int y, int z); // Chunk coords
    Chunk* getChunk(int chunkX, int chunkY, int chunkZ);
    const Chunk* getChunk(int chunkX, int chunkY, int chunkZ) const;

    // Global world coordinates
    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);

    void render(Shader& shader);
    
    // Raycast against all chunks (or optimization)
    // Returns true and fills info if hit
    bool raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, glm::ivec3& outputPos, glm::ivec3& outputPrePos);

    uint8_t getLight(int x, int y, int z);

    // Friend for generator if needed, or public method
    // Generator will just use addChunk/getChunk.

private:
    std::map<std::tuple<int, int, int>, std::unique_ptr<Chunk>> chunks;
};

#endif
