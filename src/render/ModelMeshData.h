#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Lithos {

/**
 * Structured mesh data for a model (VS-inspired).
 * Separates geometry (cacheable) from lighting (per-instance).
 *
 * This matches Vintage Story's approach:
 * - Geometry (positions, UVs, normals) cached once per model
 * - Lighting calculated per vertex instance at chunk build time
 */
struct ModelMeshData {
  // Pure geometry data (cacheable)
  std::vector<glm::vec3> positions; // Vertex positions (local to block)
  std::vector<glm::vec2> uvs;       // Texture coordinates
  std::vector<glm::vec3> normals;   // Vertex normals (for lighting)
  std::vector<glm::vec4>
      texOrigins; // Texture atlas rect [uMin, vMin, uWidth, vHeight]
  std::vector<float> overlayFlags; // Overlay/Tint flags
  std::vector<uint32_t> indices;   // Optional indices

  // Face information (for culling and AO)
  struct FaceInfo {
    int faceDirection; // 0-5: which face this vertex belongs to
    bool enableAO;     // Whether AO should be calculated
  };
  std::vector<FaceInfo> faceInfo; // Per-vertex face data

  /**
   * Get vertex count.
   */
  size_t getVertexCount() const { return positions.size(); }

  /**
   * Get memory usage estimate.
   */
  size_t getMemoryUsage() const {
    return positions.size() * sizeof(glm::vec3) +
           uvs.size() * sizeof(glm::vec2) + normals.size() * sizeof(glm::vec3) +
           texOrigins.size() * sizeof(glm::vec4) +
           overlayFlags.size() * sizeof(float) +
           indices.size() * sizeof(uint32_t) +
           faceInfo.size() * sizeof(FaceInfo);
  }

  /**
   * Clear all data.
   */
  void clear() {
    positions.clear();
    uvs.clear();
    normals.clear();
    texOrigins.clear();
    overlayFlags.clear();
    indices.clear();
    faceInfo.clear();
  }
};

} // namespace Lithos
