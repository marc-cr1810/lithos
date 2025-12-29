#ifndef CHUNK_H
#define CHUNK_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <mutex>
#include <vector>

#include "../render/Shader.h"
#include "Block.h"

class World;

const int CHUNK_SIZE = 32;

#include <memory>

class Chunk : public std::enable_shared_from_this<Chunk> {
public:
  Chunk();
  ~Chunk();

  void setWorld(World *w) { world = w; }

  glm::ivec3 chunkPosition; // Chunk coordinates (e.g. 0,0,0)
  // Thread Safety
  // Thread Safety
  std::mutex chunkMutex;

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

  // Neighbor Pointers (Cached for lock-free access)
  // Indexes: 0=Front(Z+), 1=Back(Z-), 2=Left(X-), 3=Right(X+), 4=Top(Y+),
  // 5=Bottom(Y-)
  Chunk *neighbors[6];
  static const int DIR_FRONT = 0;
  static const int DIR_BACK = 1;
  static const int DIR_LEFT = 2;
  static const int DIR_RIGHT = 3;
  static const int DIR_TOP = 4;
  static const int DIR_BOTTOM = 5;

  void calculateSunlight(); // Step 1: Seed Skylight
  void calculateBlockLight();
  void spreadLight(); // Step 2: Spread light
  void render(Shader &shader, const glm::mat4 &viewProjection,
              int pass); // 0=Opaque, 1=Transparent
  void initGL();

  ChunkBlock getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, BlockType type);
  uint8_t getSkyLight(int x, int y, int z) const;
  uint8_t getBlockLight(int x, int y, int z) const;
  uint8_t getMetadata(int x, int y, int z) const;
  void setSkyLight(int x, int y, int z, uint8_t val);
  void setBlockLight(int x, int y, int z, uint8_t val);
  void setMetadata(int x, int y, int z, uint8_t val);

  // Returns true if a block was hit. outputPos is set to the block coordinates.
  // origin: World space origin using float
  // direction: Normalized direction
  // maxDist: Maximum distance to check
  bool raycast(glm::vec3 origin, glm::vec3 direction, float maxDist,
               glm::ivec3 &outputPos, glm::ivec3 &outputPrePos);

private:
  ChunkBlock blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
  World *world;
  unsigned int VAO, VBO, EBO;
  int vertexCount;
  int vertexCountTransparent;
  std::vector<float> transparentVertices; // CPU-side copy for sorting

public:
  void sortAndUploadTransparent(const glm::vec3 &cameraPos);

private:
  void addFace(std::vector<float> &vertices, int x, int y, int z, int faceDir,
               const Block *block, int width, int height, int aoBL, int aoBR,
               int aoTR, int aoTL, uint8_t metadata, float hBL, float hBR,
               float hTR, float hTL, int layer = 0);
  int vertexAO(bool side1, bool side2, bool corner);
};

#endif
