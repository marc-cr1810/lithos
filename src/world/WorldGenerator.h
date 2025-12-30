#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H

#include "Block.h"
#include "WorldGenConfig.h"
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

class Chunk;
class WorldDecorator;

enum Biome {
  BIOME_OCEAN,
  BIOME_BEACH,
  BIOME_DESERT,
  BIOME_TUNDRA,
  BIOME_FOREST,
  BIOME_PLAINS
};

struct ChunkColumn;

// Landform configuration for octave-based terrain
struct LandformConfig {
  std::vector<float> octaveAmplitudes; // 8 values, 0.0-1.0
  std::vector<float> octaveThresholds; // 8 values, 0.0-1.0
  float baseHeight;                    // Base elevation (e.g., 64)
  float heightVariation;               // Max height variation (e.g., 20)
  std::string name;
};

// Cave generation system
class CaveGenerator {
public:
  CaveGenerator(const WorldGenConfig &config);

  // Check if position should be a cave
  bool IsCaveAt(int x, int y, int z, int maxDepth);

  // Check if position should be a ravine
  bool IsRavineAt(int x, int y, int z, int surfaceHeight);

  // Generate worm-style cave tunnel
  void GenerateWormCave(Chunk &chunk, int startX, int startY, int startZ,
                        int maxDepth);

private:
  void CarveSphere(Chunk &chunk, glm::vec3 center, float radius);

  int seed;
  float caveFrequency;
  float caveThreshold;
};

class WorldGenerator {
public:
  WorldGenerator(const WorldGenConfig &config);
  ~WorldGenerator();
  void GenerateColumn(ChunkColumn &column, int cx, int cz);
  void GenerateChunk(Chunk &chunk, const ChunkColumn &column);
  int GetHeight(int x, int z); // Converted to Instance Method
  float GetTemperature(int x, int z);
  float GetHumidity(int x, int z);
  Biome GetBiome(int x, int z);
  BlockType GetSurfaceBlock(int gx, int gy, int gz, bool checkCarving = false);
  int GetSeed() const { return seed; }

private:
  BlockType GetStrataBlock(int x, int y, int z);

  // Noise map methods
  float GetLandformNoise(int x, int z);
  float GetClimateNoise(int x, int z);
  float GetGeologicNoise(int x, int z);
  std::string GetLandformType(int x, int z);

  // Landform initialization
  void InitializeLandforms();

  LandformConfig *GetLandform(const std::string &name);

  // Members
  std::vector<WorldDecorator *> decorators;
  std::map<std::string, LandformConfig> landforms;
  CaveGenerator *caveGenerator;
  WorldGenConfig config;
  int seed;
};

#endif
