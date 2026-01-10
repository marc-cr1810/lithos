#pragma once

#include "../world/Block.h"
#include "ModelMeshCache.h"
#include "ModelMeshData.h"


namespace Lithos {

/**
 * Tessellator converts Blocks (and their Models) to ModelMeshData.
 * Uses ModelMeshCache to avoid regenerating the same block mesh.
 *
 * Keyed by Block* to ensure correct Texture Atlas UVs are baked in.
 */
class Tessellator {
public:
  Tessellator(ModelMeshCache &cache);

  /**
   * Get geometry for a block (cached or generate).
   * Returns geometry with UVs baked to Texture Atlas coordinates.
   */
  const ModelMeshData *getMesh(const Block *block);

  /**
   * Generate geometry from block (called by cache).
   * Converts model elements to mesh, resolving UVs using the Block.
   */
  static ModelMeshData tessellateBlock(const Block *block);

private:
  ModelMeshCache &meshCache;
};

} // namespace Lithos
