#include "WorldGenRegion.h"
#include "Chunk.h"

WorldGenRegion::WorldGenRegion(World *world, int cx, int cz)
    : world(world), centerX(cx), centerZ(cz) {
  // Initialize all pointers to nullptr
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      columns[i][j] = nullptr;
    }
  }

  // Fetch 3x3 grid of columns from world (if available)
  if (world) {
    std::lock_guard<std::mutex> lock(world->columnMutex);
    for (int dx = -1; dx <= 1; dx++) {
      for (int dz = -1; dz <= 1; dz++) {
        int colX = cx + dx;
        int colZ = cz + dz;

        auto it = world->columns.find({colX, colZ});
        if (it != world->columns.end()) {
          columns[dx + 1][dz + 1] = it->second.get();
        }
      }
    }
  }
  // If world is nullptr (benchmark mode), columns remain nullptr
}

const ChunkColumn *WorldGenRegion::getColumn(int dx, int dz) const {
  if (dx < -1 || dx > 1 || dz < -1 || dz > 1) {
    return nullptr;
  }
  return columns[dx + 1][dz + 1];
}

BlockType WorldGenRegion::getBlock(int x, int y, int z) const {
  // Null-safety for benchmark mode
  if (!world) {
    return AIR;
  }

  // Use floor division for chunk coordinates to handle negative values
  // correctly
  int colX = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int colZ = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int chunkY =
      (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);

  int dx = colX - centerX;
  int dz = colZ - centerZ;

  // Out of bounds check
  if (dx < -1 || dx > 1 || dz < -1 || dz > 1) {
    return AIR; // Outside region
  }

  ChunkColumn *col = columns[dx + 1][dz + 1];
  if (!col) {
    return AIR; // Column not loaded
  }

  std::shared_ptr<Chunk> chunk = world->getChunk(colX, chunkY, colZ);
  if (!chunk) {
    return AIR; // Chunk not loaded
  }

  // Convert to local coordinates
  int lx = x - colX * CHUNK_SIZE;
  int ly = y - chunkY * CHUNK_SIZE;
  int lz = z - colZ * CHUNK_SIZE;

  // Bounds check (Chunk's getBlock might have its own checks but being safe
  // here)
  if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE || lz < 0 ||
      lz >= CHUNK_SIZE) {
    return AIR;
  }

  return (BlockType)chunk->getBlock(lx, ly, lz).getType();
}

void WorldGenRegion::setBlock(int x, int y, int z, BlockType type) {
  // Null-safety for benchmark mode
  if (!world) {
    return; // Can't set blocks without World
  }

  // Use floor division for chunk coordinates to handle negative values
  // correctly
  int colX = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int colZ = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int chunkY =
      (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);

  int dx = colX - centerX;
  int dz = colZ - centerZ;

  // Out of bounds check
  if (dx < -1 || dx > 1 || dz < -1 || dz > 1) {
    return; // Outside region, ignore
  }

  ChunkColumn *col = columns[dx + 1][dz + 1];
  if (!col) {
    return; // Column not loaded
  }

  std::shared_ptr<Chunk> chunk = world->getChunk(colX, chunkY, colZ);
  if (!chunk) {
    return; // Chunk not loaded
  }

  // Convert to local coordinates
  int lx = x - colX * CHUNK_SIZE;
  int ly = y - chunkY * CHUNK_SIZE;
  int lz = z - colZ * CHUNK_SIZE;

  // Bounds check
  if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE || lz < 0 ||
      lz >= CHUNK_SIZE) {
    return;
  }

  chunk->setBlock(lx, ly, lz, type);
}
