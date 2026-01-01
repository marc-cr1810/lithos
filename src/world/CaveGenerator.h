#ifndef CAVEGENERATOR_H
#define CAVEGENERATOR_H

#include "WorldGenConfig.h"
#include <FastNoise/FastNoise.h>
#include <glm/glm.hpp>

class Chunk;
class WorldGenerator; // If needed, but trying to minimize

class CaveGenerator {
public:
  CaveGenerator(const WorldGenConfig &config);
  ~CaveGenerator() = default;

  // Main API
  bool IsCaveAt(int x, int y, int z, int maxDepth);
  void GenerateWormCave(Chunk &chunk, int startX, int startY, int startZ,
                        int maxDepth);

  // Helpers
  void CarveSphere(Chunk &chunk, glm::vec3 center, float radius);

private:
  int seed;
  float caveFrequency;
  float caveThreshold;
  const WorldGenConfig &config;

  // Internal Noise Nodes
  FastNoise::SmartNode<> fn3D; // For cheese/spaghetti
  FastNoise::SmartNode<> fn2D; // For entrance/ravine

  // Helper to init noise
  void InitNoise();
};

#endif
