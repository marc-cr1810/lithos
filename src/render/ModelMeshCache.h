#pragma once

#include "Model.h"
#include "ModelMesh.h"
#include "ModelMeshData.h"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

// Forward decl
class Block;

namespace Lithos {

/**
 * Thread-safe cache for model geometry (VS-inspired).
 * Stores pure geometry (ModelMeshData) separate from lighting.
 *
 * Keyed by Block* to ensure baked UVs are correct.
 */
class ModelMeshCache {
public:
  ModelMeshCache() = default;
  ~ModelMeshCache() = default;

  // Non-copyable
  ModelMeshCache(const ModelMeshCache &) = delete;
  ModelMeshCache &operator=(const ModelMeshCache &) = delete;

  /**
   * Get cached geometry for a block, or generate if not cached.
   * Returns geometry with baked UVs.
   * Thread-safe.
   */
  const ModelMeshData *getOrCreate(const Block *block);

  /**
   * Clear all cached meshes.
   */
  void clear();

  /**
   * Get cache statistics.
   */
  size_t getCachedCount() const { return cache.size(); }
  size_t getTotalMemoryUsage() const;

private:
  std::unordered_map<const Block *, ModelMeshData> cache;
  mutable std::shared_mutex cacheMutex;

  /**
   * Generate geometry from block (called when not in cache).
   */
  ModelMeshData generateMesh(const Block *block);
};

} // namespace Lithos
