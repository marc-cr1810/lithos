#ifndef CHUNK_H
#define CHUNK_H

#include <vector>
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
    bool meshDirty;

    void updateMesh(); // Generate mesh data
    void calculateSunlight(); // Step 1: Seed Skylight
    void calculateBlockLight();
    void spreadLight(); // Step 2: Spread light
    void render(Shader& shader);

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
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int indexCount;

    void addFace(int x, int y, int z, int faceDir, int blockType); // 0:front, 1:back, 2:left, 3:right, 4:top, 5:bottom
    int vertexAO(bool side1, bool side2, bool corner);
};

#endif
