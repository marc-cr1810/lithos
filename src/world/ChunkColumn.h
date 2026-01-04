#ifndef CHUNK_COLUMN_H
#define CHUNK_COLUMN_H

#include "Chunk.h" // For CHUNK_SIZE
#include <cstring>

struct ChunkColumn {
  int heightMap[CHUNK_SIZE][CHUNK_SIZE];
  // Biome map removed - we use raw noise values
  float temperatureMap[CHUNK_SIZE][CHUNK_SIZE];
  float humidityMap[CHUNK_SIZE][CHUNK_SIZE];
  float beachNoiseMap[CHUNK_SIZE][CHUNK_SIZE];

  // We can store these for decorators to use
  float forestNoiseMap[CHUNK_SIZE][CHUNK_SIZE];
  float bushNoiseMap[CHUNK_SIZE][CHUNK_SIZE];

  // Cave height distortion for irregular cave ceilings
  uint8_t caveHeightDistort[CHUNK_SIZE * CHUNK_SIZE];

  // Helper
  int getHeight(int localX, int localZ) const {
    if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 ||
        localZ >= CHUNK_SIZE)
      return 0;
    return heightMap[localX][localZ];
  }

  bool generated = false;
  bool decorated = false;

  ChunkColumn() {
    // Initialize heightMap to 0
    // CHUNK_SIZE * CHUNK_SIZE is 256 for typical CHUNK_SIZE=16
    // Using memset for efficiency to zero out the entire 2D array
    ::memset(heightMap, 0, sizeof(heightMap));
    // Other maps could also be initialized if needed, e.g.,
    // std::memset(temperatureMap, 0, sizeof(temperatureMap));
    // std::memset(humidityMap, 0, sizeof(humidityMap));
    // ...
  }

  // Setters for WorldGenerator to populate
  void setHeight(int x, int z, int h) {
    if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
      heightMap[x][z] = h;
    }
  }
};

#endif
