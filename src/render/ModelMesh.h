#pragma once

#include "ModelMeshData.h"
#include <glm/glm.hpp>

namespace Lithos {

/**
 * Cached mesh data for a model (VS-inspired).
 * Generated once from a Model, then reused for all instances.
 *
 * Now uses ModelMeshData for structured geometry storage.
 */
struct ModelMesh {
  ModelMeshData geometry; // Pure geometry (positions, UVs, normals)

  /**
   * Get vertex count.
   */
  size_t getVertexCount() const { return geometry.getVertexCount(); }

  /**
   * Get approximate memory usage in bytes.
   */
  size_t getMemoryUsage() const { return geometry.getMemoryUsage(); }
};

} // namespace Lithos
