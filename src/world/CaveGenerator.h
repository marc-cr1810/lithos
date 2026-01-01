#ifndef CAVEGENERATOR_H
#define CAVEGENERATOR_H

#include "WorldGenConfig.h"
#include <FastNoise/FastNoise.h>
#include <glm/glm.hpp>

class Chunk;
class NoiseManager;
struct ChunkColumn;   // Forward declaration
class WorldGenerator; // If needed, but trying to minimize

class CaveGenerator {
public:
  CaveGenerator(const WorldGenConfig &config);
  ~CaveGenerator() = default;

  // Main API
  // bool IsCaveAt(int x, int y, int z, int maxDepth); // Deprecated/Internal
  // Optimized Batch Generation
  void GenerateCaves(Chunk &chunk, const ChunkColumn &column,
                     const NoiseManager &noiseManager);

  void GenerateWormCave(Chunk &chunk, int startX, int startY, int startZ,
                        int maxDepth);

  // Helpers
  void CarveSphere(Chunk &chunk, glm::vec3 center, float radius);

private:
  int seed;
  float caveFrequency;
  float caveThreshold;
  const WorldGenConfig &config;

  // Internal Noise Nodes - Removed in favor of NoiseManager
  // FastNoise::SmartNode<> fn3D;
  // FastNoise::SmartNode<> fn2D;

  // Helper to init noise - Removed
  // void InitNoise();
};

#endif
