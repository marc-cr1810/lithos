#ifndef WORLDGENREGION_H
#define WORLDGENREGION_H

#include "Block.h"
#include "ChunkColumn.h"
#include "World.h"
#include <mutex>

/**
 * WorldGenRegion provides safe cross-chunk block access for decoration.
 * Architecture: 3x3 grid of ChunkColumns (2D), giving decorators:
 *   - Fast cached access (heightMap, biomeMap, etc.) from columns
 *   - Cross-column block placement for trees/features
 */
class WorldGenRegion {
public:
  /**
   * Constructor: Creates a region centered on column (cx, cz)
   * @param world - World instance to fetch columns from
   * @param cx - Center column X coordinate
   * @param cz - Center column Z coordinate
   */
  WorldGenRegion(World *world, int cx, int cz);

  /**
   * Get the center ChunkColumn (cached data like heightMap, biomeMap, etc.)
   */
  const ChunkColumn &getCenterColumn() const { return *columns[1][1]; }

  /**
   * Get a column by relative offset from center
   * @param dx - X offset (-1, 0, or 1)
   * @param dz - Z offset (-1, 0, or 1)
   */
  const ChunkColumn *getColumn(int dx, int dz) const;

  /**
   * Get block at world coordinates (delegates to appropriate column/chunk)
   */
  BlockType getBlock(int x, int y, int z) const;

  /**
   * Get Block* at world coordinates (delegates to appropriate column/chunk)
   */
  Block *getBlockPtr(int x, int y, int z) const;

  /**
   * Set block at world coordinates (delegates to appropriate column/chunk)
   * Thread-safe via world's chunkMutex
   */
  void setBlock(int x, int y, int z, BlockType type);

  /**
   * Set block at world coordinates using Block pointer
   * Thread-safe via world's chunkMutex
   */
  void setBlock(int x, int y, int z, Block *block);

  /**
   * Get center column X coordinate
   */
  int getCenterX() const { return centerX; }

  /**
   * Get center column Z coordinate
   */
  int getCenterZ() const { return centerZ; }

  /**
   * Get world instance
   */
  World *getWorld() const { return world; }

  /**
   * Get minimum X block coordinate in region
   */
  int getMinX() const { return centerX * 32; }

  /**
   * Get minimum Z block coordinate in region
   */
  int getMinZ() const { return centerZ * 32; }

private:
  World *world;
  int centerX, centerZ; // Column coordinates

  // 3x3 grid of columns: columns[dx+1][dz+1] where dx,dz in {-1,0,1}
  ChunkColumn *columns[3][3];
};

#endif // WORLDGENREGION_H
