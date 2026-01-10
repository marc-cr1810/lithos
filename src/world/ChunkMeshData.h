#pragma once

#include <glm/glm.hpp>
#include <vector>


namespace Lithos {

// Forward declaration
class Chunk;

/**
 * Request to generate mesh for a chunk.
 * Sent from main thread to worker thread.
 */
struct ChunkMeshRequest {
  glm::ivec3 chunkPos;
  Chunk *chunk = nullptr;
};

/**
 * Result of chunk mesh generation.
 * Sent from worker thread to main thread.
 */
struct ChunkMeshResult {
  glm::ivec3 chunkPos;

  // Opaque mesh data
  std::vector<float> opaqueVertices;

  // Transparent mesh data
  std::vector<float> transparentVertices;

  // Was generation successful?
  bool success = false;
};

} // namespace Lithos
