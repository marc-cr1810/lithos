#ifndef CHUNK_COLUMN_H
#define CHUNK_COLUMN_H

#include "Chunk.h"          // For CHUNK_SIZE
#include "WorldGenerator.h" // For Biome enum

struct ChunkColumn {
  int heightMap[CHUNK_SIZE][CHUNK_SIZE];
  Biome biomeMap[CHUNK_SIZE][CHUNK_SIZE];
  float temperatureMap[CHUNK_SIZE][CHUNK_SIZE];
  float humidityMap[CHUNK_SIZE][CHUNK_SIZE];

  // Accessors if we want them, or direct access since it's a struct
  int getHeight(int localX, int localZ) const {
    return heightMap[localX][localZ];
  }

  Biome getBiome(int localX, int localZ) const {
    return biomeMap[localX][localZ];
  }
};

#endif
