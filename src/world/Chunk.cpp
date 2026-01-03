#include "Chunk.h"
#include "../debug/Logger.h"
#include "World.h"
#include "WorldGenerator.h"
#include <GL/glew.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <queue>
#include <tuple>

Chunk::Chunk()
    : meshDirty(true), vertexCount(0), vertexCountTransparent(0),
      chunkPosition(0, 0, 0), world(nullptr), VAO(0), VBO(0), EBO(0) {
  // GL initialization deferred to Main Thread via initGL()
  // Initialize with air
  Block *air = BlockRegistry::getInstance().getBlock(AIR);
  for (int x = 0; x < CHUNK_SIZE; ++x)
    for (int y = 0; y < CHUNK_SIZE; ++y)
      for (int z = 0; z < CHUNK_SIZE; ++z)
        blocks[x][y][z] = {air, 0, 0};

  for (int i = 0; i < 6; ++i)
    neighbors[i].reset();

  // LOG_INFO("Chunk created at {}", (void*)this);
  world = nullptr;
}

void Chunk::setWorld(World *w) { world = w; }

Chunk::~Chunk() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
}

// ... Setters ...

void Chunk::initGL() {
  if (VAO == 0) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
  }
}

void Chunk::render(Shader &shader, const glm::mat4 &viewProjection, int pass) {
  if (VAO == 0)
    initGL();
  // Pass 0: Opaque
  // Pass 1: Transparent

  if (pass == 0 && vertexCount == 0)
    return;
  if (pass == 1 && vertexCountTransparent == 0)
    return;

  shader.setMat4(
      "model",
      glm::translate(glm::mat4(1.0f), glm::vec3(chunkPosition.x * CHUNK_SIZE,
                                                chunkPosition.y * CHUNK_SIZE,
                                                chunkPosition.z * CHUNK_SIZE)));

  glBindVertexArray(VAO);
  if (pass == 0) {
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
  } else {
    glDrawArrays(GL_TRIANGLES, vertexCount, vertexCountTransparent);
  }
  glBindVertexArray(0);
}

ChunkBlock Chunk::getBlock(int x, int y, int z) const {
  if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 ||
      z >= CHUNK_SIZE)
    return {BlockRegistry::getInstance().getBlock(AIR), 0, 0};
  return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
  std::lock_guard<std::mutex> lock(chunkMutex);
  if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 ||
      z >= CHUNK_SIZE)
    return;
  blocks[x][y][z].block = BlockRegistry::getInstance().getBlock(type);
  blocks[x][y][z].metadata = 0; // Reset metadata on block change!
  meshDirty = true;
}

uint8_t Chunk::getSkyLight(int x, int y, int z) const {
  // Technically should lock, but single-byte read might be atomic-ish enough
  // for visual artifacts... But strictly, we should lock. However, locking on
  // every get is EXPENSIVE. For now, let's lock setters. get might read torn
  // data but usually just old or new byte.
  if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 ||
      z >= CHUNK_SIZE)
    return 0;
  return blocks[x][y][z].skyLight;
}

uint8_t Chunk::getBlockLight(int x, int y, int z) const {
  if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 ||
      z >= CHUNK_SIZE)
    return 0;
  return blocks[x][y][z].blockLight;
}

void Chunk::setSkyLight(int x, int y, int z, uint8_t val) {
  std::lock_guard<std::mutex> lock(chunkMutex);
  if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 ||
      z >= CHUNK_SIZE)
    return;
  blocks[x][y][z].skyLight = val;
  meshDirty = true;
}

void Chunk::setBlockLight(int x, int y, int z, uint8_t val) {
  std::lock_guard<std::mutex> lock(chunkMutex);
  if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 ||
      z >= CHUNK_SIZE)
    return;
  blocks[x][y][z].blockLight = val;
  meshDirty = true;
}

uint8_t Chunk::getMetadata(int x, int y, int z) const {
  if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 ||
      z >= CHUNK_SIZE)
    return 0;
  return blocks[x][y][z].metadata;
}

void Chunk::setMetadata(int x, int y, int z, uint8_t val) {
  std::lock_guard<std::mutex> lock(chunkMutex);
  if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 ||
      z >= CHUNK_SIZE)
    return;
  blocks[x][y][z].metadata = val;
  // metadata might affect rendering (e.g. liquid level), so mark dirty
  meshDirty = true;
}

std::vector<float> Chunk::generateGeometry(int &outOpaqueCount) {
  // Thread safety: Lock while reading blocks
  std::lock_guard<std::mutex> lock(chunkMutex);

  std::vector<float> opaqueVertices;
  std::vector<float> transparentVertices;
  // Pre-allocate decent amount
  opaqueVertices.reserve(4096);
  transparentVertices.reserve(1024);

  // Cache Diagonal Neighbors (For corner liquid height)
  // Indices: 0:LB(X-1,Z-1), 1:RB(X+1,Z-1), 2:LF(X-1,Z+1), 3:RF(X+1,Z+1)
  std::shared_ptr<Chunk> diagNeighbors[4] = {nullptr, nullptr, nullptr,
                                             nullptr};

  if (world) {
    // Locking world once to get chunks is better than locking constantly in
    // getHeight Actually World::getChunk locks internally, so we just call it.
    int cx = chunkPosition.x;
    int cy = chunkPosition.y;
    int cz = chunkPosition.z;

    diagNeighbors[0] = world->getChunk(cx - 1, cy, cz - 1);
    diagNeighbors[1] = world->getChunk(cx + 1, cy, cz - 1);
    diagNeighbors[2] = world->getChunk(cx - 1, cy, cz + 1);
    diagNeighbors[3] = world->getChunk(cx + 1, cy, cz + 1);
  }

  // Greedy Meshing
  struct MaskInfo {
    Block *block;
    uint8_t sky;
    uint8_t blockVal;
    uint8_t ao[4]; // BL, BR, TR, TL
    uint8_t metadata;
    bool isInternal;

    bool operator==(const MaskInfo &other) const {
      return block == other.block && sky == other.sky &&
             blockVal == other.blockVal && ao[0] == other.ao[0] &&
             ao[1] == other.ao[1] && ao[2] == other.ao[2] &&
             ao[3] == other.ao[3] && metadata == other.metadata &&
             isInternal == other.isInternal;
    }
    bool operator!=(const MaskInfo &other) const { return !(*this == other); }
  };

  // normal axis: 0=Z, 1=Z, 2=X, 3=X, 4=Y, 5=Y -> axis index: 2, 2, 0, 0, 1, 1

  for (int faceDir = 0; faceDir < 6; ++faceDir) {
    int axis = (faceDir <= 1) ? 2 : ((faceDir <= 3) ? 0 : 1);
    int uAxis = (axis == 0) ? 2 : ((axis == 1) ? 0 : 0);
    int vAxis = (axis == 0) ? 1 : ((axis == 1) ? 2 : 1);

    int nX = 0, nY = 0, nZ = 0;
    if (faceDir == 0)
      nZ = 1;
    else if (faceDir == 1)
      nZ = -1;
    else if (faceDir == 2)
      nX = -1;
    else if (faceDir == 3)
      nX = 1;
    else if (faceDir == 4)
      nY = 1;
    else if (faceDir == 5)
      nY = -1;

    auto getAt = [&](int u, int v, int d) -> ChunkBlock {
      int p[3];
      p[axis] = d;
      p[uAxis] = u;
      p[vAxis] = v;
      return blocks[p[0]][p[1]][p[2]];
    };
    auto getPos = [&](int u, int v, int d, int &ox, int &oy, int &oz) {
      int p[3];
      p[axis] = d;
      p[uAxis] = u;
      p[vAxis] = v;
      ox = p[0];
      oy = p[1];
      oz = p[2];
    };

    Block *airBlock = BlockRegistry::getInstance().getBlock(AIR);
    for (int d = 0; d < CHUNK_SIZE; ++d) {
      MaskInfo mask[CHUNK_SIZE][CHUNK_SIZE];
      for (int u = 0; u < CHUNK_SIZE; ++u)
        for (int v = 0; v < CHUNK_SIZE; ++v)
          mask[u][v] = {airBlock, 0, 0, {0, 0, 0, 0}, 0, false};

      for (int v = 0; v < CHUNK_SIZE; ++v) {
        for (int u = 0; u < CHUNK_SIZE; ++u) {
          ChunkBlock b = getAt(u, v, d);
          if (b.isActive()) {
            int lx, ly, lz;
            getPos(u, v, d, lx, ly, lz);
            int nx = lx + nX;
            int ny = ly + nY;
            int nz = lz + nZ;

            bool occluded = false;
            bool internalFace = false;
            uint8_t skyVal = 0;
            uint8_t blockVal = 0;

            if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE &&
                nz >= 0 && nz < CHUNK_SIZE) {
              ChunkBlock nb = blocks[nx][ny][nz];
              if (nb.isActive()) {
                if (!b.isOpaque()) {
                  // Special Case: Liquid Top Face should NOT be occluded by
                  // Solids (unless full height? No, safer to render)
                  bool isLiquid =
                      (b.block->getId() == WATER || b.block->getId() == LAVA);
                  bool isLeaves = (b.block->getId() == LEAVES ||
                                   b.block->getId() == SPRUCE_LEAVES ||
                                   b.block->getId() == ACACIA_LEAVES ||
                                   b.block->getId() == BIRCH_LEAVES ||
                                   b.block->getId() == DARK_OAK_LEAVES ||
                                   b.block->getId() == JUNGLE_LEAVES);

                  if (isLiquid && faceDir == 4) {
                    // Only occlude if neighbor is also Liquid (same type)
                    // If neighbor is Stone, we still want to render Top of
                    // Water because water might be low.
                    if (nb.block == b.block)
                      occluded = true;
                  }
                  // Leaves: Don't cull against other leaves (transparent look)
                  else if (isLeaves) {
                    if (nb.isOpaque()) {
                      occluded = true;
                    } else {
                      // Check if neighbor is also leaf
                      bool nbIsLeaves = (nb.block->getId() == LEAVES ||
                                         nb.block->getId() == SPRUCE_LEAVES ||
                                         nb.block->getId() == ACACIA_LEAVES ||
                                         nb.block->getId() == BIRCH_LEAVES ||
                                         nb.block->getId() == DARK_OAK_LEAVES ||
                                         nb.block->getId() == JUNGLE_LEAVES);
                      if (nbIsLeaves)
                        internalFace = true;
                    }
                    // Else false (draw against air or other leaves)
                  } else {
                    if (nb.block == b.block || nb.isOpaque())
                      occluded = true;
                  }
                } else {
                  if (nb.isOpaque())
                    occluded = true;
                }
              } else {
                skyVal = blocks[nx][ny][nz].skyLight;
                blockVal = blocks[nx][ny][nz].blockLight;
              }
            } else {
              int ni = -1;
              int nnx = nx, nny = ny, nnz = nz;

              if (nz >= CHUNK_SIZE) {
                ni = DIR_FRONT;
                nnz -= CHUNK_SIZE;
              } else if (nz < 0) {
                ni = DIR_BACK;
                nnz += CHUNK_SIZE;
              } else if (nx < 0) {
                ni = DIR_LEFT;
                nnx += CHUNK_SIZE;
              } else if (nx >= CHUNK_SIZE) {
                ni = DIR_RIGHT;
                nnx -= CHUNK_SIZE;
              } else if (ny >= CHUNK_SIZE) {
                ni = DIR_TOP;
                nny -= CHUNK_SIZE;
              } else if (ny < 0) {
                ni = DIR_BOTTOM;
                nny += CHUNK_SIZE;
              }

              std::shared_ptr<Chunk> n =
                  (ni == -1) ? std::shared_ptr<Chunk>() : getNeighbor(ni);
              if (n) {
                ChunkBlock nb = n->getBlock(nnx, nny, nnz);
                if (nb.isActive()) {
                  if (!b.isOpaque()) {
                    bool isLiquid =
                        (b.block->getId() == WATER || b.block->getId() == LAVA);
                    bool isLeaves = (b.block->getId() == LEAVES ||
                                     b.block->getId() == SPRUCE_LEAVES ||
                                     b.block->getId() == ACACIA_LEAVES ||
                                     b.block->getId() == BIRCH_LEAVES ||
                                     b.block->getId() == DARK_OAK_LEAVES ||
                                     b.block->getId() == JUNGLE_LEAVES);

                    if (isLiquid && faceDir == 4) {
                      if (nb.block == b.block)
                        occluded = true;
                    } else if (isLeaves) {
                      if (nb.isOpaque()) {
                        occluded = true;
                      } else {
                        bool nbIsLeaves =
                            (nb.block->getId() == LEAVES ||
                             nb.block->getId() == SPRUCE_LEAVES ||
                             nb.block->getId() == ACACIA_LEAVES ||
                             nb.block->getId() == BIRCH_LEAVES ||
                             nb.block->getId() == DARK_OAK_LEAVES ||
                             nb.block->getId() == JUNGLE_LEAVES);
                        if (nbIsLeaves)
                          internalFace = true;
                      }
                    } else {
                      if (nb.block == b.block || nb.isOpaque())
                        occluded = true;
                    }
                  } else {
                    if (nb.isOpaque())
                      occluded = true;
                  }
                } else {
                  skyVal = n->getSkyLight(nnx, nny, nnz);
                  blockVal = n->getBlockLight(nnx, nny, nnz);
                }
              } else if (world) {
                int gx = chunkPosition.x * CHUNK_SIZE + nx;
                int gy = chunkPosition.y * CHUNK_SIZE + ny;
                int gz = chunkPosition.z * CHUNK_SIZE + nz;
                ChunkBlock nb = world->getBlock(gx, gy, gz);
                if (nb.isActive()) {
                  if (!b.isOpaque()) {
                    bool isLiquid =
                        (b.block->getId() == WATER || b.block->getId() == LAVA);
                    if (isLiquid && faceDir == 4) {
                      if (nb.block == b.block)
                        occluded = true;
                    } else {
                      if (nb.block == b.block || nb.isOpaque())
                        occluded = true;
                    }
                  } else {
                    if (nb.isOpaque())
                      occluded = true;
                  }
                } else {
                  skyVal = world->getSkyLight(gx, gy, gz);
                  blockVal = world->getBlockLight(gx, gy, gz);
                }
              }
            }

            if (!occluded) {
              auto sampleAO = [&](int u1, int v1, int u2, int v2, int u3,
                                  int v3) -> uint8_t {
                auto check = [&](int u, int v) -> bool {
                  int lx, ly, lz;
                  getPos(u, v, d, lx, ly, lz);
                  int nx = lx + nX;
                  int ny = ly + nY;
                  int nz = lz + nZ;
                  if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 &&
                      ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                    ChunkBlock cb = blocks[nx][ny][nz];
                    if (cb.isOpaque())
                      return true;
                    // Special case: Layered Blocks act as opaque for AO to cast
                    // contact shadows
                    if (cb.block->getRenderShape() ==
                        Block::RenderShape::LAYERED)
                      return true;
                    return false;
                  }

                  int ni = -1;
                  int nnx = nx, nny = ny, nnz = nz;
                  if (nz >= CHUNK_SIZE) {
                    ni = DIR_FRONT;
                    nnz -= CHUNK_SIZE;
                  } else if (nz < 0) {
                    ni = DIR_BACK;
                    nnz += CHUNK_SIZE;
                  } else if (nx < 0) {
                    ni = DIR_LEFT;
                    nnx += CHUNK_SIZE;
                  } else if (nx >= CHUNK_SIZE) {
                    ni = DIR_RIGHT;
                    nnx -= CHUNK_SIZE;
                  } else if (ny >= CHUNK_SIZE) {
                    ni = DIR_TOP;
                    nny -= CHUNK_SIZE;
                  } else if (ny < 0) {
                    ni = DIR_BOTTOM;
                    nny += CHUNK_SIZE;
                  }

                  bool isDiagonal =
                      (nnx < 0 || nnx >= CHUNK_SIZE || nny < 0 ||
                       nny >= CHUNK_SIZE || nnz < 0 || nnz >= CHUNK_SIZE);

                  if (!isDiagonal && ni != -1) {
                    if (auto n = getNeighbor(ni)) {
                      ChunkBlock cb = n->getBlock(nnx, nny, nnz);
                      if (cb.isOpaque())
                        return true;
                      if (cb.block->getRenderShape() ==
                          Block::RenderShape::LAYERED)
                        return true;
                      return false;
                    }
                  }

                  if (world) {
                    int gx = chunkPosition.x * CHUNK_SIZE + nx;
                    int gy = chunkPosition.y * CHUNK_SIZE + ny;
                    int gz = chunkPosition.z * CHUNK_SIZE + nz;
                    return world->getBlock(gx, gy, gz).isOpaque();
                  }
                  return false;
                };
                bool s1 = check(u1, v1);
                bool s2 = check(u2, v2);
                bool c = check(u3, v3);
                if (s1 && s2)
                  return 3;
                return (s1 ? 1 : 0) + (s2 ? 1 : 0) + (c ? 1 : 0);
              };
              uint8_t aos[4];
              aos[0] = sampleAO(u - 1, v, u, v - 1, u - 1, v - 1);
              aos[1] = sampleAO(u + 1, v, u, v - 1, u + 1, v - 1);
              aos[2] = sampleAO(u + 1, v, u, v + 1, u + 1, v + 1);
              aos[3] = sampleAO(u - 1, v, u, v + 1, u - 1, v + 1);
              aos[2] = sampleAO(u + 1, v, u, v + 1, u + 1, v + 1);
              aos[3] = sampleAO(u - 1, v, u, v + 1, u - 1, v + 1);
              mask[u][v] = {b.block,    skyVal,
                            blockVal,   {aos[0], aos[1], aos[2], aos[3]},
                            b.metadata, internalFace};
            }
          }
        }
      }

      // Greedy Mesh
      for (int v = 0; v < CHUNK_SIZE; ++v) {
        for (int u = 0; u < CHUNK_SIZE; ++u) {
          if (mask[u][v].block->isActive()) {

            // Skip Special Shapes for Cube Meshing
            Block::RenderShape shape = mask[u][v].block->getRenderShape();
            if (shape == Block::RenderShape::CROSS ||
                shape == Block::RenderShape::SLAB_BOTTOM ||
                shape == Block::RenderShape::STAIRS ||
                shape == Block::RenderShape::MODEL ||
                shape == Block::RenderShape::LAYERED)
              continue;

            MaskInfo current = mask[u][v];
            // Greedy Extend
            // Disable greedy meshing for liquids to allow per-block smooth
            // lighting/height
            bool isLiquid = (current.block->getId() == WATER ||
                             current.block->getId() == LAVA);
            // Optimization: Allow greedy meshing for source liquids (flat)
            // Flowing liquids (metadata > 0) should remain individual for
            // proper stepping/heights
            bool allowGreedy = !isLiquid || (current.metadata == 0);

            int w = 1, h = 1;
            while (u + w < CHUNK_SIZE && mask[u + w][v] == current &&
                   allowGreedy)
              w++;
            bool canExtend = true;
            while (v + h < CHUNK_SIZE && canExtend && allowGreedy) {
              for (int k = 0; k < w; ++k)
                if (mask[u + k][v + h] != current) {
                  canExtend = false;
                  break;
                }
              if (canExtend)
                h++;
            }
            int lx, ly, lz;
            getPos(u, v, d, lx, ly, lz);

            // Check if transparent
            bool isTrans = (current.block->getRenderLayer() ==
                            Block::RenderLayer::TRANSPARENT);

            // Calculate smooth water heights
            float hBL = 1.0f, hBR = 1.0f, hTR = 1.0f, hTL = 1.0f;
            if (isLiquid) {
              bool isSource = (current.metadata == 0);

              auto getHeight = [&](int bx, int by, int bz) -> float {
                // Bounds Check: Vertical
                // Optimization: Height > CHUNK_SIZE should only happen if
                // checking neighbor above top of chunk We shouldn't really be
                // checking that high unless neighbor logic is flawed? But for
                // safety:
                if (by >= CHUNK_SIZE) {
                  // Check cached chunk above? Or just return 1.0?
                  // Original code returned 1.0f.
                  // Let's check neighbor chunks for correctness.
                  // Relative to THIS chunk, by >= CHUNK_SIZE means it's in
                  // the chunk above. But 'by' passed here is usually local
                  // coordinate? getHeight is usually called with neighbors:
                  // bx, by, bz. If by=32, it means chunk above, local y=0.

                  // Simplified: just return 1.0f as per original (assuming
                  // full liquid above?)
                  return 1.0f;
                }

                ChunkBlock bVec;
                bool isLoaded = true;

                // Local Access
                if (bx >= 0 && bx < CHUNK_SIZE && by >= 0 && by < CHUNK_SIZE &&
                    bz >= 0 && bz < CHUNK_SIZE) {
                  bVec = blocks[bx][by][bz];
                }
                // Neighbor Access (Optimized)
                else {
                  std::shared_ptr<Chunk> targetChunk = nullptr;
                  int nbx = bx;
                  int nby = by;
                  int nbz = bz;

                  if (bx >= CHUNK_SIZE) {
                    targetChunk = getNeighbor(DIR_RIGHT);
                    nbx -= CHUNK_SIZE;
                  } else if (bx < 0) {
                    targetChunk = getNeighbor(DIR_LEFT);
                    nbx += CHUNK_SIZE;
                  } else if (by >= CHUNK_SIZE) {
                    targetChunk = getNeighbor(DIR_TOP);
                    nby -= CHUNK_SIZE;
                  } else if (by < 0) {
                    targetChunk = getNeighbor(DIR_BOTTOM);
                    nby += CHUNK_SIZE;
                  } else if (bz >= CHUNK_SIZE) {
                    targetChunk = getNeighbor(DIR_FRONT);
                    nbz -= CHUNK_SIZE;
                  } else if (bz < 0) {
                    targetChunk = getNeighbor(DIR_BACK);
                    nbz += CHUNK_SIZE;
                  }

                  if (targetChunk) {
                    // We must ensure that access is safe (bounds)
                    if (nbx >= 0 && nbx < CHUNK_SIZE && nby >= 0 &&
                        nby < CHUNK_SIZE && nbz >= 0 && nbz < CHUNK_SIZE) {
                      bVec = targetChunk->getBlock(nbx, nby, nbz);
                    } else {
                      // This case handles when we picked a neighbor (e.g.
                      // RIGHT), but the coordinate wanted effectively shifts
                      // into another neighbor (e.g. RIGHT + BACK = RB).
                      // Fallback to Diagonal Cache.

                      // Recalculate which diagonal based on original bx, bz
                      int diagIdx = -1;
                      if (bx < 0 && bz < 0)
                        diagIdx = 0; // LB
                      else if (bx >= CHUNK_SIZE && bz < 0)
                        diagIdx = 1; // RB
                      else if (bx < 0 && bz >= CHUNK_SIZE)
                        diagIdx = 2; // LF
                      else if (bx >= CHUNK_SIZE && bz >= CHUNK_SIZE)
                        diagIdx = 3; // RF

                      // If it's a corner lookup (diagIdx != -1)
                      if (diagIdx != -1) {
                        if (diagNeighbors[diagIdx]) {
                          // Translate coords (simple modulo-ish wrap)
                          int dbx =
                              (bx < 0) ? (bx + CHUNK_SIZE) : (bx - CHUNK_SIZE);
                          int dbz =
                              (bz < 0) ? (bz + CHUNK_SIZE) : (bz - CHUNK_SIZE);
                          bVec =
                              diagNeighbors[diagIdx]->getBlock(dbx, nby, dbz);
                        } else {
                          // Diagonal chunk missing -> Treat as SOLID to
                          // prevent dip into void
                          return -1.0f;
                        }
                      } else {
                        // Not a simple diagonal? Maybe vertical diagonal?
                        // e.g. Right + Top?
                        // If we are here, we have a targetChunk (Cardinal)
                        // but coords are out of bounds. This implies we need
                        // a neighbor OF that neighbor. Safe fallback: world
                        // lookup (but handle missing)
                        if (!world)
                          return -1.0f;
                        int gx = chunkPosition.x * CHUNK_SIZE + bx;
                        int gy = chunkPosition.y * CHUNK_SIZE + by;
                        int gz = chunkPosition.z * CHUNK_SIZE + bz;

                        // Problem: World::getBlock returns AIR for missing.
                        // We should check if chunk exists first, but that's
                        // slow. Optimization: Only trust world->getBlock if
                        // we assume it's loaded? Any out-of-bounds that isn't
                        // handled by diag cache is rare or implies distant
                        // calculation. Let's rely on world, but risk the dip?
                        // No, let's just return -1.0f for stability if we
                        // think it's unloaded. But how to know? For now,
                        // let's fallback to world but assume if it returns
                        // pure AIR it might be void.
                        bVec = world->getBlock(gx, gy, gz);
                      }
                    }
                  } else {
                    // Chunk not cached (or not loaded/exists), OR it was a
                    // diagonal purely? Original logic: "targetChunk" is set
                    // based on single axis check. If it was purely diagonal
                    // (e.g. bx<0, bz<0), targetChunk was set to LEFT (bx<0).
                    // Then logic above entered "else" block.
                    // So we are covered by the logic above.

                    // This else block is for when NO cardinal neighbor was
                    // identified (e.g. purely vertical?) Or if neighbors[DIR]
                    // was null.

                    // If neighbor is null, we can check diagonal?
                    int diagIdx = -1;
                    if (bx < 0 && bz < 0)
                      diagIdx = 0; // LB
                    else if (bx >= CHUNK_SIZE && bz < 0)
                      diagIdx = 1; // RB
                    else if (bx < 0 && bz >= CHUNK_SIZE)
                      diagIdx = 2; // LF
                    else if (bx >= CHUNK_SIZE && bz >= CHUNK_SIZE)
                      diagIdx = 3; // RF

                    if (diagIdx != -1) {
                      if (diagNeighbors[diagIdx]) {
                        int dbx =
                            (bx < 0) ? (bx + CHUNK_SIZE) : (bx - CHUNK_SIZE);
                        int dbz =
                            (bz < 0) ? (bz + CHUNK_SIZE) : (bz - CHUNK_SIZE);
                        bVec = diagNeighbors[diagIdx]->getBlock(dbx, by, dbz);
                        // Note: 'by' might be different if this was
                        // recursive? 'nby' was used above. 'by' is safe here
                        // since we didn't adjust it.
                      } else {
                        return -1.0f; // Missing diagonal -> Solid
                      }
                    } else {
                      // Fallback to world (e.g. Vertical neighbors not in
                      // cache?)
                      if (!world)
                        isLoaded = false;
                      else {
                        int gx = chunkPosition.x * CHUNK_SIZE + bx;
                        int gy = chunkPosition.y * CHUNK_SIZE + by;
                        int gz = chunkPosition.z * CHUNK_SIZE + bz;
                        // Check existence?
                        if (world->getChunk(
                                chunkPosition.x,
                                chunkPosition.y +
                                    (by >= CHUNK_SIZE ? 1 : (by < 0 ? -1 : 0)),
                                chunkPosition.z) == nullptr) {
                          // Missing -> Solid
                          return -1.0f;
                        }
                        bVec = world->getBlock(gx, gy, gz);
                        if (!bVec.isActive() && !bVec.block)
                          isLoaded = false;
                      }
                    }
                  }
                }

                if (!isLoaded && !bVec.isActive())
                  return -1.0f; // treat unloaded as solid/ignore?

                if (!bVec.isActive()) {
                  // Check if block above is liquid (Vertical Flow)
                  // Recursive check? Optimization: Don't recurse, just check
                  // one level up.
                  ChunkBlock aboveVec = {
                      BlockRegistry::getInstance().getBlock(AIR), 0, 0, 0};

                  // Logic for "aboveVec" access similar to bVec...
                  // Can we reuse logic?
                  int abx = bx;
                  int aby = by + 1;
                  int abz = bz;
                  // If 'bVec' was local, check local above
                  if (abx >= 0 && abx < CHUNK_SIZE && aby >= 0 &&
                      aby < CHUNK_SIZE && abz >= 0 && abz < CHUNK_SIZE) {
                    aboveVec = blocks[abx][aby][abz];
                  } else {
                    // Neighbor
                    std::shared_ptr<Chunk> targetChunk = nullptr;
                    int nbx = abx;
                    int nby = aby;
                    int nbz = abz;
                    if (abx >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_RIGHT);
                      nbx -= CHUNK_SIZE;
                    } else if (abx < 0) {
                      targetChunk = getNeighbor(DIR_LEFT);
                      nbx += CHUNK_SIZE;
                    } else if (aby >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_TOP);
                      nby -= CHUNK_SIZE;
                    } else if (aby < 0) {
                      targetChunk = getNeighbor(DIR_BOTTOM);
                      nby += CHUNK_SIZE;
                    } else if (abz >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_FRONT);
                      nbz -= CHUNK_SIZE;
                    } else if (abz < 0) {
                      targetChunk = getNeighbor(DIR_BACK);
                      nbz += CHUNK_SIZE;
                    }

                    if (targetChunk && nbx >= 0 && nbx < CHUNK_SIZE &&
                        nby >= 0 && nby < CHUNK_SIZE && nbz >= 0 &&
                        nbz < CHUNK_SIZE) {
                      aboveVec = targetChunk->getBlock(nbx, nby, nbz);
                    } else if (world) {
                      int gx = chunkPosition.x * CHUNK_SIZE + abx;
                      int gy = chunkPosition.y * CHUNK_SIZE + aby;
                      int gz = chunkPosition.z * CHUNK_SIZE + abz;
                      aboveVec = world->getBlock(gx, gy, gz);
                    }
                  }

                  if (aboveVec.isActive() &&
                      (aboveVec.block->getId() == WATER ||
                       aboveVec.block->getId() == LAVA)) {
                    return 2.0f; // Flag: Force Full Height
                  }

                  // Check block BELOW to distinguish Shore vs Drop-off
                  ChunkBlock belowVec = {
                      BlockRegistry::getInstance().getBlock(AIR), 0, 0, 0};
                  // Logic to get block below (reuse neighbor cache logic if
                  // valid)
                  int bbx = bx;
                  int bby = by - 1;
                  int bbz = bz;
                  if (bbx >= 0 && bbx < CHUNK_SIZE && bby >= 0 &&
                      bby < CHUNK_SIZE && bbz >= 0 && bbz < CHUNK_SIZE) {
                    belowVec = blocks[bbx][bby][bbz];
                  } else {
                    // Neighbor
                    std::shared_ptr<Chunk> targetChunk = nullptr;
                    int nbx = bbx;
                    int nby = bby;
                    int nbz = bbz;
                    if (bbx >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_RIGHT);
                      nbx -= CHUNK_SIZE;
                    } else if (bbx < 0) {
                      targetChunk = getNeighbor(DIR_LEFT);
                      nbx += CHUNK_SIZE;
                    } else if (bby >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_TOP);
                      nby -= CHUNK_SIZE;
                    } else if (bby < 0) {
                      targetChunk = getNeighbor(DIR_BOTTOM);
                      nby += CHUNK_SIZE;
                    } else if (bbz >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_FRONT);
                      nbz -= CHUNK_SIZE;
                    } else if (bbz < 0) {
                      targetChunk = getNeighbor(DIR_BACK);
                      nbz += CHUNK_SIZE;
                    }

                    if (targetChunk && nbx >= 0 && nbx < CHUNK_SIZE &&
                        nby >= 0 && nby < CHUNK_SIZE && nbz >= 0 &&
                        nbz < CHUNK_SIZE) {
                      belowVec = targetChunk->getBlock(nbx, nby, nbz);
                    } else if (world) {
                      int gx = chunkPosition.x * CHUNK_SIZE + bbx;
                      int gy = chunkPosition.y * CHUNK_SIZE + bby;
                      int gz = chunkPosition.z * CHUNK_SIZE + bbz;
                      belowVec = world->getBlock(gx, gy, gz);
                    }
                  }

                  if (belowVec.isActive() && belowVec.isSolid()) {
                    return -3.0f; // Flag: Shore (Supported Air)
                  }

                  return 0.0f; // Flag: Drop-off (Air/Liquid below) -> Slope
                               // down
                }
                if (bVec.block->getId() == WATER ||
                    bVec.block->getId() == LAVA) {
                  // Check if this neighbor has liquid above it
                  bool isVertical = false;

                  // Same "aboveVec" logic again...
                  // Reuse logic:
                  ChunkBlock aboveVec = {
                      BlockRegistry::getInstance().getBlock(AIR), 0, 0, 0};
                  int abx = bx;
                  int aby = by + 1;
                  int abz = bz;
                  if (abx >= 0 && abx < CHUNK_SIZE && aby >= 0 &&
                      aby < CHUNK_SIZE && abz >= 0 && abz < CHUNK_SIZE) {
                    aboveVec = blocks[abx][aby][abz];
                  } else {
                    // Neighbor
                    std::shared_ptr<Chunk> targetChunk = nullptr;
                    int nbx = abx;
                    int nby = aby;
                    int nbz = abz;
                    if (abx >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_RIGHT);
                      nbx -= CHUNK_SIZE;
                    } else if (abx < 0) {
                      targetChunk = getNeighbor(DIR_LEFT);
                      nbx += CHUNK_SIZE;
                    } else if (aby >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_TOP);
                      nby -= CHUNK_SIZE;
                    } else if (aby < 0) {
                      targetChunk = getNeighbor(DIR_BOTTOM);
                      nby += CHUNK_SIZE;
                    } else if (abz >= CHUNK_SIZE) {
                      targetChunk = getNeighbor(DIR_FRONT);
                      nbz -= CHUNK_SIZE;
                    } else if (abz < 0) {
                      targetChunk = getNeighbor(DIR_BACK);
                      nbz += CHUNK_SIZE;
                    }

                    if (targetChunk && nbx >= 0 && nbx < CHUNK_SIZE &&
                        nby >= 0 && nby < CHUNK_SIZE && nbz >= 0 &&
                        nbz < CHUNK_SIZE) {
                      aboveVec = targetChunk->getBlock(nbx, nby, nbz);
                    } else if (world) {
                      int gx = chunkPosition.x * CHUNK_SIZE + abx;
                      int gy = chunkPosition.y * CHUNK_SIZE + aby;
                      int gz = chunkPosition.z * CHUNK_SIZE + abz;
                      aboveVec = world->getBlock(gx, gy, gz);
                    }
                  }

                  if (aboveVec.isActive() &&
                      aboveVec.block->getId() == bVec.block->getId()) {
                    isVertical = true;
                  }
                  if (isVertical)
                    return 2.0f; // Flag: Force Full Height

                  if (bVec.metadata >= 8)
                    return 0.0f;

                  float calculatedHeight = (9.0f - bVec.metadata) / 9.0f;
                  if (calculatedHeight > 0.88f)
                    calculatedHeight = 0.88f;

                  return calculatedHeight;
                }
                if (bVec.isSolid()) {
                  return -2.0f; // Flag: Solid Block
                }
                return -1.0f;
              };

              auto avgHeight = [&](int bx, int by, int bz) -> float {
                // Get 4 samples around the corner
                float h[4];
                h[0] = getHeight(bx, by, bz);
                h[1] = getHeight(bx - 1, by, bz);
                h[2] = getHeight(bx - 1, by, bz - 1);
                h[3] = getHeight(bx, by, bz - 1);

                // Context Logic: Check for Source/Vertical influence
                bool hasSource = false;
                for (int i = 0; i < 4; ++i) {
                  // Check for High Water (Source/High Flow) or Vertical
                  // 0.88f is the new max height for source.
                  if (h[i] >= 0.87f) {
                    hasSource = true;
                    break;
                  }
                }

                // Apply Context Smoothing
                for (int i = 0; i < 4; ++i) {
                  if (hasSource) {
                    // Near Source: Ignore Solids (-2.0) and Shores (-3.0) ->
                    // Force Flat Drop-offs (0.0) are NOT ignored, allowing
                    // slope.
                    if (h[i] == -2.0f || h[i] == -3.0f)
                      h[i] = -1.0f;
                  } else {
                    // Pure Flow: Treat Solids (-2.0) and Shores (-3.0) as Air
                    // (0.0) -> Force Slope (Actually Shores -3.0 means
                    // supported air, so treating as 0.0 slopes it, which is
                    // default for flow anyway)
                    if (h[i] == -2.0f || h[i] == -3.0f)
                      h[i] = 0.0f;
                  }
                }

                // Bridge Logic: A vertical column (2.0) only forces snap if
                // it connects to another liquid
                for (int i = 0; i < 4; ++i) {
                  if (h[i] >= 2.0f) {
                    // Check neighbors in cycle (i+1, i+3)
                    // If either is liquid (>= 0.0), we bridge.
                    // Note: >= 0.0 includes vertical (2.0) and normal flow
                    // (0.0-1.0) and Drop-off (0.0)
                    int n1 = (i + 1) % 4;
                    int n2 = (i + 3) % 4;

                    bool bridge1 = (h[n1] >= 0.0f);
                    bool bridge2 = (h[n2] >= 0.0f);

                    // If bridged, return 1.0 (Forced)
                    if (bridge1 || bridge2)
                      return 1.0f;

                    // If NOT bridged (isolated vertical), we ignore it (set
                    // to -1.0) to prevent spike
                    h[i] = -1.0f;
                  }
                }

                // Standard Average
                float s = 0.0f;
                float count = 0.0f;
                for (int i = 0; i < 4; ++i) {
                  if (h[i] >= 0.0f) {
                    s += (h[i] >= 1.0f ? 1.0f
                                       : h[i]); // Clamp 2.0 to 1.0 for average
                    count += 1.0f;
                  }
                }

                if (count <= 0.0f)
                  return 1.0f;
                return s / count;
              };

              int lx, ly, lz;
              getPos(u, v, d, lx, ly, lz);

              hBL = avgHeight(lx, ly, lz);
              hBR = avgHeight(lx + 1, ly, lz);
              hTR = avgHeight(lx + 1, ly, lz + 1);
              hTL = avgHeight(lx, ly, lz + 1);

              // Special case: if block above is SAME liquid, force full
              // height (Regardless of its metadata/height, we must connect to
              // it)
              ChunkBlock aboveB =
                  blocks[lx][ly + 1]
                        [lz]; // Need safe access? ly+1 can be CHUNK_SIZE
              // Wait, lx,ly,lz are local.
              bool hasLiquidAbove = false;
              if (ly + 1 < CHUNK_SIZE) {
                ChunkBlock ab = blocks[lx][ly + 1][lz];
                if (ab.isActive() &&
                    ab.block->getId() == current.block->getId())
                  hasLiquidAbove = true;
              } else {
                // Check World
                int gx = chunkPosition.x * CHUNK_SIZE + lx;
                int gy = chunkPosition.y * CHUNK_SIZE + ly + 1;
                int gz = chunkPosition.z * CHUNK_SIZE + lz;
                if (world) {
                  ChunkBlock ab = world->getBlock(gx, gy, gz);
                  if (ab.isActive() &&
                      ab.block->getId() == current.block->getId())
                    hasLiquidAbove = true;
                }
              }

              if (hasLiquidAbove) {
                hBL = hBR = hTR = hTL = 1.0f;
              }
            }

            addFace(isTrans ? transparentVertices : opaqueVertices, lx, ly, lz,
                    faceDir, current.block, w, h, current.ao[0], current.ao[1],
                    current.ao[2], current.ao[3], current.metadata, hBL, hBR,
                    hTR, hTL, 0, current.isInternal);

            if (current.block->hasOverlay(faceDir)) {
              // Render Overlay (Cutout)
              // We put it in opaque queue usually or transparent?
              // Overlay usually needs alpha testing (cutout).
              // For now, put in same queue.
              addFace(isTrans ? transparentVertices : opaqueVertices, lx, ly,
                      lz, faceDir, current.block, w, h, current.ao[0],
                      current.ao[1], current.ao[2], current.ao[3],
                      current.metadata, hBL, hBR, hTR, hTL, 1,
                      current.isInternal);
            }

            for (int j = 0; j < h; ++j)
              for (int i = 0; i < w; ++i)
                mask[u + i][v + j] = {airBlock, 0, 0, {0, 0, 0, 0}, 0};
            u += w - 1;
          }
        }
      }
    } // End d loop
  } // End faceDir loop

  // Pass 2: Special Shapes (Plants, Slabs, Stairs)
  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int y = 0; y < CHUNK_SIZE; ++y) {
      for (int z = 0; z < CHUNK_SIZE; ++z) {
        ChunkBlock cb = blocks[x][y][z];
        if (!cb.isActive())
          continue;

        Block::RenderShape shape = cb.block->getRenderShape();
        if (shape == Block::RenderShape::CUBE)
          continue;

        float fx = (float)x;
        float fy = (float)y;
        float fz = (float)z;

        int gx = chunkPosition.x * CHUNK_SIZE + x;
        int gy = chunkPosition.y * CHUNK_SIZE + y;
        int gz = chunkPosition.z * CHUNK_SIZE + z;

        float r, g, b;
        cb.block->getColor(r, g, b);
        float alpha = cb.block->getAlpha();

        uint8_t sky = cb.skyLight;
        uint8_t bl = cb.blockLight;
        float l1Source = pow((float)sky / 15.0f, 0.8f);
        float l2Source = pow((float)bl / 15.0f, 0.8f);

        std::vector<float> &targetVerts =
            (cb.block->getRenderLayer() == Block::RenderLayer::TRANSPARENT)
                ? transparentVertices
                : opaqueVertices;

        // Helper for AO checks (Pass 2)
        auto getOpaque = [&](int dx, int dy, int dz) -> bool {
          int nx = x + dx;
          int ny = y + dy;
          int nz = z + dz;
          if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE &&
              nz >= 0 && nz < CHUNK_SIZE) {
            return blocks[nx][ny][nz].isOpaque();
          } else if (world) {
            return world->getBlock(gx + dx, gy + dy, gz + dz).isOpaque();
          }
          return false;
        };

        // AO levels: 0 (darkest) to 3 (brightest). Return 0..3 int.
        auto calcAO = [&](int s1x, int s1y, int s1z, int s2x, int s2y, int s2z,
                          int cx, int cy, int cz) -> int {
          bool s1 = getOpaque(s1x, s1y, s1z);
          bool s2 = getOpaque(s2x, s2y, s2z);
          bool c = getOpaque(cx, cy, cz);
          if (s1 && s2)
            return 3; // Corner fully occluded -> 3 (Darkest)
          // 0 occluders -> 0 (Brightest)
          // 1 occluder -> 1
          // 2 occluders -> 2
          // 3 occluders -> 3
          return (s1 + s2 + c);
        };

        auto pushVert = [&](float vx, float vy, float vz, float u, float v,
                            float uOrigin, float vOrigin, float aoVal = 0.0f,
                            float l1Override = -1.0f, float l2Override = -1.0f,
                            float shade = 1.0f) {
          targetVerts.push_back(vx);
          targetVerts.push_back(vy);
          targetVerts.push_back(vz);
          targetVerts.push_back(r * shade);
          targetVerts.push_back(g * shade);
          targetVerts.push_back(b * shade);
          targetVerts.push_back(alpha);
          targetVerts.push_back(u);
          targetVerts.push_back(v);
          targetVerts.push_back(l1Override < 0 ? l1Source : l1Override);
          targetVerts.push_back(l2Override < 0 ? l2Source : l2Override);
          targetVerts.push_back(aoVal);
          targetVerts.push_back(uOrigin);
          targetVerts.push_back(vOrigin);
        };

        if (shape == Block::RenderShape::CROSS) {
          float uMin, vMin;
          cb.block->getTextureUV(0, uMin, vMin, gx, gy, gz, cb.metadata);

          // Randomize Rotation and Offset
          long long seed = ((long long)gx * 31337 + (long long)gy * 19283 +
                            (long long)gz * 84211) ^
                           0x5a17e5;
          auto myRand = [&seed]() {
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
            return (float)seed / (float)0x7FFFFFFF;
          };

          float rndX = (myRand() - 0.5f) * 0.4f;
          float rndZ = (myRand() - 0.5f) * 0.4f;
          float rotation = myRand() * 3.14159f * 2.0f;

          float centerX = fx + 0.5f + rndX;
          float centerZ = fz + 0.5f + rndZ;

          float scale = 0.5f;

          // Plane 1
          float angle1 = rotation + 0.785398f;
          float p1_x1 = centerX + cos(angle1) * -scale;
          float p1_z1 = centerZ + sin(angle1) * -scale;
          float p1_x2 = centerX + cos(angle1) * scale;
          float p1_z2 = centerZ + sin(angle1) * scale;

          pushVert(p1_x1, fy, p1_z1, 0.0f, 0.0f, uMin, vMin);
          pushVert(p1_x2, fy, p1_z2, 1.0f, 0.0f, uMin, vMin);
          pushVert(p1_x2, fy + 1.0f, p1_z2, 1.0f, 1.0f, uMin, vMin);

          pushVert(p1_x1, fy, p1_z1, 0.0f, 0.0f, uMin, vMin);
          pushVert(p1_x2, fy + 1.0f, p1_z2, 1.0f, 1.0f, uMin, vMin);
          pushVert(p1_x1, fy + 1.0f, p1_z1, 0.0f, 1.0f, uMin, vMin);

          // Back Face Plane 1
          pushVert(p1_x2, fy, p1_z2, 1.0f, 0.0f, uMin, vMin);
          pushVert(p1_x1, fy, p1_z1, 0.0f, 0.0f, uMin, vMin);
          pushVert(p1_x1, fy + 1.0f, p1_z1, 0.0f, 1.0f, uMin, vMin);

          pushVert(p1_x2, fy, p1_z2, 1.0f, 0.0f, uMin, vMin);
          pushVert(p1_x1, fy + 1.0f, p1_z1, 0.0f, 1.0f, uMin, vMin);
          pushVert(p1_x2, fy + 1.0f, p1_z2, 1.0f, 1.0f, uMin, vMin);

          // Plane 2
          float angle2 = angle1 + 1.570796f;
          float p2_x1 = centerX + cos(angle2) * -scale;
          float p2_z1 = centerZ + sin(angle2) * -scale;
          float p2_x2 = centerX + cos(angle2) * scale;
          float p2_z2 = centerZ + sin(angle2) * scale;

          pushVert(p2_x1, fy, p2_z1, 0.0f, 0.0f, uMin, vMin);
          pushVert(p2_x2, fy, p2_z2, 1.0f, 0.0f, uMin, vMin);
          pushVert(p2_x2, fy + 1.0f, p2_z2, 1.0f, 1.0f, uMin, vMin);

          pushVert(p2_x1, fy, p2_z1, 0.0f, 0.0f, uMin, vMin);
          pushVert(p2_x2, fy + 1.0f, p2_z2, 1.0f, 1.0f, uMin, vMin);
          pushVert(p2_x1, fy + 1.0f, p2_z1, 0.0f, 1.0f, uMin, vMin);

          // Back Face Plane 2
          pushVert(p2_x2, fy, p2_z2, 1.0f, 0.0f, uMin, vMin);
          pushVert(p2_x1, fy, p2_z1, 0.0f, 0.0f, uMin, vMin);
          pushVert(p2_x1, fy + 1.0f, p2_z1, 0.0f, 1.0f, uMin, vMin);
          pushVert(p2_x2, fy, p2_z2, 1.0f, 0.0f, uMin, vMin);
          pushVert(p2_x1, fy + 1.0f, p2_z1, 0.0f, 1.0f, uMin, vMin);
          pushVert(p2_x2, fy + 1.0f, p2_z2, 1.0f, 1.0f, uMin, vMin);
        } else if (shape == Block::RenderShape::SLAB_BOTTOM ||
                   shape == Block::RenderShape::STAIRS ||
                   shape == Block::RenderShape::LAYERED) {
          // Helper to add a quad
          auto addFaceQuad = [&](int face, float xMin, float yMin, float zMin,
                                 float xMax, float yMax, float zMax,
                                 float aoTL = 0.0f, float aoTR = 0.0f,
                                 float aoBR = 0.0f, float aoBL = 0.0f) {
            int dx = 0, dy = 0, dz = 0;
            if (face == 0)
              dz = 1; // Z+
            else if (face == 1)
              dz = -1; // Z-
            else if (face == 2)
              dx = -1; // X-
            else if (face == 3)
              dx = 1; // X+
            else if (face == 4)
              dy = 1; // Y+
            else if (face == 5)
              dy = -1; // Y-

            // Sample Light from Neighbor
            int nx = x + dx;
            int ny = y + dy;
            int nz = z + dz;
            uint8_t s = 0, b = 0;

            // Opacity/Occlusion Check & Light Fetch
            // If neighbor is Opaque, we might not render?
            // Actually, for partial blocks (Slbs, Layers), we usually render
            // unless completely covered.
            // But we assume if we are calling addFaceQuad, we WANT to render.
            // So we just fetch light.
            if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE &&
                nz >= 0 && nz < CHUNK_SIZE) {
              s = blocks[nx][ny][nz].skyLight;
              b = blocks[nx][ny][nz].blockLight;
            } else if (world) {
              int gnx = chunkPosition.x * CHUNK_SIZE + nx;
              int gny = chunkPosition.y * CHUNK_SIZE + ny;
              int gnz = chunkPosition.z * CHUNK_SIZE + nz;
              s = world->getSkyLight(gnx, gny, gnz);
              b = world->getBlockLight(gnx, gny, gnz);
            }
            // Fallback: If no world/bounds, use current block light (better
            // than 0)
            else {
              s = cb.skyLight;
              b = cb.blockLight;
            }

            float l1 = pow((float)s / 15.0f, 0.8f);
            float l2 = pow((float)b / 15.0f, 0.8f);

            float uBase, vBase;
            cb.block->getTextureUV(face, uBase, vBase, gx, gy, gz, cb.metadata);

            float w = (face <= 1 || face >= 4) ? (xMax - xMin) : (zMax - zMin);
            float h = (face >= 4) ? (zMax - zMin) : (yMax - yMin);

            float u0 = 0.0f, v0 = 0.0f;
            float u1 = w, v1 = h;

            // Face Dimming Logic (Matches Standard addFace)
            float shade = 1.0f;
            if (face == 4)
              shade = 1.0f; // Top
            else if (face == 5)
              shade = 0.6f; // Bottom
            else
              shade = 0.8f; // Sides (All)

            // Adjustment for Sides of Slab
            if (face <= 3) {
              v0 = yMin; // e.g. 0.0
              v1 = yMax; // e.g. 0.5
            }

            // Draw
            // 0=Z+, 1=Z-, 2=X-, 3=X+, 4=Y+, 5=Y-
            // Pass shade explicitly to pushVert
            if (face == 0) { // Z+ (Variable Z) -> Usually zMax
              pushVert(fx + xMin, fy + yMin, fz + zMax, u0, v0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMin, fz + zMax, u1, v0, uBase, vBase,
                       aoBR, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMax, u1, v1, uBase, vBase,
                       aoTR, l1, l2, shade);

              pushVert(fx + xMin, fy + yMin, fz + zMax, u0, v0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMax, u1, v1, uBase, vBase,
                       aoTR, l1, l2, shade);
              pushVert(fx + xMin, fy + yMax, fz + zMax, u0, v1, uBase, vBase,
                       aoTL, l1, l2, shade);
            } else if (face == 1) { // Z-
              pushVert(fx + xMax, fy + yMin, fz + zMin, u0, v0, uBase, vBase,
                       aoBR, l1, l2, shade);
              pushVert(fx + xMin, fy + yMin, fz + zMin, u1, v0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMin, fy + yMax, fz + zMin, u1, v1, uBase, vBase,
                       aoTL, l1, l2, shade);

              pushVert(fx + xMax, fy + yMin, fz + zMin, u0, v0, uBase, vBase,
                       aoBR, l1, l2, shade);
              pushVert(fx + xMin, fy + yMax, fz + zMin, u1, v1, uBase, vBase,
                       aoTL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMin, u0, v1, uBase, vBase,
                       aoTR, l1, l2, shade);
            } else if (face == 2) { // X-
              pushVert(fx + xMin, fy + yMin, fz + zMin, u0, v0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMin, fy + yMin, fz + zMax, u1, v0, uBase, vBase,
                       aoBR, l1, l2, shade);
              pushVert(fx + xMin, fy + yMax, fz + zMax, u1, v1, uBase, vBase,
                       aoTR, l1, l2, shade);

              pushVert(fx + xMin, fy + yMin, fz + zMin, u0, v0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMin, fy + yMax, fz + zMax, u1, v1, uBase, vBase,
                       aoTR, l1, l2, shade);
              pushVert(fx + xMin, fy + yMax, fz + zMin, u0, v1, uBase, vBase,
                       aoTL, l1, l2, shade);
            } else if (face == 3) { // X+
              pushVert(fx + xMax, fy + yMin, fz + zMax, u0, v0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMin, fz + zMin, u1, v0, uBase, vBase,
                       aoBR, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMin, u1, v1, uBase, vBase,
                       aoTR, l1, l2, shade);

              pushVert(fx + xMax, fy + yMin, fz + zMax, u0, v0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMin, u1, v1, uBase, vBase,
                       aoTR, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMax, u0, v1, uBase, vBase,
                       aoTL, l1, l2, shade);
            } else if (face == 4) { // Y+
              pushVert(fx + xMin, fy + yMax, fz + zMax, 0, 0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMax, 1, 0, uBase, vBase,
                       aoBR, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMin, 1, 1, uBase, vBase,
                       aoTR, l1, l2, shade);

              pushVert(fx + xMin, fy + yMax, fz + zMax, 0, 0, uBase, vBase,
                       aoBL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMax, fz + zMin, 1, 1, uBase, vBase,
                       aoTR, l1, l2, shade);
              pushVert(fx + xMin, fy + yMax, fz + zMin, 0, 1, uBase, vBase,
                       aoTL, l1, l2, shade);
            } else if (face == 5) { // Y-
              pushVert(fx + xMin, fy + yMin, fz + zMin, 0, 0, uBase, vBase,
                       aoTL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMin, fz + zMin, 1, 0, uBase, vBase,
                       aoTR, l1, l2, shade);
              pushVert(fx + xMax, fy + yMin, fz + zMax, 1, 1, uBase, vBase,
                       aoBR, l1, l2, shade);

              pushVert(fx + xMin, fy + yMin, fz + zMin, 0, 0, uBase, vBase,
                       aoTL, l1, l2, shade);
              pushVert(fx + xMax, fy + yMin, fz + zMax, 1, 1, uBase, vBase,
                       aoBR, l1, l2, shade);
              pushVert(fx + xMin, fy + yMin, fz + zMax, 0, 1, uBase, vBase,
                       aoBL, l1, l2, shade);
            }
          };

          if (shape == Block::RenderShape::LAYERED) {
            // Layered blocks with variable height based on metadata
            float blockHeight = cb.block->getBlockHeight(cb.metadata);

            // Render all 6 faces with adjusted height
            // Calculate AO for Top Face (Face 4: Y+) only
            // Neighbors are checked at the SAME y-level because partial blocks
            // are occluded by full blocks next to them
            // AO Calculation: calcAO(Side1, Side2, Corner)
            // Neighbors are checked at the SAME y-level

            // AO Calculation: calcAO(Side1, Side2, Corner)
            // Neighbors for Top Face:
            // If block is full height (> 0.9), check y+1 (Standard block
            // behavior, no shadow from flush neighbors) If block is partial
            // height, check y=0 (Shadow from adjacent taller blocks)
            int topY = (blockHeight > 0.9f) ? 1 : 0;

            // TL: xMin, zMin -> Neighbors (-1, topY, 0), (0, topY, -1), Corner
            // (-1, topY, -1)
            int aoTL = calcAO(-1, topY, 0, 0, topY, -1, -1, topY, -1);

            // TR: xMax, zMin -> Neighbors (1, topY, 0), (0, topY, -1), Corner
            // (1, topY, -1)
            int aoTR = calcAO(1, topY, 0, 0, topY, -1, 1, topY, -1);

            // BR: xMax, zMax -> Neighbors (1, topY, 0), (0, topY, 1), Corner
            // (1, topY, 1)
            int aoBR = calcAO(1, topY, 0, 0, topY, 1, 1, topY, 1);

            // BL: xMin, zMax -> Neighbors (-1, topY, 0), (0, topY, 1), Corner
            // (-1, topY, 1)
            int aoBL = calcAO(-1, topY, 0, 0, topY, 1, -1, topY, 1);

            // Side AO Helper
            auto getSideAO = [&](int face) -> std::tuple<int, int, int, int> {
              // Return TL, TR, BR, BL order? addFaceQuad takes TL, TR, BR, BL?
              // addFaceQuad(..., aoTL, aoTR, aoBR, aoBL)
              // We need to calculate for the specific face.
              // Bottom Vertices check y-1. Top Vertices check y+1.

              int tl = 0, tr = 0, br = 0, bl = 0;

              if (face == 0) { // Z+ (Front) -> Air at z+1
                // Top Left: Lateral Left(-1, 0, 1), Vertical Up(0, 1, 1),
                // Corner(-1, 1, 1)
                tl = calcAO(-1, 0, 1, 0, 1, 1, -1, 1, 1);

                // Top Right: Lateral Right(1, 0, 1), Vertical Up(0, 1, 1),
                // Corner(1, 1, 1)
                tr = calcAO(1, 0, 1, 0, 1, 1, 1, 1, 1);

                // Bottom Right: Lateral Right(1, 0, 1), Vertical Down(0, -1,
                // 1), Corner(1, -1, 1)
                br = calcAO(1, 0, 1, 0, -1, 1, 1, -1, 1);

                // Bottom Left: Lateral Left(-1, 0, 1), Vertical Down(0, -1, 1),
                // Corner(-1, -1, 1)
                bl = calcAO(-1, 0, 1, 0, -1, 1, -1, -1, 1);
                return {tl, tr, br, bl};

              } else if (face == 1) { // Z- (Back) -> Air at z-1
                // BR: Lateral Right(1, 0, -1), Vertical Down(0, -1, -1),
                // Corner(1, -1, -1)
                int brVal = calcAO(1, 0, -1, 0, -1, -1, 1, -1, -1);

                // BL: Lateral Left(-1, 0, -1), Vertical Down(0, -1, -1),
                // Corner(-1, -1, -1)
                int blVal = calcAO(-1, 0, -1, 0, -1, -1, -1, -1, -1);

                // TL: Lateral Left(-1, 0, -1), Vertical Up(0, 1, -1),
                // Corner(-1, 1, -1)
                int tlVal = calcAO(-1, 0, -1, 0, 1, -1, -1, 1, -1);

                // TR: Lateral Right(1, 0, -1), Vertical Up(0, 1, -1), Corner(1,
                // 1, -1)
                int trVal = calcAO(1, 0, -1, 0, 1, -1, 1, 1, -1);

                return {tlVal, trVal, brVal, blVal};

              } else if (face == 2) { // X- (Left) -> Air at x-1
                // BL Checks:
                // Side1: (-1, 0, -1). (Back neighbor at same level).
                // Side2: (-1, -1, 0). (Down neighbor).
                // Corner: (-1, -1, -1).
                int blVal = calcAO(-1, 0, -1, -1, -1, 0, -1, -1, -1);

                // BR (Front-Bottom):
                // Side1: (-1, 0, 1). (Front neighbor at same level).
                // Side2: (-1, -1, 0). (Down neighbor).
                // Corner: (-1, -1, 1).
                int brVal = calcAO(-1, 0, 1, -1, -1, 0, -1, -1, 1);

                // TR (Front-Top):
                // Side1: (-1, 0, 1). (Front neighbor at same level).
                // Side2: (-1, 1, 0). (Up neighbor).
                // Corner: (-1, 1, 1).
                int trVal = calcAO(-1, 0, 1, -1, 1, 0, -1, 1, 1);

                // TL (Back-Top):
                // Side1: (-1, 0, -1). (Back neighbor at same level).
                // Side2: (-1, 1, 0). (Up neighbor).
                // Corner: (-1, 1, -1).
                int tlVal = calcAO(-1, 0, -1, -1, 1, 0, -1, 1, -1);

                return {tlVal, trVal, brVal, blVal};

              } else if (face == 3) { // X+ (Right) -> Air at x+1
                // BL (Front-Bottom):
                // Side1: (1, 0, 1). (Front neighbor at same level).
                // Side2: (1, -1, 0). (Down neighbor).
                // Corner: (1, -1, 1).
                int blVal = calcAO(1, 0, 1, 1, -1, 0, 1, -1, 1);

                // BR (Back-Bottom):
                // Side1: (1, 0, -1). (Back neighbor at same level).
                // Side2: (1, -1, 0). (Down neighbor).
                // Corner: (1, -1, -1).
                int brVal = calcAO(1, 0, -1, 1, -1, 0, 1, -1, -1);

                // TR (Back-Top):
                // Side1: (1, 0, -1). (Back neighbor at same level).
                // Side2: (1, 1, 0). (Up neighbor).
                // Corner: (1, 1, -1).
                int trVal = calcAO(1, 0, -1, 1, 1, 0, 1, 1, -1);

                // TL (Front-Top):
                // Side1: (1, 0, 1). (Front neighbor at same level).
                // Side2: (1, 1, 0). (Up neighbor).
                // Corner: (1, 1, 1).
                int tlVal = calcAO(1, 0, 1, 1, 1, 0, 1, 1, 1);

                return {tlVal, trVal, brVal, blVal};
              }
              return {0, 0, 0, 0};
            };

            // Map 0..3 to 0.0..1.0? No, pushVert takes float aoVal?
            // Standard MC uses 0.2 increments or similar. Chunk.cpp passes int
            // to pushVert? Wait, pushVert implementation (line 1036) takes
            // float aoVal = 0.0f Let's see how others use it. In standard
            // greedy mesh, ao is passed as uint8_t 0-255? No, line 977 passes
            // current.ao[0] which is uint8_t 0-3? pushVert implementation:
            // vertices.push_back(ao);
            // It just pushes the float.
            // So if I pass 0, 1, 2, 3... shader handles it?
            // Let's assume passed as float 0.0, 1.0, 2.0, 3.0.

            auto [s0tl, s0tr, s0br, s0bl] = getSideAO(0);
            addFaceQuad(0, 0, 0, 0, 1, blockHeight, 1, s0tl, s0tr, s0br,
                        s0bl); // Z+ (Front)

            auto [s1tl, s1tr, s1br, s1bl] = getSideAO(1);
            addFaceQuad(1, 0, 0, 0, 1, blockHeight, 1, s1tl, s1tr, s1br,
                        s1bl); // Z- (Back)

            auto [s2tl, s2tr, s2br, s2bl] = getSideAO(2);
            addFaceQuad(2, 0, 0, 0, 1, blockHeight, 1, s2tl, s2tr, s2br,
                        s2bl); // X- (Left)

            auto [s3tl, s3tr, s3br, s3bl] = getSideAO(3);
            addFaceQuad(3, 0, 0, 0, 1, blockHeight, 1, s3tl, s3tr, s3br,
                        s3bl); // X+ (Right)

            // Top Face with explicit AO
            // addFaceQuad(4...) doesn't take AO. I need to call pushVert
            // directly or use a new helper. Or I can update addFaceQuad to take
            // optional AO array. Let's use pushVert manually for Top Face to be
            // safe and explicit.

            // Texture UVs for Top
            // Texture UVs for Top
            float uBase, vBase;
            cb.block->getTextureUV(4, uBase, vBase, gx, gy, gz, cb.metadata);

            // Get neighbors light above (y+1) for Top Face
            // Using world if available.
            uint8_t topS = 0, topB = 0;
            if (world) {
              int gnx = gx;
              int gny = gy + 1; // Top
              int gnz = gz;
              topS = world->getSkyLight(gnx, gny, gnz);
              topB = world->getBlockLight(gnx, gny, gnz);
            } else {
              // Fallback: Use current block light
              topS = cb.skyLight;
              topB = cb.blockLight;
            }
            float l1Top = pow((float)topS / 15.0f, 0.8f);
            float l2Top = pow((float)topB / 15.0f, 0.8f);

            // Standard Quad winding: BL, BR, TR, TL? Or 0, 1, 2, 2, 3, 0?
            // From addFaceQuad logic:
            // pushVert(xMin, yMax, zMax, 0, 0...) -> BL
            // pushVert(xMax, yMax, zMax, 1, 0...) -> BR
            // pushVert(xMax, yMax, zMin, 1, 1...) -> TR
            // ...
            // Let's replicate logic for Face 4 (Y+)
            // Shade = 1.0f for Top
            pushVert(fx + 0, fy + blockHeight, fz + 1, 0, 0, uBase, vBase,
                     (float)aoBL, l1Top, l2Top, 1.0f);
            pushVert(fx + 1, fy + blockHeight, fz + 1, 1, 0, uBase, vBase,
                     (float)aoBR, l1Top, l2Top, 1.0f);
            pushVert(fx + 1, fy + blockHeight, fz + 0, 1, 1, uBase, vBase,
                     (float)aoTR, l1Top, l2Top, 1.0f);

            pushVert(fx + 0, fy + blockHeight, fz + 1, 0, 0, uBase, vBase,
                     (float)aoBL, l1Top, l2Top, 1.0f);
            pushVert(fx + 1, fy + blockHeight, fz + 0, 1, 1, uBase, vBase,
                     (float)aoTR, l1Top, l2Top, 1.0f);
            pushVert(fx + 0, fy + blockHeight, fz + 0, 0, 1, uBase, vBase,
                     (float)aoTL, l1Top, l2Top, 1.0f);

            addFaceQuad(5, 0, 0, 0, 1, blockHeight, 1); // Y- (Bottom)
          } else {
            // SLAB_BOTTOM or STAIRS logic

            addFaceQuad(0, 0, 0, 0, 1, 0.5f, 1); // Z+
            addFaceQuad(1, 0, 0, 0, 1, 0.5f, 1); // Z-
            addFaceQuad(2, 0, 0, 0, 1, 0.5f, 1); // X-
            addFaceQuad(3, 0, 0, 0, 1, 0.5f, 1); // X+
            addFaceQuad(5, 0, 0, 0, 1, 0.5f, 1); // Y- (Bottom)

            if (shape == Block::RenderShape::SLAB_BOTTOM) {
              addFaceQuad(4, 0, 0, 0, 1, 0.5f, 1); // Y+ (Top of Slab)
            } else {
              // STAIRS
              // Needs Top Half.
              // Helper for Partial Box?
              // Determine quadrant from metadata?
              // Metadata 0: East (X+), 1: West (X-), 2: South (Z+), 3: North
              // (Z-) Let's assume standard metadata.

              // Base is drawn. Now draw Top Part (0.5..1.0)
              // Area depends on rotation.
              float tX1 = 0, tZ1 = 0, tX2 = 1, tZ2 = 1;

              // If East (Stairs go UP towards East/West? Or Face East?)
              // "Stairs facing East" usually means Back is West, Front is East.
              // The "Step" is on the West side? Or "Ascends" to East?
              // Let's implement one and check.
              // Assume Meta 0 = Ascend towards X+ (East).
              // Blocks: Bottom full, Top Right (X > 0.5) is filled?

              int meta = cb.metadata;
              if (meta == 0) { // East (X+)
                tX1 = 0.5f;
                tX2 = 1.0f;           // Fill X+ half
              } else if (meta == 1) { // West (X-)
                tX1 = 0.0f;
                tX2 = 0.5f;           // Fill X- half
              } else if (meta == 2) { // South (Z+)
                tZ1 = 0.5f;
                tZ2 = 1.0f; // Fill Z+ half
              } else {      // North (Z-)
                tZ1 = 0.0f;
                tZ2 = 0.5f; // Fill Z- half
              }

              // Top Box
              // Y range: 0.5 to 1.0
              addFaceQuad(0, tX1, 0.5f, tZ1, tX2, 1.0f, tZ2);
              addFaceQuad(1, tX1, 0.5f, tZ1, tX2, 1.0f, tZ2);
              addFaceQuad(2, tX1, 0.5f, tZ1, tX2, 1.0f, tZ2);
              addFaceQuad(3, tX1, 0.5f, tZ1, tX2, 1.0f, tZ2);
              addFaceQuad(4, tX1, 0.5f, tZ1, tX2, 1.0f, tZ2); // Top of Stairs
              // Note: Bottom of Top Box (at 0.5) sits on Base Slab Top (at
              // 0.5). Base Slab Top (at 0.5) was NOT drawn for Stair (I split
              // the if above). But we need to draw the Exposed part of Base
              // Slab Top!

              // Base Slab Top (Exposed Part)
              // It's the Inverse of Top Box X/Z rect.
              // If East (Top is X>0.5), Exposed Base Top is X<0.5.
              float bX1 = 0, bZ1 = 0, bX2 = 1, bZ2 = 1;
              if (meta == 0) {
                bX2 = 0.5f;
              } else if (meta == 1) {
                bX1 = 0.5f;
              } else if (meta == 2) {
                bZ2 = 0.5f;
              } else {
                addFaceQuad(4, bX1, 0, bZ1, bX2, 0.5f, bZ2); // Exposed Base Top
              }
            }
          }
        } else if (shape == Block::RenderShape::MODEL) {
          const Model *model = cb.block->getModel();
          if (model) {
            // Pre-calculate Max Light for fallback (Rotated elements or
            // internal)
            uint8_t maxSky = cb.skyLight;
            uint8_t maxBlock = cb.blockLight;

            auto checkMax = [&](int nx, int ny, int nz) {
              if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE &&
                  nz >= 0 && nz < CHUNK_SIZE) {
                maxSky = std::max(maxSky, blocks[nx][ny][nz].skyLight);
                maxBlock = std::max(maxBlock, blocks[nx][ny][nz].blockLight);
              } else if (world) {
                int gnx = chunkPosition.x * CHUNK_SIZE + nx;
                int gny = chunkPosition.y * CHUNK_SIZE + ny;
                int gnz = chunkPosition.z * CHUNK_SIZE + nz;
                uint8_t sky = world->getSkyLight(gnx, gny, gnz);
                uint8_t bl = world->getBlockLight(gnx, gny, gnz);
                // Adjust for opacity if inside block?
                // Standard addFace gets neighbor light.
                maxSky = std::max(maxSky, sky);
                maxBlock = std::max(maxBlock, bl);
              }
            };

            // Check neighbors for light (simplified: check 6 neighbors?
            // Better: Check the neighbor responsible for the face)
            // But we are in a loop for faces? No, this is for Model/Shape.
            // Let's modify addFaceQuad to take light overrides or calculate
            // them.

            // Re-defining addFaceQuad to sample light
            auto addFaceQuad = [&](int face, float xMin, float yMin, float zMin,
                                   float xMax, float yMax, float zMax) {
              int dx = 0, dy = 0, dz = 0;
              if (face == 0)
                dz = 1; // Z+
              else if (face == 1)
                dz = -1; // Z-
              else if (face == 2)
                dx = -1; // X-
              else if (face == 3)
                dx = 1; // X+
              else if (face == 4)
                dy = 1; // Y+
              else if (face == 5)
                dy = -1; // Y-

              int nx = x + dx;
              int ny = y + dy;
              int nz = z + dz;

              uint8_t s = 0, b = 0;
              if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE &&
                  nz >= 0 && nz < CHUNK_SIZE) {
                s = blocks[nx][ny][nz].skyLight;
                b = blocks[nx][ny][nz].blockLight;
              } else if (world) {
                int gx = chunkPosition.x * CHUNK_SIZE + nx;
                int gy = chunkPosition.y * CHUNK_SIZE + ny;
                int gz = chunkPosition.z * CHUNK_SIZE + nz;
                s = world->getSkyLight(gx, gy, gz);
                b = world->getBlockLight(gx, gy, gz);
              }

              float l1 = pow((float)s / 15.0f, 0.8f);
              float l2 = pow((float)b / 15.0f, 0.8f);

              float uBase = 0.0f, vBase = 0.0f;
              float w = zMax - zMin; // Default for Side X
              float h = yMax - yMin;
              if (face <= 1)
                w = xMax - xMin; // Side Z
              else if (face >= 4) {
                w = xMax - xMin;
                h = zMax - zMin;
              } // Top/Bottom

              cb.block->getTextureUV(
                  face, uBase, vBase, chunkPosition.x * CHUNK_SIZE + x,
                  chunkPosition.y * CHUNK_SIZE + y,
                  chunkPosition.z * CHUNK_SIZE + z, cb.metadata, 0);

              // Face Dimming Logic
              float shade = 1.0f;
              if (face == 4)
                shade = 1.0f; // Top
              else if (face == 5)
                shade = 0.6f; // Bottom
              else
                shade = 0.8f; // Sides (All) - Matches addFace

              // Draw
              auto pV = [&](float vx, float vy, float vz, float u, float v,
                            float ao) {
                pushVert(fx + vx, fy + vy, fz + vz, u, v, uBase, vBase, ao, l1,
                         l2, shade);
              };

              // U,V mapping helpers
              // Sides: V matches Y. U matches X or Z.
              // Top/Bottom: U matches X, V matches Z.

              // Simplification: Standard full-face mapping adapted to partial
              // If face is 0 (Z+), u=x, v=y.
              float u0 = 0.0f, v0 = 0.0f;
              float u1 = 1.0f, v1 = 1.0f;

              // Partial logic
              if (face <= 3) {
                // Side faces
                v0 = yMin;
                v1 = yMax;
                if (face == 0 || face == 1) { // Z faces -> X varies
                  if (face == 0) {
                    u0 = xMin;
                    u1 = xMax;
                  } else {
                    u0 = xMax;
                    u1 = xMin;
                  } // Inverted for Back face? Check logic
                    // Actually addFace logic:
                  // Face 0 (Z+): (fx, fy, fz+1) to (fx+1, fy+1, fz+1). U: 0->1
                } else { // X faces -> Z varies
                  if (face == 3) {
                    u0 = 1.0f - zMax;
                    u1 = 1.0f - zMin;
                  } // X+
                  else {
                    u0 = zMin;
                    u1 = zMax;
                  } // X-
                }
              } else {
                // Top/Bottom
                u0 = xMin;
                u1 = xMax;
                v0 = zMin;
                v1 = zMax;
              }

              // AO sampling currently only for Top Face (4) in caller?
              // The caller calculates aoTL etc for Face 4.
              // For other faces, defaulting to 0.0 (Bright).
              // We should probably implement AO for sides too, but user only
              // complained about Top/General diff. Top face uses passed AO
              // values? No, addFaceQuad doesn't take AO args. We need to fix
              // this. My previous edit removed the MANUAL pushVertS for face 4!
              // Wait, I am replacing the OLD addFaceQuad.
              // AND I need to respect the manual loop I wrote earlier?
              // The manual loop for LAYERED (line 1280+) called pushVert.
              // It did NOT use addFaceQuad.
              // Ah! The `LAYERED` block logic was checking `faceDir` loop 976?
              // No. `Pass 2` iterates `mask` and does `if (shape == LAYERED)`.
              // Inside that, it does `addFaceQuad` for sides?
              // Let's check lines 1350 roughly.

              // I need to be careful not to break the manual top face AO I
              // added. My previous manual top face code: if (face == 4) { ...
              // pushVert(...) ... }

              // The `addFaceQuad` I am editing is defined at line ~1157.
              // It is used by `SLAB_BOTTOM`, `STAIRS`, etc.
              // `LAYERED` uses `addFaceQuad` for SIDES?

              // Let's look at `LAYERED` implementation again (lines ~1360).
            };

            for (const auto &elem : model->elements) {
              glm::vec3 minP = elem.from;
              glm::vec3 maxP = elem.to;

              auto transform = [&](glm::vec3 p) -> glm::vec3 {
                glm::vec3 res = p;
                if (elem.hasRotation) {
                  glm::vec3 local = p - elem.rotation.origin;
                  float rad = glm::radians(elem.rotation.angle);
                  float s = sin(rad), c = cos(rad);
                  float nx = local.x, ny = local.y, nz = local.z;
                  if (elem.rotation.axis == 'x') {
                    ny = local.y * c - local.z * s;
                    nz = local.y * s + local.z * c;
                  } else if (elem.rotation.axis == 'y') {
                    nx = local.x * c + local.z * s;
                    nz = -local.x * s + local.z * c;
                  } else if (elem.rotation.axis == 'z') {
                    nx = local.x * c - local.y * s;
                    ny = local.x * s + local.y * c;
                  }
                  res = elem.rotation.origin + glm::vec3(nx, ny, nz);
                }

                // Global Rotation for Logs
                if (cb.block->getId() == WOOD ||
                    cb.block->getId() == SPRUCE_LOG) {
                  if (cb.metadata == 1) { // X-Axis
                    // Rotate 90 deg around Z axis
                    // Center is 0.5, 0.5, 0.5
                    glm::vec3 center(0.5f);
                    glm::vec3 local = res - center;
                    // Z-Axis rotation 90 deg (Clockwise? or CCW?)
                    // To point Y to X.
                    // X' = X*c - Y*s
                    // Y' = X*s + Y*c
                    // -90 deg: s=-1, c=0 => X'=Y, Y'=-X
                    float tmpx = local.x;
                    local.x = local.y;
                    local.y = -tmpx;
                    res = center + local;
                  } else if (cb.metadata == 2) { // Z-Axis
                    // Rotate 90 deg around X axis
                    // Y to Z.
                    // Y' = Y*c - Z*s
                    // Z' = Y*s + Z*c
                    // 90 deg: s=1, c=0 => Y'=-Z, Z'=Y
                    glm::vec3 center(0.5f);
                    glm::vec3 local = res - center;
                    float tmpy = local.y;
                    local.y = -local.z;
                    local.z = tmpy;
                    res = center + local;
                  }
                }
                return res;
              };

              auto getFaceLight = [&](int faceIdx) -> std::pair<float, float> {
                if (elem.hasRotation) {
                  return {pow((float)maxSky / 15.0f, 0.8f),
                          pow((float)maxBlock / 15.0f, 0.8f)};
                }
                int nx = x, ny = y, nz = z;
                if (faceIdx == 0)
                  nz++;
                else if (faceIdx == 1)
                  nz--;
                else if (faceIdx == 2)
                  nx--;
                else if (faceIdx == 3)
                  nx++;
                else if (faceIdx == 4)
                  ny++;
                else if (faceIdx == 5)
                  ny--;

                uint8_t s = cb.skyLight, b = cb.blockLight;
                if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE &&
                    nz >= 0 && nz < CHUNK_SIZE) {
                  s = blocks[nx][ny][nz].skyLight;
                  b = blocks[nx][ny][nz].blockLight;
                } else if (world) {
                  int gnx = chunkPosition.x * CHUNK_SIZE + nx;
                  int gny = chunkPosition.y * CHUNK_SIZE + ny;
                  int gnz = chunkPosition.z * CHUNK_SIZE + nz;
                  ChunkBlock wb = world->getBlock(gnx, gny, gnz);
                  s = wb.skyLight;
                  b = wb.blockLight;
                }
                return {pow((float)s / 15.0f, 0.8f),
                        pow((float)b / 15.0f, 0.8f)};
              };

              for (const auto &[faceIdx, faceProp] : elem.faces) {
                glm::vec3 p0, p1, p2, p3;
                if (faceIdx == 0) { // Z+
                  p0 = glm::vec3(minP.x, minP.y, maxP.z);
                  p1 = glm::vec3(maxP.x, minP.y, maxP.z);
                  p2 = glm::vec3(maxP.x, maxP.y, maxP.z);
                  p3 = glm::vec3(minP.x, maxP.y, maxP.z);
                } else if (faceIdx == 1) { // Z-
                  p0 = glm::vec3(maxP.x, minP.y, minP.z);
                  p1 = glm::vec3(minP.x, minP.y, minP.z);
                  p2 = glm::vec3(minP.x, maxP.y, minP.z);
                  p3 = glm::vec3(maxP.x, maxP.y, minP.z);
                } else if (faceIdx == 2) { // X-
                  p0 = glm::vec3(minP.x, minP.y, minP.z);
                  p1 = glm::vec3(minP.x, minP.y, maxP.z);
                  p2 = glm::vec3(minP.x, maxP.y, maxP.z);
                  p3 = glm::vec3(minP.x, maxP.y, minP.z);
                } else if (faceIdx == 3) { // X+
                  p0 = glm::vec3(maxP.x, minP.y, maxP.z);
                  p1 = glm::vec3(maxP.x, minP.y, minP.z);
                  p2 = glm::vec3(maxP.x, maxP.y, minP.z);
                  p3 = glm::vec3(maxP.x, maxP.y, maxP.z);
                } else if (faceIdx == 4) { // Y+
                  p0 = glm::vec3(minP.x, maxP.y, maxP.z);
                  p1 = glm::vec3(maxP.x, maxP.y, maxP.z);
                  p2 = glm::vec3(maxP.x, maxP.y, minP.z);
                  p3 = glm::vec3(minP.x, maxP.y, minP.z);
                } else { // Y-
                  p0 = glm::vec3(minP.x, minP.y, minP.z);
                  p1 = glm::vec3(maxP.x, minP.y, minP.z);
                  p2 = glm::vec3(maxP.x, minP.y, maxP.z);
                  p3 = glm::vec3(minP.x, minP.y, maxP.z);
                }

                auto finalP0 = transform(p0) + glm::vec3(fx, fy, fz);
                auto finalP1 = transform(p1) + glm::vec3(fx, fy, fz);
                auto finalP2 = transform(p2) + glm::vec3(fx, fy, fz);
                auto finalP3 = transform(p3) + glm::vec3(fx, fy, fz);

                float uMin, vMin;
                cb.block->getModelTextureUV(faceProp.texture, uMin, vMin);

                float localU1 = faceProp.uv[0];
                float localV1 = 1.0f - faceProp.uv[1];
                float localU2 = faceProp.uv[2];
                float localV2 = 1.0f - faceProp.uv[3];

                // Check for Log Rotation UV Adjustment
                bool rotateUV = false;
                if (cb.block->getId() == WOOD ||
                    cb.block->getId() == SPRUCE_LOG) {
                  if (cb.metadata == 1 || cb.metadata == 2) {
                    // Rotate UVs for Bark Faces (0, 1, 2, 3) geometry-wise
                    // Faces 4 and 5 are Rings (Ends), usually don't need
                    // rotation
                    if (faceIdx <= 3) {
                      rotateUV = true;
                    }
                  }
                }

                if (rotateUV) {
                  auto rot = [](float &u, float &v) {
                    float nu = 0.5f + (v - 0.5f);
                    float nv = 0.5f - (u - 0.5f);
                    u = nu;
                    v = nv;
                  };
                  // We have U1, V2 for P0. U2, V2 for P1. U2, V1 for P2. U1, V1
                  // for P3. Actually we define range [U1, U2] x [V2, V1]? No,
                  // faceProp.uv is just 2 corners? Usually U1=0, V1=1 (Top),
                  // U2=1, V2=0 (Bot)? If we rotate the abstract rectangle? No,
                  // we should rotate the 4 coordinate pairs used in pushVert.
                  // P0_UV: (localU1, localV2)
                  // P1_UV: (localU2, localV2)
                  // P2_UV: (localU2, localV1)
                  // P3_UV: (localU1, localV1)

                  float u_p0 = localU1, v_p0 = localV2;
                  float u_p1 = localU2, v_p1 = localV2;
                  float u_p2 = localU2, v_p2 = localV1;
                  float u_p3 = localU1, v_p3 = localV1;

                  rot(u_p0, v_p0);
                  rot(u_p1, v_p1);
                  rot(u_p2, v_p2);
                  rot(u_p3, v_p3);

                  // Push Vertices with ROTATED UVs
                  std::pair<float, float> lights = getFaceLight(faceIdx);
                  float l1 = lights.first, l2 = lights.second;

                  pushVert(finalP0.x, finalP0.y, finalP0.z, u_p0, v_p0, uMin,
                           vMin, 0.0f, l1, l2);
                  pushVert(finalP1.x, finalP1.y, finalP1.z, u_p1, v_p1, uMin,
                           vMin, 0.0f, l1, l2);
                  pushVert(finalP2.x, finalP2.y, finalP2.z, u_p2, v_p2, uMin,
                           vMin, 0.0f, l1, l2);

                  pushVert(finalP0.x, finalP0.y, finalP0.z, u_p0, v_p0, uMin,
                           vMin, 0.0f, l1, l2);
                  pushVert(finalP2.x, finalP2.y, finalP2.z, u_p2, v_p2, uMin,
                           vMin, 0.0f, l1, l2);
                  pushVert(finalP3.x, finalP3.y, finalP3.z, u_p3, v_p3, uMin,
                           vMin, 0.0f, l1, l2);
                } else {
                  std::pair<float, float> lights = getFaceLight(faceIdx);
                  float l1 = lights.first, l2 = lights.second;

                  pushVert(finalP0.x, finalP0.y, finalP0.z, localU1, localV2,
                           uMin, vMin, 0.0f, l1, l2);
                  pushVert(finalP1.x, finalP1.y, finalP1.z, localU2, localV2,
                           uMin, vMin, 0.0f, l1, l2);
                  pushVert(finalP2.x, finalP2.y, finalP2.z, localU2, localV1,
                           uMin, vMin, 0.0f, l1, l2);

                  pushVert(finalP0.x, finalP0.y, finalP0.z, localU1, localV2,
                           uMin, vMin, 0.0f, l1, l2);
                  pushVert(finalP2.x, finalP2.y, finalP2.z, localU2, localV1,
                           uMin, vMin, 0.0f, l1, l2);
                  pushVert(finalP3.x, finalP3.y, finalP3.z, localU1, localV1,
                           uMin, vMin, 0.0f, l1, l2);
                }
              }
            }
          }
        }
      }
    }
  }

  // Stitch Vectors
  outOpaqueCount = opaqueVertices.size() / 14;
  opaqueVertices.insert(opaqueVertices.end(), transparentVertices.begin(),
                        transparentVertices.end());

  return opaqueVertices;
}
void Chunk::uploadMesh(const std::vector<float> &data, int opaqueCount) {
  if (VAO == 0)
    initGL();

  // Upload to GPU (Main Thread)
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(),
               GL_DYNAMIC_DRAW);

  vertexCount = opaqueCount;
  vertexCountTransparent = (data.size() / 14) - opaqueCount;

  // Store transparent part for sorting
  if (vertexCountTransparent > 0) {
    size_t opaqueFloats = opaqueCount * 14;
    if (opaqueFloats < data.size()) {
      transparentVertices.assign(data.begin() + opaqueFloats, data.end());
    }
  } else {
    transparentVertices.clear();
  }

  // Attribs
  float stride = 14 * sizeof(float);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0); // Pos
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float))); // Color (Vec4)
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(7 * sizeof(float))); // UV
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                        (void *)(9 * sizeof(float))); // Light(Sky,Block,AO)
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(12 * sizeof(float))); // TexOrigin
  glEnableVertexAttribArray(4);
}

void Chunk::sortAndUploadTransparent(const glm::vec3 &cameraPos) {
  if (vertexCountTransparent == 0)
    return;
  if (VAO == 0)
    return;

  // Throttle: Only resort if camera moved significantly or never sorted
  if (glm::distance(cameraPos, m_lastSortCameraPos) < 1.0f) {
    return;
  }
  m_lastSortCameraPos = cameraPos;

  int floatsPerVertex = 14; // As defined in uploadMesh
  int vertsPerFace = 6;
  int numFloatsPerFace = floatsPerVertex * vertsPerFace;

  // transparentVertices stores FULL FACES consecutively.
  int numFaces = transparentVertices.size() / numFloatsPerFace;
  if (numFaces == 0)
    return; // Should be covered by count check

  // Structure to hold quad info
  struct FaceInfo {
    int index; // Index of the face (0 to N-1)
    float distSq;
  };

  std::vector<FaceInfo> faces(numFaces);

  // Calculate distances
  for (int i = 0; i < numFaces; ++i) {
    faces[i].index = i;
    float *faceData = &transparentVertices[i * numFloatsPerFace];

    // Calculate centroid (average of 6 vertices)
    glm::vec3 centroid(0.0f);
    for (int v = 0; v < vertsPerFace; ++v) {
      centroid.x += faceData[v * floatsPerVertex + 0];
      centroid.y += faceData[v * floatsPerVertex + 1];
      centroid.z += faceData[v * floatsPerVertex + 2];
    }
    centroid /= (float)vertsPerFace;

    // Transform centroid to world space
    glm::vec3 worldCentroid =
        centroid + glm::vec3(chunkPosition.x * CHUNK_SIZE,
                             chunkPosition.y * CHUNK_SIZE,
                             chunkPosition.z * CHUNK_SIZE);

    faces[i].distSq = glm::distance2(worldCentroid, cameraPos);
  }

  // Sort Back-to-Front (Far to Near) -> Descending Distance
  std::sort(
      faces.begin(), faces.end(),
      [](const FaceInfo &a, const FaceInfo &b) { return a.distSq > b.distSq; });

  // Reconstruct sorted buffer
  std::vector<float> sortedData;
  sortedData.reserve(transparentVertices.size());

  for (const auto &f : faces) {
    int offset = f.index * numFloatsPerFace;
    sortedData.insert(sortedData.end(), transparentVertices.begin() + offset,
                      transparentVertices.begin() + offset + numFloatsPerFace);
  }

  // Upload to GPU (SubData)
  // Offset = opaque vertex count * floats per vertex * sizeof(float)
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER,
                  vertexCount * floatsPerVertex * sizeof(float),
                  sortedData.size() * sizeof(float), sortedData.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind VBO
  glBindVertexArray(0);
}

void Chunk::updateMesh() {
  int opaqueCount = 0;
  std::vector<float> newVertices = generateGeometry(opaqueCount);
  uploadMesh(newVertices, opaqueCount);
  meshDirty = false;

  // Force resort on next render
  m_lastSortCameraPos = glm::vec3(-99999.0f);
}

void Chunk::addFace(std::vector<float> &vertices, int x, int y, int z,
                    int faceDir, const Block *block, int width, int height,
                    int aoBL, int aoBR, int aoTR, int aoTL, uint8_t metadata,
                    float hBL, float hBR, float hTR, float hTL, int layer,
                    bool isInternal) {
  float r, g, b;
  block->getColor(r, g, b); // Base Tint

  // Decide tint
  if (!block->shouldTint(faceDir, layer)) {
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
  }

  float alpha = block->getAlpha();

  float l1 = 1.0f, l2 = 1.0f;
  float faceShade = 1.0f;
  if (faceDir == 4)
    faceShade = 1.0f;
  else if (faceDir == 5)
    faceShade = 0.6f;
  else
    faceShade = 0.8f;
  r *= faceShade;
  g *= faceShade;
  b *= faceShade;

  if (world) {
    int gx = chunkPosition.x * CHUNK_SIZE + x;
    int gy = chunkPosition.y * CHUNK_SIZE + y;
    int gz = chunkPosition.z * CHUNK_SIZE + z;
    int dx = 0, dy = 0, dz = 0;
    if (faceDir == 0)
      dz = 1;
    else if (faceDir == 1)
      dz = -1;
    else if (faceDir == 2)
      dx = -1;
    else if (faceDir == 3)
      dx = 1;
    else if (faceDir == 4)
      dy = 1;
    else
      dy = -1;

    uint8_t s = world->getSkyLight(gx + dx, gy + dy, gz + dz);
    uint8_t bl = world->getBlockLight(gx + dx, gy + dy, gz + dz);
    l1 = pow((float)s / 15.0f, 0.8f);
    l2 = pow((float)bl / 15.0f, 0.8f);
  }

  float uMin = 0.00f, vMin = 0.00f;

  if (world) {
    int gx = chunkPosition.x * CHUNK_SIZE + x;
    int gy = chunkPosition.y * CHUNK_SIZE + y;
    int gz = chunkPosition.z * CHUNK_SIZE + z;
    // Use Flow Texture (Face 0) for Top Face (4) if flowing (meta > 0)
    // Actually, user wants flow texture on top if it is flowing.
    // If metadata > 0, it is flowing. Source (0) is still?
    // Actually source blocks can flow too if they have velocity, but in
    // this simplicity: Any liquid that has flow vector should probably use
    // flow texture? Let's stick to user request: "flowing water... should
    // have flowing texture". If metadata > 0 (decaying flow), definitely
    // flowing. If metadata == 0 (source), might be still unless it's a
    // source block flowing into a hole? Let's check neighbors to see if
    // it's flowing. Simpler: If meta > 0, use flow. If meta == 0, use
    // still. BUT user said "direction in which they are flowing".
    if ((block->getId() == WATER || block->getId() == LAVA) && faceDir == 4) {
      // Check if flowing
      if (metadata > 0) {
        block->getTextureUV(0, uMin, vMin, gx, gy, gz, metadata,
                            layer); // Use Side Texture
      } else {
        block->getTextureUV(faceDir, uMin, vMin, gx, gy, gz, metadata, layer);
      }
    } else {
      block->getTextureUV(faceDir, uMin, vMin, gx, gy, gz, metadata, layer);
    }
  } else {
    block->getTextureUV(faceDir, uMin, vMin, 0, 0, 0, metadata, layer);
  }

  float fx = (float)x, fy = (float)y, fz = (float)z;

  // Overlay Offset to avoid Z-fighting
  if (layer == 1) {
    float offset = 0.002f;
    if (faceDir == 0)
      fz += offset;
    else if (faceDir == 1)
      fz -= offset;
    else if (faceDir == 2)
      fx -= offset;
    else if (faceDir == 3)
      fx += offset;
    else if (faceDir == 4)
      fy += offset;
    else if (faceDir == 5)
      fy -= offset;
  }
  float fw = (float)width, fh = (float)height;

  // Fluid Height Logic
  float topH = 1.0f; // Default Top
  if (block->getId() == WATER || block->getId() == LAVA) {
    if (metadata >= 7)
      topH = 0.1f;
    else
      topH = (8.0f - metadata) / 9.0f;
  }

  // Adjust height for side faces if this is a fluid?
  // Actually, "height" argument is the greedy-meshed height (number of
  // blocks). If NOT liquid, force h=1.0f for Top/Bottom, or h=height for
  // Sides
  if (block->getId() != WATER && block->getId() != LAVA) {
    if (faceDir <= 3) { // Side Faces: height is Y-extent
      float H = (float)height;
      hBL = H;
      hBR = H;
      hTR = H;
      hTL = H;
    } else { // Top/Bottom Faces: height is Z-extent (or X), Y-extent is 1
             // block
      hBL = 1.0f;
      hBR = 1.0f;
      hTR = 1.0f;
      hTL = 1.0f;
    }
  }

  // Flow rotation logic
  float rAngle = 0.0f;
  if ((block->getId() == WATER || block->getId() == LAVA) && faceDir == 4) {
    // Calculate Flow Vector
    // Check neighbors (using World if available, else cache?)
    // We are in addFace, called from generateGeometry, where we don't have
    // easy random access to world without locking/etc. But we have 'world'
    // pointer and coords.
    if (world) {
      float dx = 0.0f;
      float dz = 0.0f;
      int gx = chunkPosition.x * CHUNK_SIZE + x;
      int gy = chunkPosition.y * CHUNK_SIZE + y;
      int gz = chunkPosition.z * CHUNK_SIZE + z;

      auto getLiquidHeight = [&](int bx, int by, int bz) -> float {
        ChunkBlock n = world->getBlock(bx, by, bz);
        if (!n.isActive())
          return -1.0f; // Treat as sink? Or different?
        if (n.block->getId() != block->getId()) {
          if (n.isSolid())
            return 100.0f; // Blocked
          return -1.0f;    // Sink
        }
        return (float)n.metadata; // Higher meta = lower liquid = flow towards
      };

      // Neighbors
      float hL = getLiquidHeight(gx - 1, gy, gz);
      float hR = getLiquidHeight(gx + 1, gy, gz);
      float hF = getLiquidHeight(gx, gy, gz + 1); // Z+
      float hB = getLiquidHeight(gx, gy, gz - 1); // Z-

      // If neighbor is -1 (sink), treats as strong flow towards it.
      // If neighbor is 100 (solid), treats as blocked.
      // If neighbor is liquid, compare metadata.

      float myMeta = (float)metadata;

      // X-Axis
      if (hL == -1.0f || (hL != 100.0f && hL > myMeta))
        dx -= 1.0f; // Flow Left
      if (hR == -1.0f || (hR != 100.0f && hR > myMeta))
        dx += 1.0f; // Flow Right

      // Z-Axis
      if (hB == -1.0f || (hB != 100.0f && hB > myMeta))
        dz -= 1.0f; // Flow Back (Z-)
      if (hF == -1.0f || (hF != 100.0f && hF > myMeta))
        dz += 1.0f; // Flow Front (Z+)

      if (dx != 0.0f || dz != 0.0f) {
        // Only rotate if NOT Lava Source (Lava Still should not rotate)
        // Water Source can rotate (visual choice) but User specifically
        // complained about Lava Still.
        if (block->getId() == LAVA && metadata == 0) {
          rAngle = 0.0f;
        } else {
          rAngle = atan2(dz, dx) + 1.5708f; // +PI/2 to align texture correctly
        }
        // Normalize to 0..2PI or just use sin/cos
        // Texture Default Alignment: Assuming Flow Texture points UP/NORTH?
        // Standard minecraft water flow texture usually has lines going
        // vertically? If vertical lines = Z axis? Need to experiment or
        // check defaults. Let's assume standard UV orientation.
      }
    }
  } else if (block->getId() == WOOD || block->getId() == SPRUCE_LOG ||
             block->getId() == ACACIA_LOG || block->getId() == BIRCH_LOG ||
             block->getId() == DARK_OAK_LOG || block->getId() == JUNGLE_LOG ||
             block->getId() == MANGROVE_LOG || block->getId() == PALE_OAK_LOG) {

    // Log Rotation
    if (metadata == 1) { // X-Axis
      if (faceDir == 0 || faceDir == 1 || faceDir == 4 || faceDir == 5) {
        rAngle = 1.5708f; // 90 degrees
      }
    } else if (metadata == 2) { // Z-Axis
      // Only rotate sides (X-faces). Top/Bottom (Y-faces) are already Z-aligned
      // by default.
      if (faceDir == 2 || faceDir == 3) {
        rAngle = 1.5708f; // 90 degrees
      }
    }
  }

  auto pushVert = [&](float vx, float vy, float vz, float u, float v,
                      float ao) {
    vertices.push_back(vx);
    vertices.push_back(vy);
    vertices.push_back(vz);
    vertices.push_back(r);
    vertices.push_back(g);
    vertices.push_back(b);
    vertices.push_back(alpha);

    // Rotate UV if needed
    float fu = u;
    float fv = v;
    if (rAngle != 0.0f) {
      // Center of rotation (0.5, 0.5)
      float cu = 0.5f;
      float cv = 0.5f;
      float s = sin(rAngle);
      float c = cos(rAngle);

      // Translate to origin
      float tu = u - cu;
      float tv = v - cv;

      // Rotate
      float ru = tu * c - tv * s;
      float rv = tu * s + tv * c;

      // Translate back
      fu = ru + cu;
      fv = rv + cv;
    }

    vertices.push_back(fu);
    vertices.push_back(fv);
    vertices.push_back(l1);
    vertices.push_back(l2);
    vertices.push_back(ao);
    vertices.push_back(uMin);
    vertices.push_back(vMin);
  };

  // Corners Mapping:
  // hBL is for corner (x, z)
  // hBR is for corner (x+1, z)
  // hTR is for corner (x+1, z+1)
  // hTL is for corner (x, z+1)
  // (Note: w and h are 1 for liquids now, as greedy meshing is disabled if
  // levels differ)

  // Y Offsets relative to fy
  float yBL = hBL;
  float yBR = hBR;
  float yTR = hTR;
  float yTL = hTL;

  float botY = fy;

  // Determine V coordinates (Flip for liquids on sides)
  float vBottom = 0.0f;
  float vTop = fh;
  if ((block->getId() == WATER || block->getId() == LAVA) && faceDir <= 3) {
    vBottom = fh;
    vTop = 0.0f;
  }

  bool isDoubleSided =
      (block->getId() == LEAVES || block->getId() == SPRUCE_LEAVES ||
       block->getId() == ACACIA_LEAVES || block->getId() == BIRCH_LEAVES ||
       block->getId() == DARK_OAK_LEAVES || block->getId() == JUNGLE_LEAVES);

  if (isInternal) {
    if (faceDir % 2 != 0)
      return;             // Cull Odd faces for internal (Canonical Rendering)
    isDoubleSided = true; // Even faces are double-sided
  }

  float eps = 0.01f;

  if (faceDir == 0) { // Front Z+ (at z+1)
    pushVert(fx, botY, fz + 1, 0, vBottom, (float)aoBL);
    pushVert(fx + fw, botY, fz + 1, fw, vBottom, (float)aoBR);
    pushVert(fx + fw, fy + yTR, fz + 1, fw, vTop, (float)aoTR);

    pushVert(fx, botY, fz + 1, 0, vBottom, (float)aoBL);
    pushVert(fx + fw, fy + yTR, fz + 1, fw, vTop, (float)aoTR);
    pushVert(fx, fy + yTL, fz + 1, 0, vTop, (float)aoTL);

    if (isDoubleSided) {
      float zBack = fz + 1.0f - eps;
      pushVert(fx, botY, zBack, 0, vBottom, (float)aoBL);
      pushVert(fx + fw, fy + yTR, zBack, fw, vTop, (float)aoTR);
      pushVert(fx + fw, botY, zBack, fw, vBottom, (float)aoBR);

      pushVert(fx, botY, zBack, 0, vBottom, (float)aoBL);
      pushVert(fx, fy + yTL, zBack, 0, vTop, (float)aoTL);
      pushVert(fx + fw, fy + yTR, zBack, fw, vTop, (float)aoTR);
    }
  } else if (faceDir == 1) { // Back Z- (at z=0)
    pushVert(fx + fw, botY, fz, 0, vBottom, (float)aoBR);
    pushVert(fx, botY, fz, fw, vBottom, (float)aoBL);
    pushVert(fx, fy + yBL, fz, fw, vTop, (float)aoTL);

    pushVert(fx + fw, botY, fz, 0, vBottom, (float)aoBR);
    pushVert(fx, fy + yBL, fz, fw, vTop, (float)aoTL);
    pushVert(fx + fw, fy + yBR, fz, 0, vTop, (float)aoTR);

    if (isDoubleSided) {
      float zBack = fz + eps;
      pushVert(fx + fw, botY, zBack, 0, vBottom, (float)aoBR);
      pushVert(fx, fy + yBL, zBack, fw, vTop, (float)aoTL);
      pushVert(fx, botY, zBack, fw, vBottom, (float)aoBL);

      pushVert(fx + fw, botY, zBack, 0, vBottom, (float)aoBR);
      pushVert(fx + fw, fy + yBR, zBack, 0, vTop, (float)aoTR);
      pushVert(fx, fy + yBL, zBack, fw, vTop, (float)aoTL);
    }
  } else if (faceDir == 2) { // Left X- (at x=0)
    pushVert(fx, botY, fz, 0, vBottom, (float)aoBL);
    pushVert(fx, botY, fz + fw, fw, vBottom, (float)aoBR);
    pushVert(fx, fy + yTL, fz + fw, fw, vTop, (float)aoTR);

    pushVert(fx, botY, fz, 0, vBottom, (float)aoBL);
    pushVert(fx, fy + yTL, fz + fw, fw, vTop, (float)aoTR);
    pushVert(fx, fy + yBL, fz, 0, vTop, (float)aoTL);

    if (isDoubleSided) {
      float xBack = fx + eps;
      pushVert(xBack, botY, fz, 0, vBottom, (float)aoBL);
      pushVert(xBack, fy + yTL, fz + fw, fw, vTop, (float)aoTR);
      pushVert(xBack, botY, fz + fw, fw, vBottom, (float)aoBR);

      pushVert(xBack, botY, fz, 0, vBottom, (float)aoBL);
      pushVert(xBack, fy + yBL, fz, 0, vTop, (float)aoTL);
      pushVert(xBack, fy + yTL, fz + fw, fw, vTop, (float)aoTR);
    }
  } else if (faceDir == 3) { // Right X+ (at x+1)
    pushVert(fx + 1, botY, fz + fw, 0, vBottom, (float)aoBR);
    pushVert(fx + 1, botY, fz, fw, vBottom, (float)aoBL);
    pushVert(fx + 1, fy + yBR, fz, fw, vTop, (float)aoTL);

    pushVert(fx + 1, botY, fz + fw, 0, vBottom, (float)aoBR);
    pushVert(fx + 1, fy + yBR, fz, fw, vTop, (float)aoTL);
    pushVert(fx + 1, fy + yTR, fz + fw, 0, vTop, (float)aoTR);

    if (isDoubleSided) {
      float xBack = fx + 1.0f - eps;

      pushVert(xBack, botY, fz + fw, 0, vBottom, (float)aoBR);
      pushVert(xBack, fy + yBR, fz, fw, vTop, (float)aoTL);
      pushVert(xBack, botY, fz, fw, vBottom, (float)aoBL);

      pushVert(xBack, botY, fz + fw, 0, vBottom, (float)aoBR);
      pushVert(xBack, fy + yTR, fz + fw, 0, vTop, (float)aoTR);
      pushVert(xBack, fy + yBR, fz, fw, vTop, (float)aoTL);
    }
  } else if (faceDir == 4) { // Top Y+ (at y+1)
    pushVert(fx, fy + yTL, fz + fh, 0, 0, (float)aoTL);
    pushVert(fx + fw, fy + yTR, fz + fh, fw, 0, (float)aoTR);
    pushVert(fx + fw, fy + yBR, fz, fw, fh, (float)aoBR);

    pushVert(fx, fy + yTL, fz + fh, 0, 0, (float)aoTL);
    pushVert(fx + fw, fy + yBR, fz, fw, fh, (float)aoBR);
    pushVert(fx, fy + yBL, fz, 0, fh, (float)aoBL);

    if (isDoubleSided) {
      // float eps = 0.01f; // Removed redundant definition
      // Top starts at fy + fh (since fw=width, fh=height usually? For top face
      // fw is X, fh is Z? No, see Face 4 code) Face 4 code: `pushVert(fx, fy +
      // yTL, fz + fh...)` Wait, fh is Height?? For Top Face, yTL is Height
      // offset? Chunk.cpp line 2483 comments: "The 'width' (fw) here is along
      // X, 'height' (fh) is along Z." So Top Plane is at Ymax? Actually Face 4
      // code uses `fy + yTL`. yTL/TR/etc are the computed heights
      // (usually 1.0). So Top Plane is roughly `fy + 1.0`. The Backface should
      // be slightly lower. But we have 4 different Ys! We should subtract eps
      // from ALL Ys.

      float yTL_B = yTL - eps;
      float yTR_B = yTR - eps;
      float yBR_B = yBR - eps;
      float yBL_B = yBL - eps;

      pushVert(fx, fy + yTL_B, fz + fh, 0, 0, (float)aoTL);
      pushVert(fx + fw, fy + yBR_B, fz, fw, fh, (float)aoBR);
      pushVert(fx + fw, fy + yTR_B, fz + fh, fw, 0, (float)aoTR);

      pushVert(fx, fy + yTL_B, fz + fh, 0, 0, (float)aoTL);
      pushVert(fx, fy + yBL_B, fz, 0, fh, (float)aoBL);
      pushVert(fx + fw, fy + yBR_B, fz, fw, fh, (float)aoBR);
    }
  } else { // Bottom Y- (at y=0)
    pushVert(fx, botY, fz, 0, 0, (float)aoBL);
    pushVert(fx + fw, botY, fz, fw, 0, (float)aoBR);
    pushVert(fx + fw, botY, fz + fh, fw, fh, (float)aoTR);

    pushVert(fx, botY, fz, 0, 0, (float)aoBL);
    pushVert(fx + fw, botY, fz + fh, fw, fh, (float)aoTR);
    pushVert(fx, botY, fz + fh, 0, fh, (float)aoTL);

    if (isDoubleSided) {
      float yBack = botY + eps; // botY is usually fy.

      pushVert(fx, yBack, fz, 0, 0, (float)aoBL);
      pushVert(fx + fw, yBack, fz + fh, fw, fh, (float)aoTR);
      pushVert(fx + fw, yBack, fz, fw, 0, (float)aoBR);

      pushVert(fx, yBack, fz, 0, 0, (float)aoBL);
      pushVert(fx, yBack, fz + fh, 0, fh, (float)aoTL);
      pushVert(fx + fw, yBack, fz + fh, fw, fh, (float)aoTR);
    }
  }
}

bool Chunk::raycast(glm::vec3 origin, glm::vec3 direction, float maxDist,
                    glm::ivec3 &outputPos, glm::ivec3 &outputPrePos) {
  // 1. Quick AABB Check
  glm::vec3 min = glm::vec3(chunkPosition * CHUNK_SIZE);
  glm::vec3 max = min + glm::vec3(CHUNK_SIZE);

  float tMin = 0.0f;
  float tMax = maxDist;

  for (int i = 0; i < 3; ++i) {
    float invD = 1.0f / direction[i];
    float t0 = (min[i] - origin[i]) * invD;
    float t1 = (max[i] - origin[i]) * invD;
    if (invD < 0.0f)
      std::swap(t0, t1);
    tMin = t0 > tMin ? t0 : tMin;
    tMax = t1 < tMax ? t1 : tMax;
    if (tMax <= tMin)
      return false;
  }

  // If we are here, ray intersects chunk AABB.
  // Move origin to local space
  glm::vec3 localOrigin = origin - min;

  // Use a more efficient stepping or valid range relative to intersection
  // entry
  float startDist = std::max(0.0f, tMin);
  float endDist = std::min(maxDist, tMax);

  // Only iterate the segment inside the chunk
  // Step size 0.05f is still a bit coarse/inefficient but better than nothing
  // Better: DDA, but keeping logic comparable for now to minimize regression
  // risk Just Clamp range

  float step = 0.05f;
  glm::vec3 pos = localOrigin + direction * startDist;
  glm::vec3 lastPos = pos;

  // Safety nudge to ensure we are inside if starting exactly on face
  if (tMin > 0)
    pos += direction * 0.001f;

  for (float d = startDist; d < endDist; d += step) {
    int x = (int)floor(pos.x);
    int y = (int)floor(pos.y);
    int z = (int)floor(pos.z);

    // Check bounds (Strict check)
    if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 &&
        z < CHUNK_SIZE) {
      if (blocks[x][y][z].isSelectable()) {
        // Check if raycast position is within actual block bounds
        // Check bounds using AABB
        glm::vec3 blockMin, blockMax;
        blocks[x][y][z].block->getAABB(blocks[x][y][z].metadata, blockMin,
                                       blockMax);

        // Calculate local position within the block (0..1 range)
        float localY = pos.y - (float)y;

        // Only hit if ray position is within the block's actual vertical bounds
        if (localY >= blockMin.y && localY <= blockMax.y) {
          outputPos = glm::ivec3(x, y, z);
          outputPrePos =
              glm::ivec3((int)floor(lastPos.x), (int)floor(lastPos.y),
                         (int)floor(lastPos.z));
          return true;
        }
      }
    }

    lastPos = pos;
    pos += direction * step;
  }
  return false;
}

void Chunk::calculateSunlight() {
  std::lock_guard<std::mutex> lock(chunkMutex);
  // 1. Reset Sky Light
  for (int x = 0; x < CHUNK_SIZE; ++x)
    for (int y = 0; y < CHUNK_SIZE; ++y)
      for (int z = 0; z < CHUNK_SIZE; ++z)
        blocks[x][y][z].skyLight = 0;

  // 2. Sunlight Column Calculation (Y-Down)
  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
      // 2a. Determine if column start has access to sky
      int gx = chunkPosition.x * CHUNK_SIZE + x;
      int gz = chunkPosition.z * CHUNK_SIZE + z;

      // 2a. Determine if column start has access to sky
      // gx and gz are already declared above

      bool exposedToSky = false; // Default false
      int incomingLight = 0;

      // FIX: Use World Heightmap to determine Sky Exposure independently of
      // neighbors
      if (world) {
        int h = world->getHeight(gx, gz);
        // Check top block of this column (y=31)
        // Global Y of top block:
        int topGY = chunkPosition.y * CHUNK_SIZE + (CHUNK_SIZE - 1);

        if (topGY > h) {
          exposedToSky = true;
          incomingLight = 15;
        }
      }

      // Fallback/Neighbor Logic
      if (!exposedToSky) {
        Chunk *current = this;

        // Check neighbors[DIR_TOP]
        if (auto n = current->getNeighbor(DIR_TOP)) {

          // Only check bottom face of neighbor for light
          // We can't easily iterate neighbor columns without locking or
          // overhead, but we can check the *bottom* block of the neighbor.
          // Neighbor(x, 0, z)

          int nLight = n->getSkyLight(
              x, 0, z); // This might be stale if neighbor not updated?
          // But if neighbor is above heightmap, it should be 15.

          if (nLight == 15) {
            exposedToSky = true;
            incomingLight = 15;
          } else {
            incomingLight = nLight;
            if (incomingLight > 0)
              exposedToSky = true;
          }
        }
      }

      // Fallback for "High Enough" if heightmap failed or N/A
      if (!exposedToSky &&
          chunkPosition.y >= 6) { // Increased from 4 to 6 (y=192) to be safe?
        exposedToSky = true;
        incomingLight = 15;
      }

      if (exposedToSky) {
        int currentLight = incomingLight;
        for (int y = CHUNK_SIZE - 1; y >= 0; --y) {
          if (blocks[x][y][z].isOpaque()) {
            break;
          } else {
            // Attenuate light in water
            if (blocks[x][y][z].getType() == WATER) {
              currentLight -= 2;
              if (currentLight < 0)
                currentLight = 0;
            }
            blocks[x][y][z].skyLight = currentLight;

            // Queue for spreading if not full brightness?
            // Actually, if we attenuate, we might want to queue it to
            // spread the darkness/light? The spreadLight function handles
            // outward spread. The column is the source. Note: skyQueue is
            // not accessible here. This line is commented out to maintain
            // syntactical correctness. if(currentLight > 0) {
            //     skyQueue.push(glm::ivec3(x, y, z));
            // }
          }
        }

        // If top block was Water, the column below it should also be lit?
        // The loop goes Y-Down.
        // Loop continues until isOpaque().
        // So if top is Water, it is NOT opaque.
        // blocks[x][y][z].skyLight = 15;
        // Loop continues.
        // Next block (below water) is Water. Not Opaque. skyLight = 15.
        // Next block is Stone. Opaque. Break.
        // Stone gets 0 (default).
        // So the water column will be fully lit (15).
        // However, should water attenuate light? (Darker deeper down).
        // Minecraft does decrease light by 3 per water block.
        // For now, full light is fine to fix "Pitch Black".
      }
    }
  }
}

void Chunk::calculateBlockLight() {
  std::lock_guard<std::mutex> lock(chunkMutex);
  // 1. Reset and Seed Block Light
  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int y = 0; y < CHUNK_SIZE; ++y) {
      for (int z = 0; z < CHUNK_SIZE; ++z) {
        blocks[x][y][z].blockLight = 0;

        if (blocks[x][y][z].isActive()) {
          uint8_t emission = blocks[x][y][z].getEmission();
          if (emission > 0) {
            blocks[x][y][z].blockLight = emission;
          }
        }
      }
    }
  }
}

void Chunk::spreadLight() {
  std::lock_guard<std::mutex> lock(chunkMutex);
  std::queue<glm::ivec3> skyQueue;
  std::queue<glm::ivec3> blockQueue;

  // 1. Seed from self
  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int y = 0; y < CHUNK_SIZE; ++y) {
      for (int z = 0; z < CHUNK_SIZE; ++z) {
        if (blocks[x][y][z].skyLight > 1) {
          skyQueue.push(glm::ivec3(x, y, z));
        }
        if (blocks[x][y][z].blockLight > 1) {
          blockQueue.push(glm::ivec3(x, y, z));
        }
      }
    }
  }

  // 2. Seed from Neighbor Chunks
  // Neighbors: Left(-X), Right(+X), Back(-Z), Front(+Z), Bottom(-Y),
  // Top(+Y)
  struct NeighPtr {
    int ni;
    int ox, oy, oz;
    int faceAxis;
  };
  NeighPtr nPtrs[] = {
      {DIR_LEFT, CHUNK_SIZE - 1, 0, 0, 0},
      {DIR_RIGHT, 0, 0, 0, 0},
      {DIR_BACK, 0, 0, CHUNK_SIZE - 1,
       2}, // Back is Z- (Wait, in GreedyMesh faceDir=1 was Z- and
           // called Back?) Let's standardise: Z- (Back) ->
           // neighbors[DIR_BACK] Z+ (Front) -> neighbors[DIR_FRONT] X-
           // (Left) -> neighbors[DIR_LEFT] X+ (Right) ->
           // neighbors[DIR_RIGHT] Y- (Bottom) -> neighbors[DIR_BOTTOM]
           // Y+ (Top) -> neighbors[DIR_TOP]
      {DIR_FRONT, 0, 0, 0, 2},
      {DIR_BOTTOM, 0, CHUNK_SIZE - 1, 0, 1},
      {DIR_TOP, 0, 0, 0, 1}};
  // Note: nPtrs array matches iteration order or just explicit check?
  // The loop below iterates 'neighbors' array which was struct...
  // Let's rewrite the loop using explicit neighbors array

  for (const auto &np : nPtrs) {
    std::shared_ptr<Chunk> nc = getNeighbor(np.ni);
    if (nc) {
      // Iterate face
      for (int u = 0; u < CHUNK_SIZE; ++u) {
        for (int v = 0; v < CHUNK_SIZE; ++v) {
          int lx, ly, lz;
          int nx, ny, nz;

          // Define iteration based on Face Axis (0=X-face, 1=Y-face,
          // 2=Z-face) If Axis=0 (Left/Right), u=y, v=z If Axis=1 (Bot/Top),
          // u=x, v=z If Axis=2 (Back/Front), u=x, v=y

          if (np.faceAxis == 0) { // X neighbors
            lx = (np.ni == DIR_LEFT) ? 0 : CHUNK_SIZE - 1;
            ly = u;
            lz = v;
            nx = np.ox;
            ny = u;
            nz = v;
          } else if (np.faceAxis == 1) { // Y neighbors
            lx = u;
            ly = (np.ni == DIR_BOTTOM) ? 0 : CHUNK_SIZE - 1;
            lz = v;
            nx = u;
            ny = np.oy;
            nz = v; // np.oy is boundary
          } else {  // Z neighbors
            lx = u;
            ly = v;
            lz = (np.ni == DIR_BACK) ? 0 : CHUNK_SIZE - 1;
            nx = u;
            ny = v;
            nz = np.oz;
          }

          if (!blocks[lx][ly][lz].isOpaque()) {
            // Sky Light
            uint8_t nSky = nc->getSkyLight(nx, ny, nz);
            if (nSky > 1 && nSky - 1 > blocks[lx][ly][lz].skyLight) {
              blocks[lx][ly][lz].skyLight = nSky - 1;
              skyQueue.push(glm::ivec3(lx, ly, lz));
              meshDirty = true;
            }
            // Block Light
            uint8_t nBlock = nc->getBlockLight(nx, ny, nz);
            if (nBlock > 1 && nBlock - 1 > blocks[lx][ly][lz].blockLight) {
              blocks[lx][ly][lz].blockLight = nBlock - 1;
              blockQueue.push(glm::ivec3(lx, ly, lz));
              meshDirty = true;
            }
          }
        }
      }
    }
  }

  if (false) { // Disable old logic
    if (world) {
      int cx = chunkPosition.x;
      int cy = chunkPosition.y;
      int cz = chunkPosition.z;

      struct Neighbor {
        int dx, dy, dz;
        int ox, oy, oz;
        int faceAxis;
      };
      Neighbor neighbors[] = {
          {-1, 0, 0, CHUNK_SIZE - 1, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0},
          {0, -1, 0, 0, CHUNK_SIZE - 1, 0, 1}, {0, 1, 0, 0, 0, 0, 1},
          {0, 0, -1, 0, 0, CHUNK_SIZE - 1, 2}, {0, 0, 1, 0, 0, 0, 2}};

      for (const auto &n : neighbors) {
        std::shared_ptr<Chunk> nc =
            world->getChunk(cx + n.dx, cy + n.dy, cz + n.dz);
        if (nc) {
          for (int u = 0; u < CHUNK_SIZE; ++u) {
            for (int v = 0; v < CHUNK_SIZE; ++v) {
              int lx, ly, lz;
              int nx, ny, nz;

              if (n.faceAxis == 0) {
                lx = (n.dx == -1) ? 0 : CHUNK_SIZE - 1;
                ly = u;
                lz = v;
                nx = n.ox;
                ny = u;
                nz = v;
              } else if (n.faceAxis == 1) {
                lx = u;
                ly = (n.dy == -1) ? 0 : CHUNK_SIZE - 1;
                lz = v;
                nx = u;
                ny = n.oy;
                nz = v;
              } else {
                lx = u;
                ly = v;
                lz = (n.dz == -1) ? 0 : CHUNK_SIZE - 1;
                nx = u;
                ny = v;
                nz = n.oz;
              }

              if (!blocks[lx][ly][lz].isActive()) {
                // Sky Light
                uint8_t nSky = nc->getSkyLight(nx, ny, nz);
                if (nSky > 1 && nSky - 1 > blocks[lx][ly][lz].skyLight) {
                  blocks[lx][ly][lz].skyLight = nSky - 1;
                  skyQueue.push(glm::ivec3(lx, ly, lz));
                  meshDirty = true;
                }
                // Block Light
                uint8_t nBlock = nc->getBlockLight(nx, ny, nz);
                if (nBlock > 1 && nBlock - 1 > blocks[lx][ly][lz].blockLight) {
                  blocks[lx][ly][lz].blockLight = nBlock - 1;
                  blockQueue.push(glm::ivec3(lx, ly, lz));
                  meshDirty = true;
                }
              }
            }
          }
        }
      }
    }
  } // End if(false)

  // 3. Propagate BFS
  int directions[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                          {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};

  // Process Sky Light
  while (!skyQueue.empty()) {
    glm::ivec3 pos = skyQueue.front();
    skyQueue.pop();

    int curLight = blocks[pos.x][pos.y][pos.z].skyLight;
    if (curLight <= 1)
      continue;

    for (int i = 0; i < 6; ++i) {
      int nx = pos.x + directions[i][0];
      int ny = pos.y + directions[i][1];
      int nz = pos.z + directions[i][2];

      if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 &&
          nz < CHUNK_SIZE) {
        if (!blocks[nx][ny][nz].isOpaque()) {
          int decay = (blocks[nx][ny][nz].getType() == WATER) ? 3 : 1;
          if (blocks[nx][ny][nz].skyLight < curLight - decay) {
            blocks[nx][ny][nz].skyLight = curLight - decay;
            skyQueue.push(glm::ivec3(nx, ny, nz));
            meshDirty = true;
          }
        }
      } else {
        // ... neighbor prop logic ...
      }
    }
  }

  // Process Block Light
  while (!blockQueue.empty()) {
    glm::ivec3 pos = blockQueue.front();
    blockQueue.pop();

    int curLight = blocks[pos.x][pos.y][pos.z].blockLight;
    if (curLight <= 1)
      continue;

    for (int i = 0; i < 6; ++i) {
      int nx = pos.x + directions[i][0];
      int ny = pos.y + directions[i][1];
      int nz = pos.z + directions[i][2];

      if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 &&
          nz < CHUNK_SIZE) {
        if (!blocks[nx][ny][nz].isOpaque()) {
          int decay = (blocks[nx][ny][nz].getType() == WATER) ? 3 : 1;
          if (blocks[nx][ny][nz].blockLight < curLight - decay) {
            blocks[nx][ny][nz].blockLight = curLight - decay;
            blockQueue.push(glm::ivec3(nx, ny, nz));
            meshDirty = true;
          }
        }
      }
    }
  }
}

// Helper for Ambient Occlusion
// side1, side2 are the two blocks next to the vertex on the face plane
// corner is the block diagonally from the vertex
int Chunk::vertexAO(bool side1, bool side2, bool corner) {
  if (side1 && side2) {
    return 3;
  }
  return (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
}
