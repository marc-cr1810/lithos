#ifndef CHUNK_COLUMN_H
#define CHUNK_COLUMN_H

#include "Chunk.h" // For CHUNK_SIZE

struct ChunkColumn {
  int heightMap[CHUNK_SIZE][CHUNK_SIZE];
  // Biome map removed - we use raw noise values
  float temperatureMap[CHUNK_SIZE][CHUNK_SIZE];
  float humidityMap[CHUNK_SIZE][CHUNK_SIZE];
  float beachNoiseMap[CHUNK_SIZE][CHUNK_SIZE];

  // We can store these for decorators to use
  float forestNoiseMap[CHUNK_SIZE][CHUNK_SIZE];
  float bushNoiseMap[CHUNK_SIZE][CHUNK_SIZE];

  // Helper
  int getHeight(int localX, int localZ) const {
    if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 ||
        localZ >= CHUNK_SIZE)
      return 0;
    return heightMap[localX][localZ];
  }

  // Setters for WorldGenerator to populate
  void setHeight(int x, int z, int h) {
    if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
      heightMap[x][z] = h;
    }
  }
};

#endif
