#include "ModelMeshCache.h"
#include "../world/Block.h"
#include "Tessellator.h"
#include <algorithm>

namespace Lithos {

const ModelMeshData *ModelMeshCache::getOrCreate(const Block *block) {
  if (!block)
    return nullptr;

  // Try to find in cache first (read lock)
  {
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    auto it = cache.find(block);
    if (it != cache.end()) {
      return &it->second;
    }
  }

  // Not in cache - generate geometry
  ModelMeshData geom = generateMesh(block);

  // Store in cache (write lock)
  {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    // Double-check it wasn't added by another thread
    auto [it, inserted] = cache.try_emplace(block, std::move(geom));
    return &it->second;
  }
}

void ModelMeshCache::clear() {
  std::unique_lock<std::shared_mutex> lock(cacheMutex);
  cache.clear();
}

size_t ModelMeshCache::getTotalMemoryUsage() const {
  std::shared_lock<std::shared_mutex> lock(cacheMutex);
  size_t total = 0;
  for (const auto &[key, geom] : cache) {
    total += geom.getMemoryUsage();
  }
  return total;
}

ModelMeshData ModelMeshCache::generateMesh(const Block *block) {
  // Use Tessellator to convert Block Model -> Geometry
  return Tessellator::tessellateBlock(block);
}

} // namespace Lithos
