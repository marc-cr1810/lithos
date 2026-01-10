#ifndef CHUNK_H
#define CHUNK_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <mutex>
#include <random>
#include <vector>

#include "../render/Shader.h"
#include "Block.h"

class World;
class CaveGenerator;

const int CHUNK_SIZE = 32;

#include <memory>

class Chunk : public std::enable_shared_from_this<Chunk> {
  friend class WorldGenerator;
  friend class CaveGenerator;

public:
  Chunk();
  ~Chunk();

  void setWorld(World *w);
  World *getWorld() const { return world; }

  glm::ivec3 chunkPosition; // Chunk coordinates (e.g. 0,0,0)
  // Thread Safety
  // Thread Safety
  std::mutex chunkMutex;

  glm::vec3 getCenter() const {
    return glm::vec3(chunkPosition * CHUNK_SIZE) + glm::vec3(CHUNK_SIZE / 2.0f);
  }

  // Generates vertex data on CPU (Thread-Safe if mutex passed or blocks
  // read-only)
  std::vector<float> generateGeometry(int &outOpaqueCount);

  // Uploads data to GPU (Main Thread Only)
  void uploadMesh(const std::vector<float> &data, int opaqueCount);

  // Helper for Sync update (Generate + Upload)
  void updateMesh();

  bool meshDirty; // Flag for light updates
  bool needsLightingUpdate =
      false; // Flag to recalculate lighting before mesh gen

  bool isAllAir = false;    // All blocks are AIR
  bool isAllOpaque = false; // All blocks are solid/opaque
  bool isSealed = false;    // Completely buried (opaque + neighbors opaque)

  void updateSealedStatus();

  // Neighbor Pointers (Weakly linked to prevent access to unloaded chunks)
  // Indexes: 0=Front(Z+), 1=Back(Z-), 2=Left(X-), 3=Right(X+), 4=Top(Y+),
  // 5=Bottom(Y-)
  std::weak_ptr<Chunk> neighbors[6];
  static const int DIR_FRONT = 0;
  static const int DIR_BACK = 1;
  static const int DIR_LEFT = 2;
  static const int DIR_RIGHT = 3;
  static const int DIR_TOP = 4;
  static const int DIR_BOTTOM = 5;

  std::shared_ptr<Chunk> getNeighbor(int dir) const {
    if (dir < 0 || dir >= 6)
      return nullptr;
    return neighbors[dir].lock();
  }

  void calculateSunlight(); // Step 1: Seed Skylight
  void calculateBlockLight();
  void spreadLight(); // Step 2: Spread light

  // Random Ticking
  void processRandomTicks(int tickCount, std::mt19937 &rng);

  void render(Shader &shader, const glm::mat4 &viewProjection,
              int pass); // 0=Opaque, 1=Transparent, 2=Liquid
  void initGL();

  ChunkBlock getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, block_id type);
  void setBlockNoMeshUpdate(int x, int y, int z, block_id type);
  uint8_t getSkyLight(int x, int y, int z) const;
  uint8_t getBlockLight(int x, int y, int z) const;
  uint8_t getMetadata(int x, int y, int z) const;
  void setSkyLight(int x, int y, int z, uint8_t val);
  void setBlockLight(int x, int y, int z, uint8_t val);
  void setMetadata(int x, int y, int z, uint8_t val);

  // Climate Data (0-255 mapped from 0.0-1.0)
  void setClimate(int x, int z, float temp, float humid) {
    if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
      temperatureMap[x][z] =
          static_cast<uint8_t>(glm::clamp(temp, 0.0f, 1.0f) * 255.0f);
      humidityMap[x][z] =
          static_cast<uint8_t>(glm::clamp(humid, 0.0f, 1.0f) * 255.0f);
    }
  }

  void getClimate(int x, int z, float &temp, float &humid) const {
    if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
      temp = temperatureMap[x][z] / 255.0f;
      humid = humidityMap[x][z] / 255.0f;
    } else {
      temp = 0.5f;
      humid = 0.5f;
    }
  }

private:
  uint8_t temperatureMap[CHUNK_SIZE][CHUNK_SIZE];
  uint8_t humidityMap[CHUNK_SIZE][CHUNK_SIZE];

public:
  // Returns true if a block was hit. outputPos is set to the block coordinates.
  // origin: World space origin using float
  // direction: Normalized direction
  // maxDist: Maximum distance to check
  bool raycast(glm::vec3 origin, glm::vec3 direction, float maxDist,
               glm::ivec3 &outputPos, glm::ivec3 &outputPrePos,
               int *outputFace = nullptr);

private:
  World *world;
  ChunkBlock blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
  unsigned int VAO, VBO, EBO;
  unsigned int liquidVAO, liquidVBO, liquidEBO; // Liquid buffers
  int vertexCount;
  int vertexCountTransparent;
  std::vector<float> transparentVertices; // CPU-side copy for sorting
  std::vector<float> liquidVertices;      // CPU-side liquid mesh
  int liquidVertexCount;
  glm::vec3 m_lastSortCameraPos = glm::vec3(-99999.0f); // Initialize far away

public:
  void sortAndUploadTransparent(const glm::vec3 &cameraPos);

private:
  void addFace(std::vector<float> &vertices, int x, int y, int z, int faceDir,
               const Block *block, int width, int height, int aoBL, int aoBR,
               int aoTR, int aoTL, uint8_t metadata, float hBL, float hBR,
               float hTR, float hTL, bool isInternal = false);

  // Liquid Helpers
  void pushLiquidVert(float x, float y, float z, float u, float v, float r,
                      float g, float b, float a, float sun, float blockLight,
                      float ao, float flowX, float flowZ, float flags);

  void addLiquidFace(int x, int y, int z, int faceDir, const Block *block,
                     int aoBL, int aoBR, int aoTR, int aoTL, float hBL,
                     float hBR, float hTR, float hTL, uint8_t metadata);

  std::pair<float, float> getLiquidFlowVector(int x, int y, int z,
                                              uint8_t meta);

  int vertexAO(bool side1, bool side2, bool corner);
};

#endif
