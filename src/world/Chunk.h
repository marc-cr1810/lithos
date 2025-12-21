#ifndef CHUNK_H
#define CHUNK_H

#include <vector>
#include <mutex>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Block.h"
#include "../render/Shader.h"

class World;

const int CHUNK_SIZE = 16;

class Chunk {
public:
    Chunk();
    ~Chunk();

    void setWorld(World* w) { world = w; }

    glm::ivec3 chunkPosition; // Chunk coordinates (e.g. 0,0,0)
    // Thread Safety
    // Thread Safety
    std::mutex chunkMutex;

    // Generates vertex data on CPU (Thread-Safe if mutex passed or blocks read-only)
    std::vector<float> generateGeometry(); 
    
    // Uploads data to GPU (Main Thread Only)
    void uploadMesh(const std::vector<float>& data);
    
    // Helper for Sync update (Generate + Upload)
    void updateMesh();

    bool meshDirty; // Flag for light updates

    // Neighbor Pointers (Cached for lock-free access)
    // Indexes: 0=Front(Z+), 1=Back(Z-), 2=Left(X-), 3=Right(X+), 4=Top(Y+), 5=Bottom(Y-)
    Chunk* neighbors[6]; 
    static const int DIR_FRONT = 0;
    static const int DIR_BACK = 1;
    static const int DIR_LEFT = 2;
    static const int DIR_RIGHT = 3;
    static const int DIR_TOP = 4;
    static const int DIR_BOTTOM = 5;

    void calculateSunlight(); // Step 1: Seed Skylight
    void calculateBlockLight();
    void spreadLight(); // Step 2: Spread light
    void render(Shader& shader, const glm::mat4& viewProjection);
    void initGL();

    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);
    uint8_t getSkyLight(int x, int y, int z) const;
    uint8_t getBlockLight(int x, int y, int z) const;
    void setSkyLight(int x, int y, int z, uint8_t val);
    void setBlockLight(int x, int y, int z, uint8_t val);
    
    // Returns true if a block was hit. outputPos is set to the block coordinates.
    // origin: World space origin using float
    // direction: Normalized direction
    // maxDist: Maximum distance to check
    bool raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, glm::ivec3& outputPos, glm::ivec3& outputPrePos);



private:
    Block blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    World* world;
    unsigned int VAO, VBO, EBO;
    int vertexCount;

    void addFace(std::vector<float>& vertices, int x, int y, int z, int faceDir, int blockType, int width, int height, int aoBL, int aoBR, int aoTR, int aoTL); 
    int vertexAO(bool side1, bool side2, bool corner);
};

#endif
