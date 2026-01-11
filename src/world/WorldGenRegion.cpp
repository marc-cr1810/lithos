#include "WorldGenRegion.h"
#include "Chunk.h"
#include <iostream>

// Cache air block for default returns
static Block *airBlock = nullptr;
static void initAirBlock() {
  if (!airBlock) {
    airBlock = BlockRegistry::getInstance().getBlock("lithos:air");
  }
}

WorldGenRegion::WorldGenRegion(World *world, int cx, int cz)
    : world(world), centerX(cx), centerZ(cz) {
  initAirBlock();
  // Initialize all pointers to nullptr
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      columns[i][j] = nullptr;
    }
  }

  // Fetch 3x3 grid of columns from world (if available)
  if (world) {
    // Pin chunks to prevent unloading during decoration
    pinnedChunks = world->PinChunksInRegion(cx, cz);

    // Clear array cache
    std::memset(chunkArray, 0, sizeof(chunkArray));

    // Populate cache with pinned chunks for faster access
    for (const auto &chunk : pinnedChunks) {
      if (!chunk)
        continue;

      int dx = chunk->chunkPosition.x - cx;
      int chunkY = chunk->chunkPosition.y;
      int dz = chunk->chunkPosition.z - cz;

      if (dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1 && chunkY >= 0 &&
          chunkY < 16) {
        chunkArray[dx + 1][chunkY][dz + 1] = chunk.get();
      }
    }

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

WorldGenRegion::~WorldGenRegion() {
  if (!world)
    return;

  // Batch update modified chunks
  for (Chunk *chunk : modifiedChunks) {
    if (chunk) {
      chunk->meshDirty = true;
      chunk->needsLightingUpdate = true;
    }
  }

  // Unpin chunks
  for (auto &chunk : pinnedChunks) {
    chunk->pinCount--;
  }
}

const ChunkColumn *WorldGenRegion::getColumn(int dx, int dz) const {
  if (dx < -1 || dx > 1 || dz < -1 || dz > 1) {
    return nullptr;
  }
  return columns[dx + 1][dz + 1];
}

block_id WorldGenRegion::getBlock(int x, int y, int z) const {
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

  Chunk *chunk = nullptr;
  if (chunkY >= 0 && chunkY < 16) {
    chunk = chunkArray[dx + 1][chunkY][dz + 1];
  }

  if (!chunk) {
    // Note: getChunk might lock, so caching is valuable
    std::shared_ptr<Chunk> ptr = world->getChunk(colX, chunkY, colZ);
    if (ptr) {
      if (chunkY >= 0 && chunkY < 16) {
        const_cast<WorldGenRegion *>(this)->chunkArray[dx + 1][chunkY][dz + 1] =
            ptr.get();
      }
      chunk = ptr.get();
    }
  }

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

  return chunk->getBlock(lx, ly, lz).getType();
}

Block *WorldGenRegion::getBlockPtr(int x, int y, int z) const {
  // Same logic as getBlock but returns the pointer directly
  // Null-safety for benchmark mode
  if (!world) {
    return airBlock;
  }

  int colX = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int colZ = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int chunkY =
      (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);

  int dx = colX - centerX;
  int dz = colZ - centerZ;

  if (dx < -1 || dx > 1 || dz < -1 || dz > 1)
    return airBlock;

  ChunkColumn *col = columns[dx + 1][dz + 1];
  if (!col)
    return airBlock;

  Chunk *chunk = nullptr;
  if (chunkY >= 0 && chunkY < 16) {
    chunk = chunkArray[dx + 1][chunkY][dz + 1];
  }

  if (!chunk) {
    std::shared_ptr<Chunk> ptr = world->getChunk(colX, chunkY, colZ);
    if (ptr) {
      if (chunkY >= 0 && chunkY < 16) {
        const_cast<WorldGenRegion *>(this)->chunkArray[dx + 1][chunkY][dz + 1] =
            ptr.get();
      }
      chunk = ptr.get();
    }
  }

  if (!chunk)
    return airBlock;

  int lx = x - colX * CHUNK_SIZE;
  int ly = y - chunkY * CHUNK_SIZE;
  int lz = z - colZ * CHUNK_SIZE;

  if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE || lz < 0 ||
      lz >= CHUNK_SIZE) {
    return airBlock;
  }

  return chunk->getBlock(lx, ly, lz).getBlock();
}

void WorldGenRegion::setBlock(int x, int y, int z, block_id type) {
  setBlock(x, y, z, BlockRegistry::getInstance().getBlock(type));
}

void WorldGenRegion::setBlock(int x, int y, int z, Block *block) {
  // Null-safety for benchmark mode
  if (!world) {
    return;
  }

  int colX = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int colZ = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int chunkY =
      (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);

  int dx = colX - centerX;
  int dz = colZ - centerZ;

  if (dx < -1 || dx > 1 || dz < -1 || dz > 1)
    return;

  ChunkColumn *col = columns[dx + 1][dz + 1];
  if (!col)
    return;

  Chunk *chunk = nullptr;
  if (chunkY >= 0 && chunkY < 16) {
    chunk = chunkArray[dx + 1][chunkY][dz + 1];
  } else {
    // Fallback if height > 16 (unlikely with current config)
    auto ptr = world->getChunk(colX, chunkY, colZ);
    chunk = ptr.get();
  }

  if (!chunk)
    return;

  int lx = x - colX * CHUNK_SIZE;
  int ly = y - chunkY * CHUNK_SIZE;
  int lz = z - colZ * CHUNK_SIZE;

  if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE || lz < 0 ||
      lz >= CHUNK_SIZE) {
    return;
  }

  chunk->setBlockNoMeshUpdate(lx, ly, lz, block->getId());
  modifiedChunks.insert(chunk);
}

Chunk *WorldGenRegion::getChunk(int x, int y, int z) const {
  if (!world)
    return nullptr;

  int colX = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int colZ = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
  int chunkY =
      (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);

  int dx = colX - centerX;
  int dz = colZ - centerZ;

  if (dx < -1 || dx > 1 || dz < -1 || dz > 1)
    return nullptr;

  ChunkColumn *col = columns[dx + 1][dz + 1];
  if (!col)
    return nullptr;

  if (chunkY >= 0 && chunkY < 16) {
    Chunk *chunk = chunkArray[dx + 1][chunkY][dz + 1];
    if (chunk)
      return chunk;
  }

  // Fallback slow path
  auto ptr = world->getChunk(colX, chunkY, colZ);
  if (ptr) {
    if (chunkY >= 0 && chunkY < 16) {
      const_cast<WorldGenRegion *>(this)->chunkArray[dx + 1][chunkY][dz + 1] =
          ptr.get();
    }
    return ptr.get();
  }

  return nullptr;
}
