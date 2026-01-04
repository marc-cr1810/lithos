#ifndef CAVEGENERATOR_H
#define CAVEGENERATOR_H

#include "WorldGenConfig.h"
#include <FastNoise/FastNoise.h>
#include <glm/glm.hpp>
#include <random>

class Chunk;
class NoiseManager;
struct ChunkColumn;
class WorldGenRegion;

// Data-driven configuration for cave generation
struct CaveConfig {
  float cavesPerChunk = 0.75f;
  int chunkRange = 5;

  // Tunnel sizes
  float horizontalMin = 1.0f;
  float horizontalMax = 3.0f;
  float verticalMin = 0.75f;
  float verticalMax = 1.15f;

  // Special cave types
  float wideFlatChance = 0.04f;
  float tallNarrowChance = 0.01f;
  float extraBranchyChance = 0.02f;
  float largeNearLavaChance = 0.3f;

  // Curviness
  float curviness_normal = 0.1f;
  float curviness_high = 0.5f;
  float curviness_veryLow = 0.035f;
  float curviness_highChance = 0.03f;
  float curviness_veryLowChance = 0.01f;

  // Branching
  int horizontalBranchBase = 25;
  int horizontalBranchExtraBranchy = 12;
  int verticalShaftChance = 60;
  int maxBranchDepth = 3;
  int verticalShaftMinY = 60;
  float verticalShaftMinRadius = 3.0f;

  // Sizing
  float baseHorizontal = 1.5f;
  float baseVertical = 1.5f;
  float minHorizontal = 1.0f;
  float minVertical = 0.6f;
  float sizeChangeSpeed = 0.15f;

  // Lava
  int lavaY = 11;
  int largeCavernMinY = 19;
  int largeCavernMaxY = -5;
  float largeCavernMinRadius = 4.0f;
  float largeCavernMinVertRadius = 2.0f;

  // Iteration
  int maxIterationBase = 160;
  float maxIterationVariance = 0.25f;

  // Distortion
  int heightDistortOctaves = 3;
  float heightDistortFrequency = 0.05f;
  float heightDistortStrength = 0.1f;

  // Hot springs
  int hotSprings_minY = -5;
  int hotSprings_maxY = 16;
  float hotSprings_minHorizontalRadius = 4.0f;
  float hotSprings_minVerticalRadius = 2.0f;
  int hotSprings_minGeologicActivity = 128;

  // Angle variation
  float initialVerticalAngleRange = 0.25f;
  float verticalAngleDamping = 0.8f;
  float verticalAngleChangeFactor = 3.0f;
  float horizontalAngleChangeFactor = 1.0f;
  float majorDirectionChangeChance = 0.003f;
  float minorDirectionChangeChance = 0.0076f;

  // Random events
  float goWideChance = 0.006f;
  float goThinChance = 0.006f;
  float goFlatChance = 0.005f;
  float goReallyWideChance = 0.0009f;
  float goReallyTallChance = 0.0009f;
  float largeLavaCavernChance = 0.01f;

  // Load from JSON file
  static CaveConfig LoadFromFile(const std::string &filepath);
};

class CaveGenerator {
public:
  CaveGenerator(const WorldGenConfig &config);
  ~CaveGenerator() = default;

  // Generate cave height distortion map for a chunk column
  void GenerateHeightDistortion(ChunkColumn &column, int cx, int cz);

  // Generate caves in a region (cross-chunk safe)
  void GenerateCaves(WorldGenRegion &region, int chunkX, int chunkZ);

private:
  int seed;
  const WorldGenConfig &worldConfig;
  CaveConfig caveConfig;

  // Height distortion noise
  FastNoise::SmartNode<> heightDistortNoise;

  // RNG for cave generation
  std::mt19937 caveRng;
  std::mt19937 chunkRng;

  // Initialize random number generator for a specific chunk
  void InitChunkRng(int chunkX, int chunkZ);

  // Main tunnel carving algorithm
  void CarveTunnel(WorldGenRegion &region, int chunkX, int chunkZ, double posX,
                   double posY, double posZ, float horAngle, float vertAngle,
                   float horizontalSize, float verticalSize,
                   int currentIteration, int maxIterations, int branchLevel,
                   bool extraBranchy = false, float curviness = 0.1f,
                   bool largeNearLava = false);

  // Vertical shaft carving
  void CarveShaft(WorldGenRegion &region, int chunkX, int chunkZ, double posX,
                  double posY, double posZ, float horAngle, float vertAngle,
                  float horizontalSize, float verticalSize,
                  int caveCurrentIteration, int maxIterations, int branchLevel);

  // Actually remove blocks in an ellipsoid
  bool SetBlocks(WorldGenRegion &region, float horRadius, float vertRadius,
                 double centerX, double centerY, double centerZ, int chunkX,
                 int chunkZ, bool genHotSpring);

  // Helper to get random float in range [min, max]
  float RandomFloat(float min, float max);

  // Helper to get random int
  int RandomInt(int max);
};

#endif
